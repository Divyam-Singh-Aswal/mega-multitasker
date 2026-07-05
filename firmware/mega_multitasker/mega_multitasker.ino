 /*                    
                                          <<<<<<<<    "Mega Multitasker" the badass multitasking electronic device    >>>>>>>       
*/     

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <Fonts/FreeSans9pt7b.h>
//#include <Fonts/Org_01.h>
//#include <Fonts/TomThumb.h>

// DISPLAY  
#define SCREEN_W  240
#define SCREEN_H  320
#define STATUS_H   28
#define BOTTOM_H   45
#define CONTENT_Y  STATUS_H
#define CONTENT_H  (SCREEN_H - STATUS_H - BOTTOM_H)

// TOUCH  
#define TS_MINX  120
#define TS_MAXX  900
#define TS_MINY  70
#define TS_MAXY  920
#define MINPRESSURE  10
#define MAXPRESSURE  1000
#define XP  8
#define XM  A2
#define YP  A3
#define YM  9
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// HARDWARE  
MCUFRIEND_kbv tft;

#define BUZZER_PIN 26 
#define DHTPIN 22
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

HardwareSerial &gpsSerial = Serial1;   // GPS  on pins 18/19
HardwareSerial &espSerial = Serial2;   // ESP32 on pins 16/17
TinyGPSPlus gps;

// COLORS  
#define BLACK  0x0000
#define WHITE  0xFFFF
#define DARKGRAY   0x4208
#define BLUE  0x001F
#define CYAN  0x07FF
#define RED  0xF800
#define ORANGE 0xFD20
#define YELLOW 0xFFE0
#define MAGENTA 0xF81F
#define DARK_NAVY 0x0861
#define DEEP_BLUE  0x0841
#define SLATE_BLUE  0x2965
#define STEEL_GRAY  0xBDD7
#define DARK_ORANGE 0xFC00
#define LIME_GREEN 0x07E0
#define MAUVE  0x867D
#define DARK_TEAL  0x0359
#define MED_GREEN  0x03E0   

// MODES 
enum ScreenMode { HOME, TEMP, GPS, DRAW, CHAT, TICTACTOE,TTT_MODE_SELECT, TTT_SYMBOL_PICK, TTT_ONLINE_REQUEST,
                  NET, NET_WIFI, NET_BT, NET_INTERNET, KEYBOARD,CLOCK };
ScreenMode currentMode = HOME;

// NAVIGATION 
ScreenMode navHistory[10];
uint8_t    navDepth = 0;

// ICONS  
#define ICON_W  45
#define ICON_H  45
#define ICON_R  9

struct AppIcon {
  int16_t  x, y;
  uint16_t bg;
  uint16_t accent;
  const char* label;
  ScreenMode target;
};

AppIcon icons[] = {
  { 7,  CONTENT_Y + 40, 0x0020, DARK_ORANGE , "TEMP", TEMP },
  { 67,  CONTENT_Y + 40, 0x0200, LIME_GREEN,  "GPS",  GPS  },
  { 127, CONTENT_Y + 40,  0x4001, MAGENTA, "DRAW", DRAW },
  { 187, CONTENT_Y + 40, 0x0018, MAUVE, "CHAT", CHAT },
  { 7, CONTENT_Y + 100, 0x0820, YELLOW, "TTT", TICTACTOE },
  { 67, CONTENT_Y + 100, 0x0010, CYAN, "NET", NET },
  { 127, CONTENT_Y + 100, 0x2000, YELLOW,"CLOCK", CLOCK},
};
const uint8_t ICON_COUNT = sizeof(icons) / sizeof(icons[0]);

// CLOCK
uint8_t  fakeHour  = 12;
uint8_t  fakeMin = 0;
uint32_t lastClockTick= 0;

// SENSOR
float smoothTemp = NAN;
float  smoothHum = NAN;
uint32_t lastTempUpdate = 0;
uint32_t lastGPSUpdate  = 0;

float emaFilter(float prev, float next, float alpha = 0.25f) {
  if (isnan(prev)) return next;
  return prev + alpha * (next - prev);
}

// ONSCREEN KEYBOARD
#define KB_MAX_LEN  48

char    kbBuffer[KB_MAX_LEN + 1] = "";
uint8_t kbLen        = 0;
bool    kbIsPassword = false;
bool    kbShift      = false;
bool    kbSymbols    = false;
char    kbPrompt[32] = "";

enum KBTarget { KB_NONE, KB_WIFI_PASS, KB_CHAT_MSG };
KBTarget kbTarget = KB_NONE;

// key rows  lowercase letters; shift/symbols handled at draw+touch time 
const char* kbRow1 = "qwertyuiop";
const char* kbRow2 = "asdfghjkl";
const char* kbRow3 = "zxcvbnm";
const char* kbRow1Sym = "1234567890";
const char* kbRow2Sym = "@#$%&-+()";
const char* kbRow3Sym = "*\"':;!?";

#define KB_KEY_W   24
#define KB_KEY_H   28
#define KB_KEY_GAP 1
#define KB_TOP_Y   (CONTENT_Y + 70)

//GPS
uint8_t gpsPage = 0;   // 0,1,2,3  which GPS page is showing

//CLOCK

#define COL_ACCENT_CLOCK  YELLOW
uint8_t clockPage = 0;          // 0=clock ,1=stopwatch, 2=alarm
uint8_t  clockSec = 0;    // seconds
uint32_t lastSecTick = 0;    // ms timestamp of last second increment
bool     gpsSynced = false;// true once GPS gave us a valid time

// clock app NAV BUTTONS 

#define CLK_BTN_Y   (SCREEN_H - BOTTOM_H - 38)
#define CLK_BTN_H   30
#define CLK_BTN_W   60
#define CLK_PREV_X  10
#define CLK_NEXT_X  (SCREEN_W - CLK_BTN_W - 10)

// Stopwatch 
bool     swRunning  = false;
uint32_t swStartMs = 0;    // millis() when last started
uint32_t swAccumMs = 0;    // accumulated ms before last pause
bool swForceRedraw = false;

// Alarm
#define  MAX_ALARMS 2
struct Alarm {
  uint8_t hour;
  uint8_t minute;
  bool enabled;
  bool triggered;           // prevents re-fire same minute
};

Alarm alarms[MAX_ALARMS] = {{7, 0, false, false}, {12, 0, false, false}, };

uint8_t  alarmEditing = 0;  // which alarm row is selected
bool     alarmBuzzing = false;
uint32_t alarmBuzzStart = 0;
#define  AL_BUZZ_DUR    2000    // buzz for 5 seconds

int16_t prevHrX2 = -1, prevHrY2 = -1;
int16_t prevMinX2 = -1, prevMinY2 = -1;
int16_t prevSecX2 = -1, prevSecY2 = -1;

// TIC TAC TOE GAME 

// online handshake states
enum TTTOnlineState {
  ONLINE_IDLE,        // no req received or sent
  ONLINE_REQUESTING,  // we sent TTT:REQ, waiting for accept
  ONLINE_RECEIVING,   // we got TTT:REQ
  ONLINE_IN_GAME      // game active
};
TTTOnlineState tttOnlineState = ONLINE_IDLE;

// warning popup
bool tttShowWarning = false;

// left game message
bool tttShowLeftMsg = false;
uint32_t tttLeftMsgTimer = 0;


// board layout 
#define TTT_BOARD_X   15
#define TTT_BOARD_Y   (CONTENT_Y + 55)
#define TTT_BOARD_W   171
#define TTT_BOARD_H   171
#define TTT_CELL_W    57
#define TTT_CELL_H    57

// game state 
int8_t  tttBoard[9];       // 0=empty 1=X -1=O
bool    tttP1Turn  = true;  // true=X turn false=O turn
bool    tttOver   = false;
int8_t  tttWinner = 0;     // 0=none 1=X -1=O 2=draw
int8_t  tttWinLine[3];     // indices of winning cells
uint8_t tttScoreP1 = 0;
uint8_t tttScoreP2 = 0;

// TTT game state (offline + online shared)
bool    tttOnline         = false;
char    tttMySymbol       = ' ';    // Mega's symbol in online mode
bool    tttWaitingPick    = false;
uint8_t tttSeriesP1 = 0;   // whoever chooses first in game 1 is player 1 for whole series ,0=not decided, 1=first person who tapped, 2=second person
uint8_t tttThisGamePicker = 1;   // 1=P1 picks first this game, 2=P2 picks first this game
char    tttP1Symbol       = ' ';    // symbol player1 chose this game
char    tttP2Symbol       = ' ';    // symbol player2 got this game
uint32_t tttAutoResetTimer = 0;     // for 1 second auto reset after game ends
bool    tttAutoResetting  = false;

// DRAW 

uint16_t drawColor      = RED;
bool     toolbarVisible = true;

struct ColorSwatch { int16_t x; uint16_t color; };
ColorSwatch swatches[] = {
  { 5,  RED     },
  { 33,  LIME_GREEN   },
  { 59,  BLUE    },
  { 85, YELLOW  },
  { 111, MAGENTA },
  { 137, CYAN    },
};
const uint8_t SWATCH_COUNT = sizeof(swatches) / sizeof(swatches[0]);

#define SWATCH_SIZE  22
#define TOOLBAR_H 34
#define DOT_BTN_X (SCREEN_W - 30)
#define DOT_BTN_Y (CONTENT_Y + 6)
#define DOT_BTN_W  24
#define DOT_BTN_H 24
#define ERASE_BTN_X DOT_BTN_X-36
#define ERASE_BTN_Y  (DOT_BTN_Y)
#define ERASE_BTN_W  DOT_BTN_W+6
#define ERASE_BTN_H  22

// CHAT
#define MAX_MSGS     9
#define MAX_MSG_LEN  200

struct ChatMsg {
  char text[MAX_MSG_LEN + 1];
  bool sent;
};
ChatMsg chatLog[MAX_MSGS];
uint8_t chatCount = 0;

#define BUBBLE_GAP     6
#define BUBBLE_PAD_X   8
#define BUBBLE_PAD_Y   5
#define BUBBLE_MAX_W   185
#define BUBBLE_MARGIN  6
#define CHAT_AREA_Y    (CONTENT_Y + 32)
#define BUBBLE_LINE_H  10    // pixels per text line (size 1 font = 8px + 2 padding)
#define BUBBLE_CHARS_PER_LINE  ((BUBBLE_MAX_W - 2 * BUBBLE_PAD_X) / 6)  // chars that fit per line


//  NET STATE  
#define MAX_NET_ITEMS  10
#define NET_ITEM_LEN   32
#define NET_PAGE_SIZE 4

uint8_t wifiPage = 0;
char     wifiList[MAX_NET_ITEMS][NET_ITEM_LEN];
int8_t   wifiRSSI[MAX_NET_ITEMS];
uint8_t  wifiCount        = 0;
uint8_t  wifiSelected     = 0;
bool     wifiConnected    = false;
char     wifiConnectedSSID[NET_ITEM_LEN] = "";
bool     wifiScanning     = false;

uint8_t btPage   = 0;
char     btList[MAX_NET_ITEMS][NET_ITEM_LEN];
uint8_t  btCount          = 0;
uint8_t  btSelected       = 0;
bool     btConnected      = false;
char     btConnectedName[NET_ITEM_LEN] = "";
bool     btScanning       = false;

char     netStatusMsg[48] = "Ready";
bool     netWaiting       = false;   // waiting for ESP1 response

// wifi password buffer  typed via Serial Monitor 
char     wifiPassword[64] = "";
bool     wifiAwaitingPass = false;
char     wifiPendingSSID[NET_ITEM_LEN] = "";

// internet
char     httpResponse[128] = "";
bool     httpWaiting       = false;
char     httpPendingURL[96] = "";

// BOTTOM NAV
#define NAV_BTN_Y   (SCREEN_H - BOTTOM_H+2)
#define NAV_BTN_H   28
#define NAV_BACK_CX  40
#define NAV_HOME_CX  120
#define NAV_SET_CX   200

// FORWARD DECLARATIONS
void drawScreen();
void drawStatusBar();
void drawBottomBar();
void drawAppIcon(const AppIcon& ic);
void drawHomeScreen();
void navigateTo(ScreenMode newMode);
void navigateBack();
void navigateHome();
void handleTouch();
//keyboard
void startKeyboard(const char* prompt, bool isPassword, KBTarget target);
void drawKeyboardScreen();
void drawKBKey(int16_t x, int16_t y, int16_t w, const char* label, bool active);
void handleKeyboardTouch(int tx, int ty);
void kbFinish(bool accepted);
//temp
void drawTempScreen();
void updateTempScreen();
//gps
void drawGPSScreen();
void updateGPSScreen();
void drawGPSNavButtons();
void drawGPSPage0();
void drawGPSPage1();
void drawGPSPage2();
void updateGPSPage0();
void updateGPSPage1();
void updateGPSPage2();
void drawGPSPage3();
void updateGPSPage3();
//CLOCK APP
void drawClockScreen();
void drawClockNavButtons();
void drawClockPage0();
void drawClockPage1();
void drawClockPage2();
void updateClockPage0();
void updateClockPage1();
void updateClockPage2();
void handleClockTouch(int tx, int ty);
void handleClockPage1Touch(int tx, int ty);
void handleClockPage2Touch(int tx, int ty);
void syncGPSTime();
void buzzFor(uint16_t ms);
void updateClockBackground();
void checkAlarms();
//chat
void drawChatScreen();
void addMessage(const char* txt, bool sent);
void redrawChatBubbles();
void drawOneBubble(uint8_t idx, int16_t y);
//Handling protocols and messages
void sendsignal(const String& msg);
void handleProtocol(const char* raw);
//draw
void drawDrawScreen();
void handleDrawTouch(int tx, int ty);
void drawToolbar();
//others
void drawDotButton();
void drawCard(int16_t x, int16_t y, int16_t w, int16_t h,uint16_t bg, uint16_t border);
bool touchInRect(int tx, int ty, int rx, int ry, int rw, int rh);
//serial UART communication
void handleESPSerial();
//TTT
void drawTTTScreen();
void drawTTTBoard();
void drawTTTCell(uint8_t idx);
void drawTTTX(int16_t cx, int16_t cy);
void drawTTTO(int16_t cx, int16_t cy);
void drawTTTStatus();
void drawTTTScores();
void drawTTTWinLine();
void drawTTTRestartBtn();
void handleTTTTouch(int tx, int ty);
int8_t checkTTTWinner();
void resetTTTBoard();
void cellCenter(uint8_t idx, int16_t &cx, int16_t &cy);
void drawTTTModeSelect();
void handleTTTModeSelectTouch(int tx, int ty);
void drawTTTOnlineRequest();
void handleTTTOnlineRequestTouch(int tx, int ty);
void drawWarningPopup();
void handleWarningTouch(int tx, int ty);
void applyTTTOnlineMove(uint8_t cell, char symbol);
void drawTTTOnlineStatus();
void drawTTTSymbolPicker(bool isOnline);
void handleTTTSymbolPickerTouch(int tx, int ty);
void tttStartGame();
void tttLeaveGame();
void tttHandleGameEnd();
//connection and network
void drawNetScreen();
void drawNetWifiScreen();
void drawNetBTScreen();
void drawNetInternetScreen();
void handleNetTouch(int tx, int ty);
void handleNetWifiTouch(int tx, int ty);
void handleNetBTTouch(int tx, int ty);
void handleNetInternetTouch(int tx, int ty);
void handleNetProtocol(const String& msg);
void drawNetStatusBar(const char* msg, uint16_t color);
void drawNetListItem(uint8_t idx, int16_t y, const char* name, int8_t rssi, bool selected, bool connected);

// STATUS BAR
void drawStatusBar() {
  tft.fillRect(0, 0, SCREEN_W, STATUS_H, DARK_NAVY);
  tft.drawFastHLine(0, STATUS_H - 1, SCREEN_W, SLATE_BLUE);
  tft.setFont(NULL);
  tft.setTextSize(1);

  float t = dht.readTemperature();
  char tempStr[8];
  if (isnan(t)) strcpy(tempStr, "--.-C");
  else sprintf(tempStr, "%.1fC", t);
  tft.setTextColor(DARK_ORANGE );
  tft.setCursor(5, 10);
  tft.print(tempStr);

  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", fakeHour, fakeMin);
  tft.setTextColor(WHITE);
  tft.setCursor(SCREEN_W / 2 - 15, 10);
  tft.print(timeStr);

  int bx = SCREEN_W - 32, by = 8;
  tft.drawRect(bx, by, 22, 12, STEEL_GRAY);
  tft.drawRect(bx + 22, by + 3, 3, 6, STEEL_GRAY);
  tft.fillRect(bx + 2, by + 2, 14, 8, LIME_GREEN);
}

// BOTTOM BAR
void drawBottomBar() {
  int barY = SCREEN_H - BOTTOM_H;
  tft.fillRect(0, barY, SCREEN_W, BOTTOM_H, DARK_NAVY);
  tft.drawFastHLine(0, barY, SCREEN_W, SLATE_BLUE);

  int bx = NAV_BACK_CX;
  int by = NAV_BTN_Y + NAV_BTN_H / 2;
  tft.fillTriangle(bx - 10, by, bx + 4, by - 9, bx + 4, by + 9, CYAN);
  tft.drawCircle(NAV_HOME_CX, NAV_BTN_Y + NAV_BTN_H / 2, 9,  WHITE);
  tft.drawCircle(NAV_HOME_CX, NAV_BTN_Y + NAV_BTN_H / 2, 10, WHITE);
  tft.drawRect(NAV_SET_CX - 9, NAV_BTN_Y + 5, 18, 18, ORANGE);
  tft.drawRect(NAV_SET_CX - 6, NAV_BTN_Y + 8, 12, 12, ORANGE);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(NAV_BACK_CX - 9, NAV_BTN_Y + NAV_BTN_H + 2);
  tft.print("Back");
  tft.setCursor(NAV_HOME_CX - 9, NAV_BTN_Y + NAV_BTN_H + 2);
  tft.print("Home");
  tft.setCursor(NAV_SET_CX - 7,  NAV_BTN_Y + NAV_BTN_H + 2);
  tft.print("Set");
}

// HELPERS
void drawCard(int16_t x, int16_t y, int16_t w, int16_t h,
              uint16_t bg, uint16_t border) {
  tft.fillRoundRect(x, y, w, h, 8, bg);
  tft.drawRoundRect(x, y, w, h, 8, border);
}

bool touchInRect(int tx, int ty, int rx, int ry, int rw, int rh) {
  return tx >= rx && tx <= rx + rw && ty >= ry && ty <= ry + rh;
}

//navigation
void navigateTo(ScreenMode newMode) {
  if (navDepth < 10) navHistory[navDepth++] = currentMode;
  currentMode = newMode;
  drawScreen();
}

void navigateBack() {
  if (currentMode == GPS) gpsPage = 0;
  if (currentMode == CLOCK) clockPage = 0;
  if (navDepth > 0)
      currentMode = navHistory[--navDepth];
  else
      currentMode = HOME;
  drawScreen();
}

void navigateHome() {
  gpsPage = 0;
  clockPage = 0;
  navDepth = 0;
  currentMode = HOME;
  drawScreen();
}

// MASTER SCREEN
void drawScreen() {
  tft.fillScreen(BLACK);
  drawStatusBar();
  drawBottomBar();
  switch (currentMode) {
    case HOME: drawHomeScreen(); break;
    case TEMP: drawTempScreen(); break;
    case GPS: drawGPSScreen(); break;
    case DRAW: drawDrawScreen(); break;
    case CHAT: drawChatScreen(); break;
    case TICTACTOE: drawTTTScreen(); break;
    case TTT_MODE_SELECT: drawTTTModeSelect(); break;
    case TTT_ONLINE_REQUEST: drawTTTOnlineRequest(); break;
    case TTT_SYMBOL_PICK: drawTTTSymbolPicker(tttOnline); break;
    case NET: drawNetScreen(); break;
    case NET_WIFI: drawNetWifiScreen(); break;
    case NET_BT: drawNetBTScreen(); break;
    case NET_INTERNET: drawNetInternetScreen(); break;
    case KEYBOARD: drawKeyboardScreen(); break;
    case CLOCK: drawClockScreen(); break;
    
  }
}

// HOME SCREEN
void drawAppIcon(const AppIcon& ic) {
  tft.fillRoundRect(ic.x + 2, ic.y + 2, ICON_W, ICON_H, ICON_R, DARKGRAY);
  tft.fillRoundRect(ic.x, ic.y, ICON_W, ICON_H, ICON_R, ic.bg);
  tft.drawRoundRect(ic.x, ic.y, ICON_W, ICON_H, ICON_R, ic.accent);
  tft.fillRoundRect(ic.x + 6, ic.y + 6, ICON_W - 12, 5, 2, ic.accent);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  int16_t lw = strlen(ic.label) * 6;
  tft.setCursor(ic.x + (ICON_W - lw) / 2, ic.y + ICON_H - 20);
  tft.print(ic.label);
}

void drawHomeScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(10, CONTENT_Y + 22);
  tft.print("Mega Multitasker");
  tft.setFont(NULL);
  tft.drawFastHLine(10, CONTENT_Y + 32, SCREEN_W - 20, SLATE_BLUE);
  for (uint8_t i = 0; i < ICON_COUNT; i++) drawAppIcon(icons[i]);
  tft.setTextColor(DARKGRAY);
  tft.setCursor(5, SCREEN_H - BOTTOM_H - 12);
  tft.print("v3.0  Temp GPS Draw Chat TTT");}

// ON-SCREEN QWERTY KEYBOARD

//start a keyboard session
void startKeyboard(const char* prompt, bool isPassword, KBTarget target) {
  strncpy(kbPrompt, prompt, 31);
  kbPrompt[31] = '\0';
  kbBuffer[0] = '\0';
  kbLen  = 0;
  kbIsPassword = isPassword;
  kbTarget  = target;
  kbShift = false;
  kbSymbols = false;
  navigateTo(KEYBOARD);
}

// draw one key
void drawKBKey(int16_t x, int16_t y, int16_t w, const char* label, bool active) {
  uint16_t bg = active ? CYAN : DEEP_BLUE;
  uint16_t fg = active ? BLACK : WHITE;
  tft.fillRoundRect(x, y, w, KB_KEY_H, 3, bg);
  tft.drawRoundRect(x, y, w, KB_KEY_H, 3, SLATE_BLUE);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(fg);
  int16_t lw = strlen(label) * 6;
  tft.setCursor(x + (w - lw) / 2, y + (KB_KEY_H - 8) / 2);
  tft.print(label);
}

//full keyboard draw
void drawKeyboardScreen() {
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(CYAN);
  tft.setCursor(12, CONTENT_Y + 8);
  tft.print(kbPrompt);

  drawCard(8, CONTENT_Y + 18, SCREEN_W - 16, 30, DEEP_BLUE, CYAN);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(14, CONTENT_Y + 28);
  
  // show tail end if too long to fit 
  int16_t maxChars = (SCREEN_W - 28) / 6;
  if (kbLen > maxChars) tft.print(kbBuffer + (kbLen - maxChars));
  else tft.print(kbBuffer);

  // cursor blink bar 
  int16_t cx = 14 + min((int)kbLen, (SCREEN_W - 28) / 6) * 6;
  tft.fillRect(cx, CONTENT_Y + 26, 2, 14, CYAN);

  const char* row1 = kbSymbols ? kbRow1Sym : kbRow1;
  const char* row2 = kbSymbols ? kbRow2Sym : kbRow2;
  const char* row3 = kbSymbols ? kbRow3Sym : kbRow3;

  // row 1 
  int16_t y = KB_TOP_Y;
  for (uint8_t i = 0; i < strlen(row1); i++) {
    int16_t x = i * (KB_KEY_W + KB_KEY_GAP);
    char lbl[2] = { row1[i], '\0' };
    if (kbShift && !kbSymbols) lbl[0] = toupper(lbl[0]);
    drawKBKey(x, y, KB_KEY_W, lbl, false);
  }

  //row 2 (indented half key)
  y += KB_KEY_H + KB_KEY_GAP;
  int16_t row2Offset = KB_KEY_W / 2;
  for (uint8_t i = 0; i < strlen(row2); i++) {
    int16_t x = row2Offset + i * (KB_KEY_W + KB_KEY_GAP);
    char lbl[2] = { row2[i], '\0' };
    if (kbShift && !kbSymbols) lbl[0] = toupper(lbl[0]);
    drawKBKey(x, y, KB_KEY_W, lbl, false);
  }

  //row 3: SHIFT + letters + BACKSPACE
  y += KB_KEY_H + KB_KEY_GAP;
  int16_t shiftW = 28;
  drawKBKey(0, y, shiftW, "^", kbShift);
  for (uint8_t i = 0; i < strlen(row3); i++) {
    int16_t x = shiftW + KB_KEY_GAP + i * (KB_KEY_W + KB_KEY_GAP);
    char lbl[2] = { row3[i], '\0' };
    if (kbShift && !kbSymbols) lbl[0] = toupper(lbl[0]);
    drawKBKey(x, y, KB_KEY_W, lbl, false);
  }
  int16_t bsX = shiftW + KB_KEY_GAP + strlen(row3) * (KB_KEY_W + KB_KEY_GAP);
  int16_t bsW = SCREEN_W - bsX;
  drawKBKey(bsX, y, bsW, "<-", false);

  //row 4: 123/ABC, space, ENTER
  y += KB_KEY_H + KB_KEY_GAP;
  drawKBKey(0, y, 40, kbSymbols ? "ABC" : "123", kbSymbols);
  drawKBKey(42, y, 130, "SPACE", false);
  drawKBKey(174, y, SCREEN_W - 174, "ENTER", false);

  // cancel / back hint 
  tft.setTextColor(DARKGRAY);
  tft.setCursor(8, y + KB_KEY_H + 6);
  tft.print("Back button to cancel");
}

// keyboard touch handler
void handleKeyboardTouch(int tx, int ty) {
  const char* row1 = kbSymbols ? kbRow1Sym : kbRow1;
  const char* row2 = kbSymbols ? kbRow2Sym : kbRow2;
  const char* row3 = kbSymbols ? kbRow3Sym : kbRow3;

  int16_t y1 = KB_TOP_Y;
  int16_t y2 = y1 + KB_KEY_H + KB_KEY_GAP;
  int16_t y3 = y2 + KB_KEY_H + KB_KEY_GAP;
  int16_t y4 = y3 + KB_KEY_H + KB_KEY_GAP;

  // row 1 
  if (ty >= y1 && ty < y1 + KB_KEY_H) {
    for (uint8_t i = 0; i < strlen(row1); i++) {
      int16_t x = i * (KB_KEY_W + KB_KEY_GAP);
      if (tx >= x && tx < x + KB_KEY_W) {
        char c = row1[i];
        if (kbShift && !kbSymbols) c = toupper(c);
        if (kbLen < KB_MAX_LEN) { kbBuffer[kbLen++] = c; kbBuffer[kbLen] = '\0'; }
        kbShift = false;
        drawKeyboardScreen();
        return;
      }
    }
  }

  // row 2 
  if (ty >= y2 && ty < y2 + KB_KEY_H) {
    int16_t row2Offset = KB_KEY_W / 2;
    for (uint8_t i = 0; i < strlen(row2); i++) {
      int16_t x = row2Offset + i * (KB_KEY_W + KB_KEY_GAP);
      if (tx >= x && tx < x + KB_KEY_W) {
        char c = row2[i];
        if (kbShift && !kbSymbols) c = toupper(c);
        if (kbLen < KB_MAX_LEN) { kbBuffer[kbLen++] = c; kbBuffer[kbLen] = '\0'; }
        kbShift = false;
        drawKeyboardScreen();
        return;
      }
    }
  }

  // row 3 
  if (ty >= y3 && ty < y3 + KB_KEY_H) {
    int16_t shiftW = 28;
    // shift key 
    if (tx >= 0 && tx < shiftW) {
      kbShift = !kbShift;
      drawKeyboardScreen();
      return;
    }
    for (uint8_t i = 0; i < strlen(row3); i++) {
      int16_t x = shiftW + KB_KEY_GAP + i * (KB_KEY_W + KB_KEY_GAP);
      if (tx >= x && tx < x + KB_KEY_W) {
        char c = row3[i];
        if (kbShift && !kbSymbols) c = toupper(c);
        if (kbLen < KB_MAX_LEN) { kbBuffer[kbLen++] = c; kbBuffer[kbLen] = '\0'; }
        kbShift = false;
        drawKeyboardScreen();
        return;
      }
    }
    // backspace 
    int16_t bsX = shiftW + KB_KEY_GAP + strlen(row3) * (KB_KEY_W + KB_KEY_GAP);
    if (tx >= bsX && tx < SCREEN_W) {
      if (kbLen > 0) { kbLen--; kbBuffer[kbLen] = '\0'; }
      drawKeyboardScreen();
      return;
    }
  }

  // row 4 
  if (ty >= y4 && ty < y4 + KB_KEY_H) {
    // 123/ABC toggle 
    if (tx >= 0 && tx < 40) {
      kbSymbols = !kbSymbols;
      kbShift = false;
      drawKeyboardScreen();
      return;
    }
    // space 
    if (tx >= 42 && tx < 172) {
      if (kbLen < KB_MAX_LEN) { kbBuffer[kbLen++] = ' '; kbBuffer[kbLen] = '\0'; }
      drawKeyboardScreen();
      return;
    }
    // enter 
    if (tx >= 174 && tx < SCREEN_W) {
      kbFinish(true);
      return;
    }
  }
}

//finish session, move to previous screen
void kbFinish(bool accepted) {
  if (accepted) {
    if (kbTarget == KB_WIFI_PASS) {
      strncpy(wifiPassword, kbBuffer, 63);
      wifiAwaitingPass = false;
      char packet[128];
      snprintf(packet, sizeof(packet), "ESP1:NET:WIFI:CONNECT:%s:%s",
               wifiPendingSSID, wifiPassword);
      sendsignal(packet);
      wifiPassword[0] = '\0';
      snprintf(netStatusMsg, sizeof(netStatusMsg),"Connecting to %s...", wifiPendingSSID);
      netWaiting = true;
      navigateBack();   // back to NET_WIFI 
      if (currentMode == NET_WIFI) drawNetStatusBar(netStatusMsg, ORANGE);

    } else if (kbTarget == KB_CHAT_MSG) {
      if (kbLen > 0) {
        addMessage(kbBuffer, true);
        char packet[80];
        snprintf(packet, sizeof(packet), "ESP2:CHAT:%s", kbBuffer);
        sendsignal(packet);
      }
      navigateBack();   // back to CHAT 
    }
  } else {
    navigateBack();
  }
  kbBuffer[0] = '\0';
  kbLen       = 0;
  kbTarget    = KB_NONE;
}

//   TEMP
void drawTempScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(DARK_ORANGE );
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Temperature");
  tft.setFont(NULL);
  tft.drawFastHLine(10, CONTENT_Y + 26, SCREEN_W - 20, SLATE_BLUE);

  drawCard(10, CONTENT_Y + 34,  SCREEN_W - 20, 80, DEEP_BLUE, DARK_ORANGE );
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, CONTENT_Y + 44);
  tft.print("TEMPERATURE");

  drawCard(10, CONTENT_Y + 124, SCREEN_W - 20, 80, DEEP_BLUE, CYAN);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, CONTENT_Y + 134);
  tft.print("HUMIDITY");

  lastTempUpdate = 0;
  updateTempScreen();
}

void updateTempScreen() {
  if (millis() - lastTempUpdate < 2000) return;
  lastTempUpdate = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) return;

  smoothTemp = emaFilter(smoothTemp, t);
  smoothHum  = emaFilter(smoothHum,  h);

  tft.fillRect(14, CONTENT_Y + 54,  SCREEN_W - 28, 52, DEEP_BLUE);
  tft.fillRect(14, CONTENT_Y + 144, SCREEN_W - 28, 52, DEEP_BLUE);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(2);
  tft.setTextColor(DARK_ORANGE );
  tft.setCursor(20, CONTENT_Y + 98);
  tft.print(smoothTemp, 1);
  tft.setTextSize(1);
  tft.print(" C");

  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(20, CONTENT_Y + 188);
  tft.print(smoothHum, 0);
  tft.setTextSize(1);
  tft.print(" %");

  tft.setFont(NULL);
  tft.setTextSize(1);
}

//GPS LAYOUT CONSTANTS
    
#define GPS_CARD1_Y   (CONTENT_Y + 32)
#define GPS_CARD1_H   82
#define GPS_CARD2_Y   (CONTENT_Y + 122)
#define GPS_CARD2_H   82
#define GPS_BTN_Y     (SCREEN_H - BOTTOM_H - 38)
#define GPS_BTN_H     30
#define GPS_BTN_W     60
#define GPS_PREV_X    10
#define GPS_NEXT_X    (SCREEN_W - GPS_BTN_W - 10)

 // nav buttons   
void drawGPSNavButtons() {
   // PREV   
  if (gpsPage > 0) {
    tft.fillRoundRect(GPS_PREV_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, 6, DEEP_BLUE);
    tft.drawRoundRect(GPS_PREV_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, 6, LIME_GREEN);
    tft.setFont(NULL); tft.setTextSize(1);
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(GPS_PREV_X + 10, GPS_BTN_Y + 11);
    tft.print("< PREV");
  } else {
    tft.fillRect(GPS_PREV_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, BLACK);
  }

   // page dots( 4 pages )  
  for (uint8_t i = 0; i < 4; i++) {
    int16_t dx = SCREEN_W / 2 - 21 + i * 14;
    int16_t dy = GPS_BTN_Y + GPS_BTN_H / 2;
    if (i == gpsPage)
      tft.fillCircle(dx, dy, 4, LIME_GREEN);
    else {
      tft.fillCircle(dx, dy, 4, BLACK);
      tft.drawCircle(dx, dy, 4, DARKGRAY);
    }
  }

   // NEXT   
  if (gpsPage < 3) {
    tft.fillRoundRect(GPS_NEXT_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, 6, DEEP_BLUE);
    tft.drawRoundRect(GPS_NEXT_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, 6, LIME_GREEN);
    tft.setFont(NULL); tft.setTextSize(1);
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(GPS_NEXT_X + 10, GPS_BTN_Y + 11);
    tft.print("NEXT >");
  } else {
    tft.fillRect(GPS_NEXT_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H, BLACK);
  }
}

 //  PAGE 0  Fix Status + Coordinates    
void drawGPSPage0() {
  drawCard(10, GPS_CARD1_Y, SCREEN_W - 20, GPS_CARD1_H, DEEP_BLUE, LIME_GREEN);
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD1_Y + 10);
  tft.print("FIX STATUS");

  drawCard(10, GPS_CARD2_Y, SCREEN_W - 20, GPS_CARD2_H, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD2_Y + 10);
  tft.print("COORDINATES");
}

void updateGPSPage0() {
  tft.setFont(NULL); tft.setTextSize(1);

   // fix status   
  tft.fillRect(14, GPS_CARD1_Y + 22, SCREEN_W - 28, GPS_CARD1_H - 28, DEEP_BLUE);
  if (gps.location.isValid()) {
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(18, GPS_CARD1_Y + 26);
    tft.print("Fix acquired");
    char buf[24];
    sprintf(buf, "Satellites: %d", (int)gps.satellites.value());
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(18, GPS_CARD1_Y + 42);
    tft.print(buf);
    sprintf(buf, "Data age: %lu ms", gps.location.age());
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD1_Y + 58);
    tft.print(buf);
  } else {
    tft.setTextColor(ORANGE);
    tft.setCursor(18, GPS_CARD1_Y + 26);
    tft.print("Searching...");
    char buf[24];
    sprintf(buf, "Sats seen: %d", (int)gps.satellites.value());
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD1_Y + 42);
    tft.print(buf);
  }

   // coordinates   
  tft.fillRect(14, GPS_CARD2_Y + 22, SCREEN_W - 28, GPS_CARD2_H - 28, DEEP_BLUE);
  if (gps.location.isValid()) {
    char latBuf[16], lngBuf[16];
    dtostrf(gps.location.lat(), 10, 6, latBuf);
    dtostrf(gps.location.lng(), 10, 6, lngBuf);
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(18, GPS_CARD2_Y + 26);
    tft.print("Lat: ");
    tft.setTextColor(WHITE);
    tft.print(latBuf);
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(18, GPS_CARD2_Y + 42);
    tft.print("Lng: ");
    tft.setTextColor(WHITE);
    tft.print(lngBuf);
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD2_Y + 42);
    tft.print("-- no fix --");
  }
}

 //  PAGE 1  Altitude + Speed    
void drawGPSPage1() {
  drawCard(10, GPS_CARD1_Y, SCREEN_W - 20, GPS_CARD1_H, DEEP_BLUE, LIME_GREEN);
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD1_Y + 10);
  tft.print("ALTITUDE");

  drawCard(10, GPS_CARD2_Y, SCREEN_W - 20, GPS_CARD2_H, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD2_Y + 10);
  tft.print("SPEED");
}

void updateGPSPage1() {
  tft.setFont(NULL); tft.setTextSize(1);

   // altitude   
  tft.fillRect(14, GPS_CARD1_Y + 22, SCREEN_W - 28, GPS_CARD1_H - 28, DEEP_BLUE);
  if (gps.altitude.isValid()) {
    char mBuf[12], fBuf[12];
    dtostrf(gps.altitude.meters(), 6, 1, mBuf);
    dtostrf(gps.altitude.feet(),   6, 1, fBuf);
    tft.setTextColor(WHITE);
    tft.setCursor(18, GPS_CARD1_Y + 28);
    tft.print(mBuf); tft.print(" m");
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(18, GPS_CARD1_Y + 44);
    tft.print(fBuf); tft.print(" ft");
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD1_Y + 42);
    tft.print("-- no data");
  }

   // speed   
  tft.fillRect(14, GPS_CARD2_Y + 22, SCREEN_W - 28, GPS_CARD2_H - 28, DEEP_BLUE);
  if (gps.speed.isValid()) {
    char buf[28];
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(18, GPS_CARD2_Y + 28);
    sprintf(buf, "%.1f km/h", gps.speed.kmph());
    tft.print(buf);
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(18, GPS_CARD2_Y + 44);
    sprintf(buf, "%.1f mph  /  %.1f kn", gps.speed.mph(), gps.speed.knots());
    tft.print(buf);
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD2_Y + 42);
    tft.print("-- no data");
  }
}

 //  PAGE 2  Course + Date/Time    
void drawGPSPage2() {
  drawCard(10, GPS_CARD1_Y, SCREEN_W - 20, GPS_CARD1_H, DEEP_BLUE, LIME_GREEN);
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD1_Y + 10);
  tft.print("COURSE / HEADING");

  drawCard(10, GPS_CARD2_Y, SCREEN_W - 20, GPS_CARD2_H, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD2_Y + 10);
  tft.print("DATE & TIME  (IST)");
}

void updateGPSPage2() {
  tft.setFont(NULL); tft.setTextSize(1);

   // course   
  tft.fillRect(14, GPS_CARD1_Y + 22, SCREEN_W - 28, GPS_CARD1_H - 28, DEEP_BLUE);
  if (gps.course.isValid()) {
    double deg = gps.course.deg();
    char buf[24];
    sprintf(buf, "%.2f degrees", deg);
    tft.setTextColor(WHITE);
    tft.setCursor(18, GPS_CARD1_Y + 28);
    tft.print(buf);
    const char* dir = "N";
    if      (deg >=  22.5 && deg <  67.5) dir = "NE";
    else if (deg >=  67.5 && deg < 112.5) dir = "E";
    else if (deg >= 112.5 && deg < 157.5) dir = "SE";
    else if (deg >= 157.5 && deg < 202.5) dir = "S";
    else if (deg >= 202.5 && deg < 247.5) dir = "SW";
    else if (deg >= 247.5 && deg < 292.5) dir = "W";
    else if (deg >= 292.5 && deg < 337.5) dir = "NW";
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(18, GPS_CARD1_Y + 44);
    tft.print("Direction: "); tft.print(dir);
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD1_Y + 42);
    tft.print("-- no course data");
  }

   // date & time   
  tft.fillRect(14, GPS_CARD2_Y + 22, SCREEN_W - 28, GPS_CARD2_H - 28, DEEP_BLUE);
  if (gps.date.isValid() && gps.time.isValid()) {
    char dateBuf[24], timeBuf[24];
    sprintf(dateBuf, "%04d-%02d-%02d",
            gps.date.year(), gps.date.month(), gps.date.day());
    sprintf(timeBuf, "%02d:%02d:%02d IST",
            gps.time.hour(), gps.time.minute(), gps.time.second());
    tft.setTextColor(WHITE);
    tft.setCursor(18, GPS_CARD2_Y + 28);
    tft.print(dateBuf);
    tft.setCursor(18, GPS_CARD2_Y + 44);
    tft.print(timeBuf);
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD2_Y + 42);
    tft.print("No time data yet");
  }
}

 //  PAGE 3  HDOP + Signal Stats    
void drawGPSPage3() {
  drawCard(10, GPS_CARD1_Y, SCREEN_W - 20, GPS_CARD1_H, DEEP_BLUE, LIME_GREEN);
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD1_Y + 10);
  tft.print("ACCURACY  (HDOP)");

  drawCard(10, GPS_CARD2_Y, SCREEN_W - 20, GPS_CARD2_H, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD2_Y + 10);
  tft.print("SIGNAL STATS");
}

void updateGPSPage3() {
  tft.setFont(NULL); tft.setTextSize(1);

   // HDOP   
  tft.fillRect(14, GPS_CARD1_Y + 22, SCREEN_W - 28, GPS_CARD1_H - 28, DEEP_BLUE);
  if (gps.hdop.isValid()) {
    double hdop = gps.hdop.hdop();
    char buf[16];
    dtostrf(hdop, 5, 2, buf);
    tft.setTextColor(WHITE);
    tft.setCursor(18, GPS_CARD1_Y + 28);
    tft.print("HDOP: "); tft.print(buf);
    tft.setCursor(18, GPS_CARD1_Y + 44);
    if      (hdop < 1.0)  { tft.setTextColor(LIME_GREEN); tft.print("Ideal"); }
    else if (hdop < 2.0)  { tft.setTextColor(LIME_GREEN); tft.print("Excellent"); }
    else if (hdop < 5.0)  { tft.setTextColor(YELLOW);         tft.print("Good"); }
    else if (hdop < 10.0) { tft.setTextColor(ORANGE);         tft.print("Moderate"); }
    else                  { tft.setTextColor(RED);             tft.print("Poor"); }
  } else {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(18, GPS_CARD1_Y + 42);
    tft.print("-- no HDOP data");
  }

   // signal stats   
  tft.fillRect(14, GPS_CARD2_Y + 22, SCREEN_W - 28, GPS_CARD2_H - 28, DEEP_BLUE);
  char buf[32];
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, GPS_CARD2_Y + 28);
  sprintf(buf, "Chars:     %lu", gps.charsProcessed());
  tft.print(buf);
  tft.setCursor(18, GPS_CARD2_Y + 44);
  sprintf(buf, "Sentences: %lu", gps.sentencesWithFix());
  tft.print(buf);
  tft.setCursor(18, GPS_CARD2_Y + 60);
  sprintf(buf, "CRC fails: %lu", gps.failedChecksum());
  tft.setTextColor(gps.failedChecksum() > 0 ? ORANGE : DARKGRAY);
  tft.print(buf);
}

//GPS MASTER DRAW
    
void drawGPSScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(LIME_GREEN);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("GPS");
  tft.setFont(NULL);

  const char* titles[] = { "Location", "Altitude & Speed","Course & Time", "Accuracy" };
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(62, CONTENT_Y + 14);
  tft.fillRect(60, CONTENT_Y + 13, 100, 30, TFT_BLACK);
  tft.print(titles[gpsPage]);


  tft.drawFastHLine(10, CONTENT_Y + 26, SCREEN_W - 20, SLATE_BLUE);

  switch (gpsPage) {
    case 0: drawGPSPage0(); break;
    case 1: drawGPSPage1(); break;
    case 2: drawGPSPage2(); break;
    case 3: drawGPSPage3(); break;
  }

  drawGPSNavButtons();
  lastGPSUpdate = 0;
  updateGPSScreen();
}

//GPS UPDATE
    
void updateGPSScreen() {
  if (millis() - lastGPSUpdate < 2000) return;
  lastGPSUpdate = millis();
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  switch (gpsPage) {
    case 0: updateGPSPage0(); break;
    case 1: updateGPSPage1(); break;
    case 2: updateGPSPage2(); break;
    case 3: updateGPSPage3(); break;
  }
}

//CLOCK APP

//buzzer
void buzzFor(uint16_t ms) {
  tone(BUZZER_PIN, 1000, ms);   // 1 kHz tone, duration ms
}

// GPS TIME SYNC Call once per second inside updateClockBackground()

void syncGPSTime() {
  if (!gps.time.isValid() || !gps.date.isValid()) return;

  // GPS gives IST
  int16_t h = gps.time.hour() ;
  int16_t m = gps.time.minute();
  int16_t s = gps.time.second();

  // carry
  if (m >= 60) { m -= 60; h++; }
  if (m <  0)  { m += 60; h--; }
  if (h >= 24) h -= 24;
  if (h <  0)  h += 24;

  fakeHour  = (uint8_t)h;
  fakeMin   = (uint8_t)m;
  clockSec  = (uint8_t)s;
  gpsSynced = true;
}

//BACKGROUND UPDATER  (called every loop tick) This replaces the existing millis()-based fake clock in loop()

void updateClockBackground() {
  uint32_t now = millis();

  // second tick
  if (now - lastSecTick >= 1000UL) {
    lastSecTick = now;
    syncGPSTime();// try GPS sync every second

    // if GPS not available, free-run the fake clock
    if (!gpsSynced) {
      if (++clockSec >= 60) {
        clockSec = 0;
        if (++fakeMin >= 60) {
          fakeMin = 0;
          if (++fakeHour >= 24) fakeHour = 0;
        }
      }
    }

    // status-bar time update (replaces the old 60-second ticker)
    if (currentMode != CLOCK) drawStatusBar();   // CLOCK redraws its own
  }

  //  stopwatch running tick → refresh display 
  if (swRunning && currentMode == CLOCK && clockPage == 1) {
    updateClockPage1();   // internally throttled to once per second
  }
  checkAlarms();

  // stop alarm buzz after duration
  if (alarmBuzzing && now - alarmBuzzStart >= (uint32_t)AL_BUZZ_DUR) {
    alarmBuzzing = false;
    noTone(BUZZER_PIN);
  }
}

// ALARM CHECK

void checkAlarms() {
  static uint8_t lastCheckedMin = 255;
  if (fakeMin == lastCheckedMin) return;   // check once per new minute
  lastCheckedMin = fakeMin;

  for (uint8_t i = 0; i < MAX_ALARMS; i++) {
    if (!alarms[i].enabled) { alarms[i].triggered = false; continue; }
    if (alarms[i].hour == fakeHour && alarms[i].minute == fakeMin) {
      if (!alarms[i].triggered) {
        alarms[i].triggered = true;
        alarmBuzzing   = true;
        alarmBuzzStart = millis();
        buzzFor(AL_BUZZ_DUR);
        // optional: flash to Clock page
        if (currentMode == CLOCK) { clockPage = 2; drawClockScreen(); }
      }
    } else {
      alarms[i].triggered = false;   // reset for next day
    }
  }
}

void drawClockNavButtons() {
  if (clockPage > 0) {
    tft.fillRoundRect(CLK_PREV_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, 6, DEEP_BLUE);
    tft.drawRoundRect(CLK_PREV_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, 6, COL_ACCENT_CLOCK);
    tft.setFont(NULL); tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT_CLOCK);
    tft.setCursor(CLK_PREV_X + 10, CLK_BTN_Y + 11);
    tft.print("< PREV");
  } else {
    tft.fillRect(CLK_PREV_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, BLACK);
  }

  // 3 page dots
  for (uint8_t i = 0; i < 3; i++) {
    int16_t dx = SCREEN_W / 2 - 21 + i * 14;
    int16_t dy = CLK_BTN_Y + CLK_BTN_H / 2;
    if (i == clockPage) tft.fillCircle(dx, dy, 4, COL_ACCENT_CLOCK);
    else {
      tft.fillCircle(dx, dy, 4, BLACK);
      tft.drawCircle(dx, dy, 4, DARKGRAY);
    }
  }

  if (clockPage < 2) {
    tft.fillRoundRect(CLK_NEXT_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, 6, DEEP_BLUE);
    tft.drawRoundRect(CLK_NEXT_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, 6, COL_ACCENT_CLOCK);
    tft.setFont(NULL); tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT_CLOCK);
    tft.setCursor(CLK_NEXT_X + 10, CLK_BTN_Y + 11);
    tft.print("NEXT >");
  } else {
    tft.fillRect(CLK_NEXT_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H, BLACK);
  }
}

//MASTER CLOCK SCREEN DRAW

void drawClockScreen() {
  tft.fillRect(0, CONTENT_Y, SCREEN_W, SCREEN_H - CONTENT_Y - BOTTOM_H, BLACK);
  // reset all hand endpoints
  prevHrX2  = prevHrY2  = -1;
  prevMinX2 = prevMinY2 = -1;
  prevSecX2 = prevSecY2 = -1;
  tft.setTextColor(COL_ACCENT_CLOCK);
  tft.setCursor(12, CONTENT_Y + 10);

  const char* titles[] = { "Clock", "Stopwatch", "Alarm" };
  tft.print(titles[clockPage]);
  tft.setFont(NULL);

  // GPS sync badge
  tft.setTextSize(1);
  tft.setTextColor(gpsSynced ? LIME_GREEN : DARKGRAY);
  tft.setCursor(SCREEN_W - 64, CONTENT_Y + 14);
  tft.print(gpsSynced ? "GPS OK" : "No GPS");

  tft.drawFastHLine(10, CONTENT_Y + 26, SCREEN_W - 20, SLATE_BLUE);

  switch (clockPage) {
    case 0: drawClockPage0(); break;
    case 1: drawClockPage1(); break;
    case 2: drawClockPage2(); break;
  }
  drawClockNavButtons();
}

//  PAGE 0  ANALOG CLOCK

// Clock face centre & radius
#define CLK_CX   120
#define CLK_CY   (CONTENT_Y + 125)
#define CLK_R    80

// helper: point on circle
static void clkPt(int16_t cx, int16_t cy, int16_t r, float deg, int16_t &ox, int16_t &oy) {
  float rad = deg * PI / 180.0f;
  ox = cx + (int16_t)(r * sin(rad));
  oy = cy - (int16_t)(r * cos(rad));
}

void drawClockFace() {
  // outer ring
  tft.drawCircle(CLK_CX, CLK_CY, CLK_R - 1, COL_ACCENT_CLOCK);
  tft.drawCircle(CLK_CX, CLK_CY, CLK_R - 2, SLATE_BLUE);

  // hour tick marks  from R-4 to R-14 (well inside rim)
  for (uint8_t h = 0; h < 12; h++) {
    float deg = h * 30.0f;
    int16_t x1, y1, x2, y2;
    clkPt(CLK_CX, CLK_CY, CLK_R - 4,  deg, x1, y1);
    clkPt(CLK_CX, CLK_CY, CLK_R - 14, deg, x2, y2);
    uint16_t col = (h % 3 == 0) ? COL_ACCENT_CLOCK : SLATE_BLUE;
    tft.drawLine(x1, y1, x2, y2, col);
  }

  // minute tick marks  from R-4 to R-8 only (very short)
  for (uint8_t m = 0; m < 60; m++) {
    if (m % 5 == 0) continue;
    float deg = m * 6.0f;
    int16_t x1, y1, x2, y2;
    clkPt(CLK_CX, CLK_CY, CLK_R - 4, deg, x1, y1);
    clkPt(CLK_CX, CLK_CY, CLK_R - 8, deg, x2, y2);
    tft.drawLine(x1, y1, x2, y2, DARKGRAY);
  }

  // hour numbers  placed at R-22 so they sit inside tick zone
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  const char* nums[] = {"12","1","2","3","4","5","6","7","8","9","10","11"};
  for (uint8_t h = 0; h < 12; h++) {
    float deg = h * 30.0f;
    int16_t nx, ny;
    clkPt(CLK_CX, CLK_CY, CLK_R - 22, deg, nx, ny);
    uint8_t lw = strlen(nums[h]) * 6;
    tft.setCursor(nx - lw / 2, ny - 4);
    tft.print(nums[h]);
  }
  // centre ring  permanent(hands dont touch it thus no updation), drawn with face
  tft.drawCircle(CLK_CX, CLK_CY, 4, COL_ACCENT_CLOCK);
  tft.drawCircle(CLK_CX, CLK_CY, 5, COL_ACCENT_CLOCK);
}

// draws a hand FROM the ring edge outward to tip  centre ring never touched
void drawHand(int16_t cx, int16_t cy, int16_t len, float deg,
              uint16_t color, uint8_t thick) {
  int16_t x1, y1, x2, y2;
  clkPt(cx, cy, 8, deg, x1, y1);    // start just outside the ring (radius 8)
  clkPt(cx, cy, len, deg, x2, y2);  // end at tip
  for (int8_t t = -(thick / 2); t <= (thick / 2); t++) {
    tft.drawLine(x1 + t, y1, x2 + t, y2, color);
    tft.drawLine(x1, y1 + t, x2, y2 + t, color);
  }
}


// erase one hand by redrawing it black, then restore any tick marks it crossed
void eraseHandClean(int16_t cx, int16_t cy,int16_t x2, int16_t y2, uint8_t thick) {
  // recompute the degree from stored endpoint to get start point at ring edge
  // simpler: just erase the full line from near-centre to tip
  // we use radius 8 as start  same as drawHand
  float dx = x2 - cx, dy = y2 - cy;
  float len = sqrt(dx*dx + dy*dy);
  if (len == 0) return;
  int16_t x1 = cx + (int16_t)(8.0f * dx / len);
  int16_t y1 = cy + (int16_t)(8.0f * dy / len);
  for (int8_t t = -(thick/2); t <= (thick/2); t++) {
    tft.drawLine(x1 + t, y1, x2 + t, y2, BLACK);
    tft.drawLine(x1, y1 + t, x2, y2 + t, BLACK);
  }
}

// redraw any tick marks that fall within a bounding box
// called after erasing a hand to restore face detail
void restoreTicksNearHand(int16_t x2, int16_t y2) {
  // bounding box of erased area (hand tip region)
  int16_t bx1 = min(x2, CLK_CX) - 4;
  int16_t by1 = min(y2, CLK_CY) - 4;
  int16_t bx2 = max(x2, CLK_CX) + 4;
  int16_t by2 = max(y2, CLK_CY) + 4;

  // redraw only hour ticks that overlap the bounding box
  for (uint8_t h = 0; h < 12; h++) {
    float    deg = h * 30.0f;
    int16_t  tx1, ty1, tx2, ty2;
    clkPt(CLK_CX, CLK_CY, CLK_R - 4,  deg, tx1, ty1);
    clkPt(CLK_CX, CLK_CY, CLK_R - 14, deg, tx2, ty2);
    // check if tick is near erased region
    if (tx1 >= bx1 && tx1 <= bx2 && ty1 >= by1 && ty1 <= by2) {
      uint16_t col = (h % 3 == 0) ? COL_ACCENT_CLOCK : SLATE_BLUE;
      tft.drawLine(tx1, ty1, tx2, ty2, col);
    }
  }

  // redraw minute ticks near erased region
  for (uint8_t m = 0; m < 60; m++) {
    if (m % 5 == 0) continue;
    float    deg = m * 6.0f;
    int16_t  tx1, ty1, tx2, ty2;
    clkPt(CLK_CX, CLK_CY, CLK_R - 4, deg, tx1, ty1);
    clkPt(CLK_CX, CLK_CY, CLK_R - 8, deg, tx2, ty2);
    if (tx1 >= bx1 && tx1 <= bx2 && ty1 >= by1 && ty1 <= by2) {
      tft.drawLine(tx1, ty1, tx2, ty2, DARKGRAY);
    }
  }

  // redraw hour numbers near erased region
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  const char* nums[] = {"12","1","2","3","4","5","6","7","8","9","10","11"};
  for (uint8_t h = 0; h < 12; h++) {
    float   deg = h * 30.0f;
    int16_t nx, ny;
    clkPt(CLK_CX, CLK_CY, CLK_R - 22, deg, nx, ny);
    // number bounding box approx
    if (nx + 12 >= bx1 && nx - 12 <= bx2 &&
        ny + 8  >= by1 && ny - 8  <= by2) {
      uint8_t lw = strlen(nums[h]) * 6;
      tft.setCursor(nx - lw/2, ny - 4);
      tft.print(nums[h]);
    }
  }
}

void drawAnalogClock() {
  float secDeg = clockSec * 6.0f;
  float minDeg = fakeMin  * 6.0f + clockSec * 0.1f;
  float hrDeg  = (fakeHour % 12) * 30.0f + fakeMin * 0.5f;

  // compute new endpoints
  int16_t newHrX2,  newHrY2;
  int16_t newMinX2, newMinY2;
  int16_t newSecX2, newSecY2;
  clkPt(CLK_CX, CLK_CY, CLK_R - 28, hrDeg,  newHrX2,  newHrY2);
  clkPt(CLK_CX, CLK_CY, CLK_R - 14, minDeg, newMinX2, newMinY2);
  clkPt(CLK_CX, CLK_CY, CLK_R - 18, secDeg, newSecX2, newSecY2);

  // erase old hands
  if (prevSecX2 >= 0) {
    //tft.fillCircle(CLK_CX, CLK_CY, 5, BLACK);

    eraseHandClean(CLK_CX, CLK_CY, prevSecX2, prevSecY2, 1);
    restoreTicksNearHand(prevSecX2, prevSecY2);

    eraseHandClean(CLK_CX, CLK_CY, prevMinX2, prevMinY2, 3);
    restoreTicksNearHand(prevMinX2, prevMinY2);

    eraseHandClean(CLK_CX, CLK_CY, prevHrX2,  prevHrY2,  4);
    restoreTicksNearHand(prevHrX2, prevHrY2);
  }

  //  draw new hands 
  drawHand(CLK_CX, CLK_CY, CLK_R - 28, hrDeg,  WHITE,     4);
  drawHand(CLK_CX, CLK_CY, CLK_R - 14, minDeg, WHITE,     3);
  drawHand(CLK_CX, CLK_CY, CLK_R - 18, secDeg, COL_ACCENT_CLOCK, 1);

  // store endpoints
  prevHrX2  = newHrX2;  prevHrY2  = newHrY2;
  prevMinX2 = newMinX2; prevMinY2 = newMinY2;
  prevSecX2 = newSecX2; prevSecY2 = newSecY2;
}

void drawClockPage0() {

  drawClockFace();   // draw face ONCE here, never again until page re-entry
  if (gps.date.isValid()) {
    tft.setFont(NULL); tft.setTextSize(1);
    tft.setTextColor(STEEL_GRAY);
    char dateBuf[20];
    sprintf(dateBuf, "%04d-%02d-%02d",
            gps.date.year(), gps.date.month(), gps.date.day());
    tft.setCursor((SCREEN_W - strlen(dateBuf)*6)/2, CLK_BTN_Y - 46);
    tft.print(dateBuf);
  }

  drawAnalogClock();  // first hand draw
}

void updateClockPage0() {
  static uint8_t lastSec = 255;
  if (clockSec == lastSec) return;
  lastSec = clockSec;
  drawAnalogClock();   // erase old hands, draw new  face untouched
}

//  PAGE 1  STOPWATCH

#define SW_BTN_Y  (CONTENT_Y + 180)
#define SW_BTN_H  36
#define SW_BTN_W  90

void drawClockPage1() {
  tft.setFont(NULL); tft.setTextSize(1);

  // big time card
  drawCard(10, CONTENT_Y + 38, SCREEN_W - 20, 80, DEEP_BLUE, COL_ACCENT_CLOCK);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(18, CONTENT_Y + 50);
  tft.print("ELAPSED");

  // START/STOP button
  uint16_t btnCol = swRunning ? RED : LIME_GREEN;
  tft.fillRoundRect(20, CONTENT_Y + 150, 90, 36, 8, DEEP_BLUE);
  tft.drawRoundRect(20, CONTENT_Y + 150, 90, 36, 8, btnCol);
  tft.setTextSize(2); tft.setTextColor(btnCol);
  tft.setCursor(30, CONTENT_Y + 162);
  tft.print(swRunning ? "STOP" : "START");
  tft.setTextSize(1);

  // RESET button
  tft.fillRoundRect(130, CONTENT_Y + 150, 90, 36, 8, DEEP_BLUE);
  tft.drawRoundRect(130, CONTENT_Y + 150, 90, 36, 8, ORANGE);
  tft.setTextSize(2); tft.setTextColor(ORANGE);
  tft.setCursor(140, CONTENT_Y + 162);
  tft.print("RESET");
  tft.setTextSize(1);

  // force updateClockPage1 to always draw on full page redraw
  swForceRedraw = true;
  updateClockPage1();
}

void updateClockPage1() {
  static uint32_t lastDrawnSec = 0xFFFFFFFF;

  uint32_t elapsed = swRunning ? swAccumMs + (millis() - swStartMs) : swAccumMs;

  uint32_t totalSec = elapsed / 1000;

  // skip only if running and second unchanged and no force flag
  if (swRunning && totalSec == lastDrawnSec && !swForceRedraw) return;
  swForceRedraw = false;
  lastDrawnSec = totalSec;

  uint8_t h = totalSec / 3600;
  uint8_t m = totalSec / 60 % 60;
  uint8_t s = totalSec % 60;

  tft.fillRect(14, CONTENT_Y + 62, SCREEN_W - 28, 48, DEEP_BLUE);
  tft.setFont(NULL); tft.setTextSize(3);
  tft.setTextColor(COL_ACCENT_CLOCK);
  char buf[10];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  int16_t tw = strlen(buf) * 18;
  tft.setCursor((SCREEN_W - tw) / 2, CONTENT_Y + 78);
  tft.print(buf);
  tft.setTextSize(1);
}

void handleClockPage1Touch(int tx, int ty) {
  if (touchInRect(tx, ty, 20, CONTENT_Y + 150, 90, 36)) {
    if (swRunning) {
      swAccumMs += millis() - swStartMs;
      swRunning  = false;
    } else {
      swStartMs = millis();
      swRunning = true;
    }
    drawClockPage1();  // full redraw  button + time, no stale static
    return;
  }
  if (touchInRect(tx, ty, 130, CONTENT_Y + 150, 90, 36)) {
    swRunning  = false;
    swAccumMs  = 0;
    swStartMs  = 0;
    drawClockPage1();
    return;
  }
}

// PAGE 2  ALARM

#define AL_ROW_H     75        // tall rows  plenty of room for buttons
#define AL_ROW_Y     (CONTENT_Y + 48)   // below header text

void drawAlarmRow(uint8_t i) {
  int16_t y   = AL_ROW_Y + i * AL_ROW_H;
  bool    sel = (i == alarmEditing);
  bool    en  = alarms[i].enabled;

  uint16_t bg     = sel ? 0x1082 : DEEP_BLUE;
  uint16_t border = en  ? COL_ACCENT_CLOCK : SLATE_BLUE;

  // card  74px tall with 6px gap before next row
  tft.fillRoundRect(6, y, SCREEN_W - 12, 70, 10, bg);
  tft.drawRoundRect(6, y, SCREEN_W - 12, 70, 10, border);

  // time text  large, left side
  char tbuf[8];
  sprintf(tbuf, "%02d:%02d", alarms[i].hour, alarms[i].minute);
  tft.setFont(NULL);
   tft.setTextSize(3);
  tft.setTextColor(en ? COL_ACCENT_CLOCK : DARKGRAY);
  tft.setCursor(12, y + 22);
  tft.print(tbuf);
  tft.setTextSize(1);

  // ON/OFF toggle  right side, vertically centred
  uint16_t togCol = en ? LIME_GREEN : DARKGRAY;
  tft.fillRoundRect(SCREEN_W - 56, y + 22, 44, 30, 8, togCol);
  tft.setTextColor(WHITE);
   tft.setTextSize(1);
  tft.setCursor(SCREEN_W - 50, y + 28);
  tft.setTextSize(2);
  tft.print(en ? "ON" : "OFF");

  // +/- nudge buttons  only when selected
  if (sel) {
    // layout: hour buttons at x=118, min buttons at x=158
    // + buttons on top row (y+6), - buttons on bottom row (y+42)
    // each button 34px wide, 24px tall

    // HOUR label
    tft.setTextColor(STEEL_GRAY); tft.setTextSize(1);
    tft.setCursor(122, y + 4);
    tft.print("HR");

    // hour +
    tft.fillRoundRect(114, y + 14, 28, 21, 5, DEEP_BLUE);
    tft.drawRoundRect(114, y + 14, 28, 21, 5, COL_ACCENT_CLOCK);
    tft.setTextColor(COL_ACCENT_CLOCK); tft.setTextSize(2);
    tft.setCursor(123, y + 17);
    tft.print("+");

    // hour -
    tft.fillRoundRect(114, y + 40, 28, 21, 5, DEEP_BLUE);
    tft.drawRoundRect(114, y + 40, 28, 21, 5, ORANGE);
    tft.setTextColor(ORANGE); tft.setTextSize(2);
    tft.setCursor(123, y + 42);
    tft.print("-");

    // MIN label
    tft.setTextColor(STEEL_GRAY); tft.setTextSize(1);
    tft.setCursor(151, y + 4);
    tft.print("MIN");

    // min +
    tft.fillRoundRect(145, y + 14, 28, 21, 5, DEEP_BLUE);
    tft.drawRoundRect(145, y + 14, 28, 21, 5, COL_ACCENT_CLOCK);
    tft.setTextColor(COL_ACCENT_CLOCK); tft.setTextSize(2);
    tft.setCursor(154, y + 17);
    tft.print("+");

    // min -
    tft.fillRoundRect(145, y + 40, 28, 21, 5, DEEP_BLUE);
    tft.drawRoundRect(145, y + 40, 28, 21, 5, ORANGE);
    tft.setTextColor(ORANGE); tft.setTextSize(2);
    tft.setCursor(154, y + 44);
    tft.print("-");

    tft.setTextSize(1);
  }
}

void drawClockPage2() {
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(10, CONTENT_Y + 33);
  tft.print("Tap to select. Adjust HR/MIN.");

  for (uint8_t i = 0; i < MAX_ALARMS; i++) drawAlarmRow(i);

  if (alarmBuzzing) {
    tft.fillRoundRect(20, CLK_BTN_Y - 28, SCREEN_W - 40, 22, 6, RED);
    tft.setTextColor(WHITE); tft.setTextSize(1);
    tft.setCursor(38, CLK_BTN_Y - 21);
    tft.print("!! STOP ALARM !!");
  }
}

void handleClockPage2Touch(int tx, int ty) {
  if (alarmBuzzing &&
      touchInRect(tx, ty, 20, CLK_BTN_Y - 28, SCREEN_W - 40, 22)) {
    alarmBuzzing = false;
    noTone(BUZZER_PIN);
    drawClockPage2();
    return;
  }

  for (uint8_t i = 0; i < MAX_ALARMS; i++) {
    int16_t y = AL_ROW_Y + i * AL_ROW_H;

    if (touchInRect(tx, ty, 6, y, SCREEN_W - 12, 74)) {

      // ON/OFF toggle
      if (touchInRect(tx, ty, SCREEN_W - 56, y + 28, 44, 20)) {
        alarms[i].enabled   = !alarms[i].enabled;
        alarms[i].triggered = false;
        drawAlarmRow(i);
        return;
      }

      if (alarmEditing == i) {
        // hour +
        if (touchInRect(tx, ty, 114, y + 14, 34, 24)) {
          alarms[i].hour = (alarms[i].hour + 1) % 24;
          alarms[i].triggered = false; drawAlarmRow(i); return;
        }
        // hour -
        if (touchInRect(tx, ty, 114, y + 42, 34, 24)) {
          alarms[i].hour = (alarms[i].hour + 23) % 24;
          alarms[i].triggered = false; drawAlarmRow(i); return;
        }
        // min +
        if (touchInRect(tx, ty, 154, y + 14, 34, 24)) {
          alarms[i].minute = (alarms[i].minute + 1) % 60;
          alarms[i].triggered = false; drawAlarmRow(i); return;
        }
        // min -
        if (touchInRect(tx, ty, 154, y + 42, 34, 24)) {
          alarms[i].minute = (alarms[i].minute + 59) % 60;
          alarms[i].triggered = false; drawAlarmRow(i); return;
        }
      }

      // select this row
      uint8_t prev = alarmEditing;
      alarmEditing = i;
      drawAlarmRow(prev);
      drawAlarmRow(i);
      return;
    }
  }
}

// MASTER TOUCH HANDLER FOR CLOCK

void handleClockTouch(int tx, int ty) {
  if (clockPage < 2 && touchInRect(tx, ty, CLK_NEXT_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H)) {
    clockPage++;
    prevHrX2  = prevHrY2  = -1;
    prevMinX2 = prevMinY2 = -1;
    prevSecX2 = prevSecY2 = -1;
    drawClockScreen();
    return;
  }
  if (clockPage > 0 && touchInRect(tx, ty, CLK_PREV_X, CLK_BTN_Y, CLK_BTN_W, CLK_BTN_H)) {
    clockPage--;
    prevHrX2  = prevHrY2  = -1;
    prevMinX2 = prevMinY2 = -1;
    prevSecX2 = prevSecY2 = -1;
    drawClockScreen();
    return;
  }
  switch (clockPage) {
    case 0: break;
    case 1: handleClockPage1Touch(tx, ty); break;
    case 2: handleClockPage2Touch(tx, ty); break;
  }
}

//NETWORK APP
   
//NET  STATUS BAR (inside screen, not top bar)
   
void drawNetStatusBar(const char* msg, uint16_t color) {
  tft.fillRect(0, SCREEN_H - BOTTOM_H - 20, SCREEN_W, 18, BLACK);
  tft.setFont(NULL); 
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.setCursor(8, SCREEN_H - BOTTOM_H - 14);
  tft.print(msg);
}

//NET  LIST ITEM DRAW
   
void drawNetListItem(uint8_t idx, int16_t y,const char* name, int8_t rssi,bool selected, bool connected) {
  uint16_t bg     = selected   ? 0x1082 : DEEP_BLUE;
  uint16_t border = connected  ? LIME_GREEN: selected   ? CYAN: SLATE_BLUE;

  tft.fillRoundRect(8, y, SCREEN_W - 16, 30, 5, bg);
  tft.drawRoundRect(8, y, SCREEN_W - 16, 30, 5, border);

  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(connected ? LIME_GREEN : WHITE);
  tft.setCursor(16, y + 6);

  // truncate name if too long 
  char truncName[20];
  strncpy(truncName, name, 19); truncName[19] = '\0';
  tft.print(truncName);

  if (connected) {
    tft.setTextColor(LIME_GREEN);
    tft.setCursor(SCREEN_W - 70, y + 11);
    tft.print("CONNECTED");
  } else if (rssi != 0) {
    // signal bars for WiFi 
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(SCREEN_W - 50, y + 6);
    if      (rssi > -60) tft.print("||||");
    else if (rssi > -70) tft.print("|||");
    else if (rssi > -80) tft.print("||");
    else                 tft.print("|");
    tft.setCursor(SCREEN_W - 60, y + 18);
    char rBuf[8]; sprintf(rBuf, "%ddBm", rssi);
    tft.print(rBuf);
  }
}

//NET HOME SCREEN  WiFi / Bluetooth / Internet
   
void drawNetScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(CYAN);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Network");
  tft.setFont(NULL);
  tft.drawFastHLine(10, CONTENT_Y + 32, SCREEN_W - 20, SLATE_BLUE);

  //WiFi Card
  drawCard(12, CONTENT_Y + 42, SCREEN_W - 24, 52, DEEP_BLUE, CYAN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(CYAN);
  tft.setCursor(62, CONTENT_Y + 65);
  tft.print("WiFi");
  tft.setFont(NULL);
  tft.setTextColor(wifiConnected ? LIME_GREEN : DARKGRAY);
  tft.setCursor(62, CONTENT_Y + 78);
  tft.print(wifiConnected ? wifiConnectedSSID : "Not connected");

  //WiFi icon
  uint16_t c = CYAN;
  tft.drawCircle(38, CONTENT_Y + 67, 14, c);
  tft.fillRect(23, CONTENT_Y + 67, 31, 16, DEEP_BLUE);
  tft.drawCircle(38, CONTENT_Y + 67, 9, c);
  tft.fillRect(28, CONTENT_Y + 67, 21, 11, DEEP_BLUE);
  tft.drawCircle(38, CONTENT_Y + 67, 5, c);
  tft.fillRect(32, CONTENT_Y + 67, 13, 7, DEEP_BLUE);
  tft.fillCircle(38, CONTENT_Y + 67, 2, c);
  tft.drawRoundRect(12, CONTENT_Y + 42, SCREEN_W - 24, 52, 8, CYAN);

  // arrow 
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(SCREEN_W - 30, CONTENT_Y + 64);
  tft.print(">");

  //Bluetooth Card 
  drawCard(12, CONTENT_Y + 104, SCREEN_W - 24, 52, DEEP_BLUE, MAGENTA);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(MAGENTA);
  tft.setCursor(62, CONTENT_Y + 127);
  tft.print("Bluetooth");
  tft.setFont(NULL);
  tft.setTextColor(btConnected ? LIME_GREEN : DARKGRAY);
  tft.setCursor(62, CONTENT_Y + 140);
  tft.print(btConnected ? btConnectedName : "Not paired");

  // Bluetooth icon 
  c = MAGENTA;
  tft.drawFastVLine(30,     CONTENT_Y + 114, 24, c);
  tft.drawFastVLine(31,     CONTENT_Y + 114, 24, c);
  tft.drawLine(30, CONTENT_Y + 114, 42, CONTENT_Y + 126, c);
  tft.drawLine(30, CONTENT_Y + 114, 43, CONTENT_Y + 126, c);
  tft.drawLine(42, CONTENT_Y + 126, 30, CONTENT_Y + 126, c);
  tft.drawLine(43, CONTENT_Y + 126, 30, CONTENT_Y + 126, c);
  tft.drawLine(30, CONTENT_Y + 126, 42, CONTENT_Y + 132, c);
  tft.drawLine(30, CONTENT_Y + 126, 43, CONTENT_Y + 132, c);
  tft.drawLine(42, CONTENT_Y + 132, 30, CONTENT_Y + 138, c);
  tft.drawLine(43, CONTENT_Y + 132, 30, CONTENT_Y + 138, c);

  // arrow
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(SCREEN_W - 30, CONTENT_Y + 126);
  tft.print(">");

  // Internet Card 
  drawCard(12, CONTENT_Y + 166, SCREEN_W - 24, 52, DEEP_BLUE, ORANGE);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ORANGE);
  tft.setCursor(62, CONTENT_Y + 190);
  tft.print("Internet");
  tft.setFont(NULL);
  tft.setTextColor(DARKGRAY);
  tft.setCursor(62, CONTENT_Y + 202);
  tft.print(wifiConnected ? "WiFi ready" : "Need WiFi first");

  //Globe icon
  c = ORANGE;
  tft.drawCircle(38, CONTENT_Y + 192, 13, c);
  tft.drawCircle(38, CONTENT_Y + 192, 12, c);
  tft.drawFastHLine(25, CONTENT_Y + 192, 27, c);
  tft.drawFastHLine(29, CONTENT_Y + 186, 18, c);
  tft.drawFastHLine(29, CONTENT_Y + 198, 18, c);
  tft.drawFastVLine(38, CONTENT_Y + 179, 27, c);
  tft.drawLine(38, CONTENT_Y + 179, 46, CONTENT_Y + 192, c);
  tft.drawLine(46, CONTENT_Y + 192, 38, CONTENT_Y + 205, c);
  tft.drawLine(38, CONTENT_Y + 179, 30, CONTENT_Y + 192, c);
  tft.drawLine(30, CONTENT_Y + 192, 38, CONTENT_Y + 205, c);
  tft.drawRoundRect(12, CONTENT_Y + 166, SCREEN_W - 24, 52, 8, ORANGE);

  // arrow
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(SCREEN_W - 30, CONTENT_Y + 188);
  tft.print(">");
}

void handleNetTouch(int tx, int ty) {
  if (touchInRect(tx, ty, 12, CONTENT_Y + 42, SCREEN_W - 24, 52)) {
    navigateTo(NET_WIFI);
    return;
  }
  if (touchInRect(tx, ty, 12, CONTENT_Y + 104, SCREEN_W - 24, 52)) {
    navigateTo(NET_BT);
    return;
  }
  if (touchInRect(tx, ty, 12, CONTENT_Y + 166, SCREEN_W - 24, 52)) {
    navigateTo(NET_INTERNET);
    return;
  }
}

//NET  WIFI SCREEN
   
void drawNetWifiScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(CYAN);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("WiFi");
  tft.setFont(NULL);

  tft.fillRoundRect(SCREEN_W - 75, CONTENT_Y+3, 66, 25, 4, DEEP_BLUE);
  tft.drawRoundRect(SCREEN_W - 75, CONTENT_Y+3, 66, 25, 4, CYAN);
  tft.setTextColor(CYAN); tft.setTextSize(1);
  tft.setCursor(SCREEN_W - 63, CONTENT_Y + 11);
  tft.print(wifiScanning ? "Scanning" : "Scan");

  tft.drawFastHLine(10, CONTENT_Y + 28, SCREEN_W - 20, SLATE_BLUE);

  if (wifiScanning) {
    tft.setTextColor(ORANGE);
    tft.setCursor(20, CONTENT_Y + 50);
    tft.print("Scanning for networks...");
    return;
  }

  if (wifiCount == 0) {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(20, CONTENT_Y + 60);
    tft.print("No networks found.");
    tft.setCursor(20, CONTENT_Y + 76);
    tft.print("Tap Scan to search.");
    return;
  }

  uint8_t totalPages = (wifiCount + NET_PAGE_SIZE - 1) / NET_PAGE_SIZE;
  if (wifiPage >= totalPages && totalPages > 0) wifiPage = totalPages - 1;

  uint8_t startIdx = wifiPage * NET_PAGE_SIZE;
  uint8_t endIdx   = min((uint8_t)(startIdx + NET_PAGE_SIZE), wifiCount);

  for (uint8_t i = startIdx; i < endIdx; i++) {
    int16_t y = CONTENT_Y + 34 + (i - startIdx) * 36;
    bool isSel  = (i == wifiSelected);
    bool isConn = (wifiConnected && strncmp(wifiList[i], wifiConnectedSSID, NET_ITEM_LEN) == 0);
    drawNetListItem(i, y, wifiList[i], wifiRSSI[i], isSel, isConn);
  }

  int16_t pageY = SCREEN_H - BOTTOM_H - 58;

  // < PREV arrow only
  if (wifiPage > 0) {
    tft.fillRoundRect(4, pageY, 26, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(4, pageY, 26, 22, 4, CYAN);
    tft.setTextColor(CYAN); tft.setTextSize(1);
    tft.setCursor(11, pageY + 8);
    tft.print("<");
  }

  // Connect
  if (wifiCount > 0) {
    tft.fillRoundRect(36, pageY, 66, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(36, pageY, 66, 22, 4, CYAN);
    tft.setTextColor(CYAN); tft.setTextSize(1);
    tft.setCursor(46, pageY + 8);
    tft.print("Connect");
  }

  // Disconnect
  if (wifiConnected) {
    tft.fillRoundRect(122, pageY, 76, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(122, pageY, 76, 22, 4, RED);
    tft.setTextColor(RED); tft.setTextSize(1);
    tft.setCursor(126, pageY + 8);
    tft.print("Disconnect");
  }

  // NEXT > arrow only
  if (wifiPage < totalPages - 1) {
    tft.fillRoundRect(210, pageY, 26, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(210, pageY, 26, 22, 4, CYAN);
    tft.setTextColor(CYAN); tft.setTextSize(1);
    tft.setCursor(217, pageY + 8);
    tft.print(">");
  }

  drawNetStatusBar(netStatusMsg, wifiConnected ? LIME_GREEN : netWaiting ? ORANGE : STEEL_GRAY);
}

void handleNetWifiTouch(int tx, int ty) {

  if (touchInRect(tx, ty, SCREEN_W - 75, CONTENT_Y + 3, 66, 25)) {
    wifiScanning = true;
    wifiCount    = 0;
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Scanning...");
    sendsignal("ESP1:NET:WIFI:SCAN");
    drawNetWifiScreen();
    return;
  }

  int16_t pageY = SCREEN_H - BOTTOM_H - 58;
  uint8_t totalPages = (wifiCount + NET_PAGE_SIZE - 1) / NET_PAGE_SIZE;

  if (wifiPage > 0 && touchInRect(tx, ty, 4, pageY, 26, 22)) {
    wifiPage--;
    drawNetWifiScreen();
    return;
  }
  // connect button 
  if (wifiCount > 0 && touchInRect(tx, ty, 36, pageY, 66, 22)) {
    strncpy(wifiPendingSSID, wifiList[wifiSelected], NET_ITEM_LEN);
    wifiAwaitingPass = true;
    Serial.print(F("Enter WiFi password for: "));
    startKeyboard("WiFi Password", true, KB_WIFI_PASS);
    Serial.println(wifiPendingSSID);
    return;
  }
  
  //disconnect button
  if (wifiConnected && touchInRect(tx, ty, 122, pageY, 76, 22)) {
    sendsignal("ESP1:NET:WIFI:DISCONNECT");
    wifiConnected = false;
    wifiConnectedSSID[0] = '\0';
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Disconnected");
    drawNetWifiScreen();
    return;
  }

  if (wifiPage < totalPages - 1 && touchInRect(tx, ty, 210, pageY, 26, 22)) {
    wifiPage++;
    drawNetWifiScreen();
    return;
  }

  uint8_t startIdx = wifiPage * NET_PAGE_SIZE;
  uint8_t endIdx   = min((uint8_t)(startIdx + NET_PAGE_SIZE), wifiCount);
  for (uint8_t i = startIdx; i < endIdx; i++) {
    int16_t y = CONTENT_Y + 34 + (i - startIdx) * 36;
    if (touchInRect(tx, ty, 8, y, SCREEN_W - 16, 30)) {
      wifiSelected = i;
      drawNetWifiScreen();
      return;
    }
  }
}

void drawNetBTScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(MAGENTA);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Bluetooth BLE");
  tft.setFont(NULL);

  tft.fillRoundRect(SCREEN_W - 75, CONTENT_Y+3, 66, 25, 4, DEEP_BLUE);
  tft.drawRoundRect(SCREEN_W - 75, CONTENT_Y+3, 66, 25, 4, MAGENTA);
  tft.setTextColor(MAGENTA); tft.setTextSize(1);
  tft.setCursor(SCREEN_W - 63, CONTENT_Y + 11);
  tft.print(btScanning ? "Scanning" : "Scan");

  tft.drawFastHLine(10, CONTENT_Y + 28, SCREEN_W - 20, SLATE_BLUE);

  if (btScanning) {
    tft.setTextColor(ORANGE);
    tft.setCursor(20, CONTENT_Y + 50);
    tft.print("Scanning for BLE devices...");
    return;
  }

  if (btCount == 0) {
    tft.setTextColor(DARKGRAY);
    tft.setCursor(20, CONTENT_Y + 60);
    tft.print("No BLE devices found.");
    tft.setCursor(20, CONTENT_Y + 76);
    tft.print("Tap Scan to search.");
    return;
  }

  uint8_t totalPages = (btCount + NET_PAGE_SIZE - 1) / NET_PAGE_SIZE;
  if (btPage >= totalPages && totalPages > 0) btPage = totalPages - 1;

  uint8_t startIdx = btPage * NET_PAGE_SIZE;
  uint8_t endIdx   = min((uint8_t)(startIdx + NET_PAGE_SIZE), btCount);

  for (uint8_t i = startIdx; i < endIdx; i++) {
    int16_t y = CONTENT_Y + 34 + (i - startIdx) * 36;
    bool isSel  = (i == btSelected);
    bool isConn = (btConnected && strncmp(btList[i], btConnectedName, NET_ITEM_LEN) == 0);
    drawNetListItem(i, y, btList[i], 0, isSel, isConn);
  }

  int16_t pageY = SCREEN_H - BOTTOM_H - 58;

  // < arrow only
  if (btPage > 0) {
    tft.fillRoundRect(4, pageY, 26, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(4, pageY, 26, 22, 4, MAGENTA);
    tft.setTextColor(MAGENTA); tft.setTextSize(1);
    tft.setCursor(11, pageY + 8);
    tft.print("<");
  }

  // Pair
  if (btCount > 0) {
    tft.fillRoundRect(36, pageY, 66, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(36, pageY, 66, 22, 4, MAGENTA);
    tft.setTextColor(MAGENTA); tft.setTextSize(1);
    tft.setCursor(56, pageY + 8);
    tft.print("Pair");
  }

  // Unpair
  if (btConnected) {
    tft.fillRoundRect(122, pageY, 76, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(122, pageY, 76, 22, 4, RED);
    tft.setTextColor(RED); tft.setTextSize(1);
    tft.setCursor(136, pageY + 8);
    tft.print("Unpair");
  }

  // > arrow only
  if (btPage < totalPages - 1) {
    tft.fillRoundRect(210, pageY, 26, 22, 4, DEEP_BLUE);
    tft.drawRoundRect(210, pageY, 26, 22, 4, MAGENTA);
    tft.setTextColor(MAGENTA); tft.setTextSize(1);
    tft.setCursor(217, pageY + 8);
    tft.print(">");
  }

  drawNetStatusBar(netStatusMsg, btConnected ? LIME_GREEN : netWaiting ? ORANGE : STEEL_GRAY);
}

void handleNetBTTouch(int tx, int ty) {

  if (touchInRect(tx, ty, SCREEN_W - 75, CONTENT_Y + 3, 66, 25)) {
    btScanning = true;
    btCount    = 0;
    snprintf(netStatusMsg, sizeof(netStatusMsg), "BLE scanning...");
    sendsignal("ESP1:NET:BT:SCAN");
    drawNetBTScreen();
    return;
  }

  int16_t pageY = SCREEN_H - BOTTOM_H - 58;
  uint8_t totalPages = (btCount + NET_PAGE_SIZE - 1) / NET_PAGE_SIZE;

  if (btPage > 0 && touchInRect(tx, ty, 4, pageY, 26, 22)) {
    btPage--;
    drawNetBTScreen();
    return;
  }

  if (btCount > 0 && touchInRect(tx, ty, 36, pageY, 66, 22)) {
    char packet[64];
    snprintf(packet, sizeof(packet), "ESP1:NET:BT:CONNECT:%s", btList[btSelected]);
    sendsignal(packet);
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Pairing with %s...", btList[btSelected]);
    netWaiting = true;
    drawNetStatusBar(netStatusMsg, ORANGE);
    return;
  }

  if (btConnected && touchInRect(tx, ty, 122, pageY, 76, 22)) {
    sendsignal("ESP1:NET:BT:DISCONNECT");
    btConnected = false;
    btConnectedName[0] = '\0';
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Unpaired");
    drawNetBTScreen();
    return;
  }

  if (btPage < totalPages - 1 && touchInRect(tx, ty, 210, pageY, 26, 22)) {
    btPage++;
    drawNetBTScreen();
    return;
  }

  uint8_t startIdx = btPage * NET_PAGE_SIZE;
  uint8_t endIdx   = min((uint8_t)(startIdx + NET_PAGE_SIZE), btCount);
  for (uint8_t i = startIdx; i < endIdx; i++) {
    int16_t y = CONTENT_Y + 34 + (i - startIdx) * 36;
    if (touchInRect(tx, ty, 8, y, SCREEN_W - 16, 30)) {
      btSelected = i;
      drawNetBTScreen();
      return;
    }
  }
}

//NET  INTERNET SCREEN
   
void drawNetInternetScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ORANGE);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Internet");
  tft.setFont(NULL);
  tft.drawFastHLine(10, CONTENT_Y + 26, SCREEN_W - 20, SLATE_BLUE);

  if (!wifiConnected) {
    drawCard(12, CONTENT_Y + 40, SCREEN_W - 24, 60, DEEP_BLUE, ORANGE);
    tft.setTextColor(ORANGE);
    tft.setCursor(22, CONTENT_Y + 58);
    tft.print("No WiFi connection.");
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(22, CONTENT_Y + 74);
    tft.print("Connect via WiFi first.");
    return;
  }

  // connected  show WiFi info 
  tft.setTextColor(LIME_GREEN);
  tft.setCursor(12, CONTENT_Y + 36);
  tft.print("Connected: ");
  tft.print(wifiConnectedSSID);

  // IP address (filled by protocol) 
  drawCard(12, CONTENT_Y + 50, SCREEN_W - 24, 38, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(20, CONTENT_Y + 60);
  tft.print("IP: ");
  tft.setTextColor(WHITE);
  tft.print(netStatusMsg);

  // HTTP GET button 
  drawCard(12, CONTENT_Y + 98, SCREEN_W - 24, 42, DEEP_BLUE, ORANGE);
  tft.setTextColor(ORANGE);
  tft.setCursor(22, CONTENT_Y + 114);
  tft.print("HTTP GET");
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(22, CONTENT_Y + 128);
  tft.print("Type URL in Serial");

  // response display 
  drawCard(12, CONTENT_Y + 150, SCREEN_W - 24, 80, DEEP_BLUE, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(20, CONTENT_Y + 160);
  tft.print("Response:");
  tft.setTextColor(WHITE);
  // word wrap simple version  just print up to 3 lines 
  char line[28];
  uint8_t len = strlen(httpResponse);
  for (uint8_t l = 0; l < 3 && l * 27 < len; l++) {
    strncpy(line, httpResponse + l * 27, 27);
    line[27] = '\0';
    tft.setCursor(20, CONTENT_Y + 172 + l * 14);
    tft.print(line);
  }

  // ping button 
  drawCard(12, CONTENT_Y + 238, SCREEN_W - 24, 30, DEEP_BLUE, CYAN);
  tft.setTextColor(CYAN);
  tft.setCursor(22, CONTENT_Y + 250);
  tft.print("Ping 8.8.8.8");
}

void handleNetInternetTouch(int tx, int ty) {
  if (!wifiConnected) return;

  // HTTP GET 
  if (touchInRect(tx, ty, 12, CONTENT_Y + 98, SCREEN_W - 24, 42)) {
    httpWaiting = true;
    Serial.println(F("Enter URL (e.g. http://192.168.1.1):"));
    return;
  }

  // Ping 
  if (touchInRect(tx, ty, 12, CONTENT_Y + 238, SCREEN_W - 24, 30)) {
    sendsignal("ESP1:NET:HTTP:PING:8.8.8.8");
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Pinging...");
    drawNetInternetScreen();
    return;
  }
}

//NET  PROTOCOL HANDLER NET COMMANDS 
void handleNetProtocol(const String& msg) {
  // NET:WIFI:SCAN:RESULT:count:ssid1,rssi1;ssid2,rssi2;... 
  if (msg.startsWith("NET:WIFI:SCAN:RESULT:")) {
    wifiScanning = false;
    wifiCount = 0;
    String payload = msg.substring(21);
    int colonIdx = payload.indexOf(':');
    if (colonIdx < 0) { drawNetWifiScreen(); return; }
    uint8_t count = payload.substring(0, colonIdx).toInt();
    String items  = payload.substring(colonIdx + 1);
    for (uint8_t i = 0; i < count && i < MAX_NET_ITEMS; i++) {
      int semi = items.indexOf(';');
      String item = (semi >= 0) ? items.substring(0, semi) : items;
      int comma = item.indexOf(',');
      if (comma > 0) {
        strncpy(wifiList[i], item.substring(0, comma).c_str(), NET_ITEM_LEN - 1);
        wifiRSSI[i] = item.substring(comma + 1).toInt();
      } else {
        strncpy(wifiList[i], item.c_str(), NET_ITEM_LEN - 1);
        wifiRSSI[i] = -99;
      }
      wifiCount++;
      if (semi >= 0) items = items.substring(semi + 1);
    }
    snprintf(netStatusMsg, sizeof(netStatusMsg),"%d network(s) found", wifiCount);
    netWaiting = false;
    if (currentMode == NET_WIFI) drawNetWifiScreen();
    return;
  }

  // NET:WIFI:CONNECTED:ssid:ip 
  if (msg.startsWith("NET:WIFI:CONNECTED:")) {
    String payload = msg.substring(19);
    int colon = payload.indexOf(':');
    if (colon > 0) {
      strncpy(wifiConnectedSSID, payload.substring(0, colon).c_str(), NET_ITEM_LEN - 1);
      snprintf(netStatusMsg, sizeof(netStatusMsg), "%s",payload.substring(colon + 1).c_str());
    } else {
      strncpy(wifiConnectedSSID, payload.c_str(), NET_ITEM_LEN - 1);
    }
    wifiConnected = true;
    netWaiting    = false;
    wifiAwaitingPass = false;
    if (currentMode == NET_WIFI || currentMode == NET) drawNetWifiScreen();
    Serial.print(F("WiFi connected: ")); Serial.println(wifiConnectedSSID);
    return;
  }

  // NET:WIFI:FAIL:reason 
  if (msg.startsWith("NET:WIFI:FAIL:")) {
    wifiConnected = false;
    netWaiting    = false;
    wifiAwaitingPass = false;
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Failed: %s",msg.substring(14).c_str());
    if (currentMode == NET_WIFI) drawNetStatusBar(netStatusMsg, RED);
    return;
  }

  // NET:WIFI:DISCONNECTED 
  if (msg == "NET:WIFI:DISCONNECTED") {
    wifiConnected = false;
    wifiConnectedSSID[0] = '\0';
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Disconnected");
    if (currentMode == NET_WIFI) drawNetWifiScreen();
    return;
  }

  // NET:BT:SCAN:RESULT:count:name1;name2;... 
  if (msg.startsWith("NET:BT:SCAN:RESULT:")) {
    btScanning = false;
    btCount    = 0;
    String payload = msg.substring(19);
    int colonIdx = payload.indexOf(':');
    if (colonIdx < 0) { drawNetBTScreen(); return; }
    uint8_t count = payload.substring(0, colonIdx).toInt();
    String items  = payload.substring(colonIdx + 1);
    for (uint8_t i = 0; i < count && i < MAX_NET_ITEMS; i++) {
      int semi = items.indexOf(';');
      String item = (semi >= 0) ? items.substring(0, semi) : items;
      strncpy(btList[i], item.c_str(), NET_ITEM_LEN - 1);
      btCount++;
      if (semi >= 0) items = items.substring(semi + 1);
    }
    snprintf(netStatusMsg, sizeof(netStatusMsg),"%d BLE device(s) found", btCount);
    netWaiting = false;
    if (currentMode == NET_BT) drawNetBTScreen();
    return;
  }

  // NET:BT:CONNECTED:name 
  if (msg.startsWith("NET:BT:CONNECTED:")) {
    strncpy(btConnectedName, msg.substring(17).c_str(), NET_ITEM_LEN - 1);
    btConnected = true;
    netWaiting  = false;
    snprintf(netStatusMsg, sizeof(netStatusMsg),"Paired: %s", btConnectedName);
    if (currentMode == NET_BT) drawNetBTScreen();
    return;
  }

  // NET:BT:FAIL:reason 
  if (msg.startsWith("NET:BT:FAIL:")) {
    btConnected = false;
    netWaiting  = false;
    snprintf(netStatusMsg, sizeof(netStatusMsg), "BT Fail: %s",msg.substring(12).c_str());
    if (currentMode == NET_BT) drawNetStatusBar(netStatusMsg, RED);
    return;
  }

  // NET:HTTP:RESPONSE:data 
  if (msg.startsWith("NET:HTTP:RESPONSE:")) {
    strncpy(httpResponse, msg.substring(18).c_str(), 127);
    httpWaiting = false;
    if (currentMode == NET_INTERNET) drawNetInternetScreen();
    return;
  }

  // NET:HTTP:PING:result 
  if (msg.startsWith("NET:HTTP:PING:")) {
    snprintf(netStatusMsg, sizeof(netStatusMsg), "Ping: %s",msg.substring(14).c_str());
    if (currentMode == NET_INTERNET) drawNetInternetScreen();
    return;
  }
}

// DRAW

// toolbar
void drawToolbar() {
  tft.fillRect(0, CONTENT_Y, SCREEN_W - 34, TOOLBAR_H, DEEP_BLUE);
  tft.drawFastHLine(0, CONTENT_Y + TOOLBAR_H, SCREEN_W-34, SLATE_BLUE);

  for (uint8_t i = 0; i < SWATCH_COUNT; i++) {
    int16_t sx = swatches[i].x;
    int16_t sy = CONTENT_Y + (TOOLBAR_H - SWATCH_SIZE) / 2;
    tft.fillRoundRect(sx, sy, SWATCH_SIZE, SWATCH_SIZE, 4, swatches[i].color);
    if (swatches[i].color == drawColor)
      tft.drawRoundRect(sx - 1, sy - 1, SWATCH_SIZE + 2, SWATCH_SIZE + 2, 5, WHITE);
    else
      tft.drawRoundRect(sx, sy, SWATCH_SIZE, SWATCH_SIZE, 4, DARKGRAY);
  }

  tft.fillRoundRect(ERASE_BTN_X, ERASE_BTN_Y, ERASE_BTN_W, ERASE_BTN_H, 4, RED);
  tft.drawRoundRect(ERASE_BTN_X, ERASE_BTN_Y, ERASE_BTN_W, ERASE_BTN_H, 4, WHITE);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(ERASE_BTN_X + 6, ERASE_BTN_Y + 7);
  tft.print("CLR");
}

void hideToolbar() {
  tft.fillRect(0, CONTENT_Y, SCREEN_W - 34, TOOLBAR_H + 1, BLACK);
  tft.fillRect(ERASE_BTN_X, ERASE_BTN_Y, ERASE_BTN_W, ERASE_BTN_H, BLACK);
}

void drawDotButton() {
  tft.fillRoundRect(DOT_BTN_X, DOT_BTN_Y, DOT_BTN_W, DOT_BTN_H, 4, DEEP_BLUE);
  tft.drawRoundRect(DOT_BTN_X, DOT_BTN_Y, DOT_BTN_W, DOT_BTN_H, 4, SLATE_BLUE);
  int cx = DOT_BTN_X + DOT_BTN_W / 2;
  int cy = DOT_BTN_Y + DOT_BTN_H / 2;
  for (int8_t i = -1; i <= 1; i++)
    tft.fillCircle(cx, cy + i * 5, 2, WHITE);
}

void drawDrawScreen() {
  tft.fillRect(0, CONTENT_Y, SCREEN_W, SCREEN_H - CONTENT_Y - BOTTOM_H, BLACK);
  if (toolbarVisible) drawToolbar();
  drawDotButton();
}

void handleDrawTouch(int tx, int ty) {
  if (touchInRect(tx-3, ty-4, DOT_BTN_X-10, DOT_BTN_Y-10, DOT_BTN_W+15, DOT_BTN_H+15)) {
    if (toolbarVisible) hideToolbar();
    else drawToolbar();
    drawDotButton();
    toolbarVisible = !toolbarVisible;
    return;
  }

  if (toolbarVisible) {
    int16_t sy = CONTENT_Y + (TOOLBAR_H - SWATCH_SIZE) / 2;

    for (uint8_t i = 0; i < SWATCH_COUNT; i++) {
      if (touchInRect(tx, ty, swatches[i].x, sy, SWATCH_SIZE, SWATCH_SIZE)) {
        drawColor = swatches[i].color;
        drawToolbar();
        return;
      }
    }

    if (touchInRect(tx, ty, ERASE_BTN_X, ERASE_BTN_Y, ERASE_BTN_W, ERASE_BTN_H)) {
      int16_t canvasTop = CONTENT_Y + TOOLBAR_H + 1;
      int16_t canvasBot = SCREEN_H - BOTTOM_H;
      tft.fillRect(0, canvasTop, SCREEN_W, canvasBot - canvasTop, BLACK);
      return;
    }
  }

  int16_t canvasTop = toolbarVisible ? CONTENT_Y + TOOLBAR_H + 3 : CONTENT_Y+3;
  int16_t canvasBot = SCREEN_H - BOTTOM_H-3;
  if (ty > canvasTop && ty < canvasBot)
    tft.fillCircle(tx, ty, 3, drawColor);
}

// CHAT

void addMessage(const char* txt, bool sent) {
  if (chatCount < MAX_MSGS) {
    strncpy(chatLog[chatCount].text, txt, MAX_MSG_LEN);
    chatLog[chatCount].text[MAX_MSG_LEN] = '\0';
    chatLog[chatCount].sent = sent;
    chatCount++;
  } else {
    for (uint8_t i = 1; i < MAX_MSGS; i++)
      chatLog[i - 1] = chatLog[i];
    strncpy(chatLog[MAX_MSGS - 1].text, txt, MAX_MSG_LEN);
    chatLog[MAX_MSGS - 1].text[MAX_MSG_LEN] = '\0';
    chatLog[MAX_MSGS - 1].sent = sent;
  }
  if (currentMode == CHAT) redrawChatBubbles();
}

// compute bubble height for a given text
uint8_t bubbleHeight(const char* txt) {
  uint8_t len   = strlen(txt);
  uint8_t lines = (len + BUBBLE_CHARS_PER_LINE - 1) / BUBBLE_CHARS_PER_LINE;
  if (lines < 1) lines = 1;
  return BUBBLE_PAD_Y * 2 + lines * BUBBLE_LINE_H;
}

// CHAT  DRAW ONE BUBBLE
void drawOneBubble(uint8_t idx, int16_t y) {
  const char* txt  = chatLog[idx].text;
  bool        sent = chatLog[idx].sent;

  uint8_t  bh   = bubbleHeight(txt);
  if (y < CHAT_AREA_Y || y + bh > SCREEN_H - BOTTOM_H) return;

  uint8_t  cpl  = BUBBLE_CHARS_PER_LINE;
  uint8_t  len  = strlen(txt);
  uint8_t  lines = (len + cpl - 1) / cpl;
  if (lines < 1) lines = 1;

  int16_t bubW = BUBBLE_MAX_W;
  // for single short line, shrink width to fit
  if (lines == 1) {
    int16_t txtW = len * 6 + 2 * BUBBLE_PAD_X;
    if (txtW < bubW) bubW = txtW;
  }

  int16_t bubX = sent ? SCREEN_W - BUBBLE_MARGIN - bubW : BUBBLE_MARGIN;

  uint16_t bgCol  = sent ? DARK_TEAL : MED_GREEN;
  uint16_t txtCol = sent ? WHITE    : BLACK;

  // bubble body
  tft.fillRoundRect(bubX, y, bubW, bh, 6, bgCol);

  // tail
  if (sent) {
    tft.fillTriangle(
      bubX + bubW,     y + bh - 10,
      bubX + bubW + 6, y + bh - 4,
      bubX + bubW,     y + bh - 4,
      bgCol);
  } else {
    tft.fillTriangle(
      bubX,     y + bh - 10,
      bubX - 6, y + bh - 4,
      bubX,     y + bh - 4,
      bgCol);
  }

  // text  word wrap by character chunks
  tft.setFont(NULL); tft.setTextSize(1);
  tft.setTextColor(txtCol);
  for (uint8_t l = 0; l < lines; l++) {
    char lineBuf[BUBBLE_CHARS_PER_LINE + 1];
    uint8_t start = l * cpl;
    uint8_t count = len - start;
    if (count > cpl) count = cpl;
    strncpy(lineBuf, txt + start, count);
    lineBuf[count] = '\0';
    tft.setCursor(bubX + BUBBLE_PAD_X, y + BUBBLE_PAD_Y + l * BUBBLE_LINE_H);
    tft.print(lineBuf);
  }
}

// CHAT  REDRAW ALL BUBBLES
void redrawChatBubbles() {
  int16_t chatAreaBot = SCREEN_H - BOTTOM_H;
  tft.fillRect(0, CHAT_AREA_Y, SCREEN_W, chatAreaBot - CHAT_AREA_Y, BLACK);

  if (chatCount == 0) {
    tft.setFont(NULL);
    tft.setTextColor(DARKGRAY);
    tft.setCursor(30, SCREEN_H / 2 - 10);
    tft.print("No messages yet");
    tft.setCursor(10, SCREEN_H / 2 + 8);
    tft.print("Type in Serial Monitor");
    return;
  }

  // calculate total height needed working backwards from newest msg
  // find how many messages fit from bottom up
  int16_t availH   = chatAreaBot - CHAT_AREA_Y - 8;
  int16_t usedH    = 0;
  uint8_t firstIdx = chatCount;  // will decrement

  for (int8_t i = chatCount - 1; i >= 0; i--) {
    uint8_t bh = bubbleHeight(chatLog[i].text);
    int16_t needed = bh + BUBBLE_GAP;
    if (usedH + needed > availH) break;
    usedH += needed;
    firstIdx = i;
  }

  // draw from firstIdx downward, stacking from top of used area
  int16_t y = chatAreaBot - usedH;
  if (y < CHAT_AREA_Y + 4) y = CHAT_AREA_Y + 4;

  for (uint8_t i = firstIdx; i < chatCount; i++) {
    uint8_t bh = bubbleHeight(chatLog[i].text);
    drawOneBubble(i, y);
    y += bh + BUBBLE_GAP;
  }
}

// CHAT SCREEN

void drawChatScreen() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(MAUVE);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Chat Inbox");
  tft.setFont(NULL);

  // Type button  top right, next to header
  drawCard(SCREEN_W - 64, CONTENT_Y + 6, 56, 20, DEEP_BLUE, MAUVE);
  tft.setTextColor(MAUVE);
  tft.setCursor(SCREEN_W - 54, CONTENT_Y + 12);
  tft.print("Type");

  // status text 
  tft.setTextColor(DARKGRAY);
  tft.setCursor(10, CONTENT_Y + 28);
  tft.drawFastHLine(0, CONTENT_Y + 30, SCREEN_W, SLATE_BLUE);

  redrawChatBubbles();
}

// Send all kinds of signal over UART to ESP1 
void sendsignal(const String& msg) {
  espSerial.println("MEGA:"+msg);
  Serial.println("MEGA:"+msg);
}

//   PROTOCOL HANDLER  called for every incoming line
void applyTTTOnlineMove(uint8_t cell, char symbol) {
  if (cell > 8 || tttBoard[cell] != 0) return;
  tttBoard[cell] = (symbol == 'X') ? 1 : -1;
  if (currentMode == TICTACTOE) drawTTTCell(cell);

  tttWinner = checkTTTWinner();
  if (tttWinner != 0) {
    tttOver = true;
    if (tttWinner == (tttP1Symbol == 'X' ? 1 : -1))  tttScoreP1++;
    else if (tttWinner != 2) tttScoreP2++;
    if (currentMode == TICTACTOE) {
      drawTTTScores();
      drawTTTStatus();
      if (tttWinner != 2) drawTTTWinLine();
    }
    tttHandleGameEnd();   // schedule auto-reset
  } else {
    tttP1Turn = !tttP1Turn;
    if (currentMode == TICTACTOE) {
      drawTTTStatus();
      if (tttOnline) drawTTTOnlineStatus();
    }
  }
}

//handling incoming protocols
void handleProtocol(const char* raw) {
  Serial.println(raw);
  String full = String(raw);
  int firstColon  = full.indexOf(':');
  int secondColon = full.indexOf(':', firstColon + 1);
  if (secondColon < 0) return; 
  String msg = full.substring(secondColon + 1);

  //NET 
  if (msg.startsWith("NET:")) {
    handleNetProtocol(msg);
    return;
  }

  // CHAT incoming chat msg
  if (msg.startsWith("CHAT:")) {
    String text = msg.substring(5);
    addMessage(text.c_str(), false);  // left bubble
    return;
  }

  // TTT
  if (msg.startsWith("TTT:")) {
    String payload = msg.substring(4);

    // TTT:REQ  opponent requesting game
    if (payload == "REQ") {
      if ( tttOnlineState == ONLINE_IDLE) {
        tttOnlineState = ONLINE_RECEIVING;
        if(currentMode == TTT_ONLINE_REQUEST){
          drawTTTOnlineRequest();
        }
      }
      return;
    }

    // TTT:REQ:ACCEPT  opponent accepted our request
    if (payload == "REQ:ACCEPT") {
      if (tttOnlineState == ONLINE_REQUESTING) {
        tttOnlineState = ONLINE_IN_GAME;
        // go to symbol pick
        tttSeriesP1 = 0;
        tttThisGamePicker = 1;
        tttP1Symbol = ' ';
        tttP2Symbol = ' ';
        tttMySymbol = ' ';
        navigateTo(TTT_SYMBOL_PICK);
      }
      return;
    }

    // TTT:REQ:DECLINE  opponent declined
    if (payload == "REQ:DECLINE") {
      tttOnlineState = ONLINE_IDLE;
      if (currentMode == TTT_ONLINE_REQUEST) drawTTTOnlineRequest();
      return;
    }

    // TTT:LEFT  opponent left game or cancelled
    if (payload == "LEFT") {
      // handle LEFT regardless of which screen we're on
      tttOnlineState  = ONLINE_IDLE;
      tttShowWarning  = false;
      tttOver         = false;
      tttMySymbol     = ' ';
      tttP1Symbol     = ' ';
      tttP2Symbol     = ' ';
      for (uint8_t i = 0; i < 9; i++) tttBoard[i] = 0;
      tttShowLeftMsg   = true;
      tttLeftMsgTimer  = millis();
      navDepth = 0;
      navHistory[navDepth++] = HOME;
      navHistory[navDepth++] = TTT_MODE_SELECT;
      currentMode = TTT_ONLINE_REQUEST;
      drawScreen();
      return;
    }
    // TTT:RESET
    if (payload == "RESET") {
      if (!tttOver && tttOnline) {
        // block reset mid-game  game still active
        Serial.println(F("TTT: reset blocked  game in progress"));
        return;
      }
      tttOnline   = false;
      tttMySymbol = ' ';
      tttP1Symbol = ' ';
      tttP2Symbol = ' ';
      tttSeriesP1 = 0;
      resetTTTBoard();
      if (currentMode == TICTACTOE) drawTTTScreen();
      Serial.println(F("TTT: reset by remote"));
      return;
    }

    // TTT:PICK:X or TTT:PICK:O  remote picked their symbol
    if (payload.startsWith("PICK:")) {
      char remoteSym = payload[5];
      if (remoteSym != 'X' && remoteSym != 'O') return;

      if (tttSeriesP1 == 0) {
        // remote picked first  they are Player 1 for the series
        tttSeriesP1 = 2;
        tttP1Symbol = remoteSym;
        tttP2Symbol = (remoteSym == 'X') ? 'O' : 'X';
        tttMySymbol = tttP2Symbol;   // Mega gets remaining
        // send Mega's symbol back so remote knows
        char packet[16];
        snprintf(packet, sizeof(packet), "ESP2:TTT:PICK:%c", tttMySymbol);
        sendsignal(packet);
        tttStartGame();

      } else if (tttSeriesP1 == 1) {
        // Mega picked first, now remote confirms their symbol
        // remote gets whatever is left
        tttP2Symbol = (tttP1Symbol == 'X') ? 'O' : 'X';
        tttStartGame();

      } else {
        // remote is P1 this series, they picked  Mega gets remaining
        tttMySymbol = (remoteSym == 'X') ? 'O' : 'X';
        tttP1Symbol = remoteSym;
        tttP2Symbol = tttMySymbol;
        tttStartGame();
      }

      Serial.print(F("TTT online: Mega is "));
      Serial.println(tttMySymbol);
      return;
    }

    // TTT:cell:symbol  e.g. TTT:4:X
    int colon = payload.indexOf(':');
    if (colon > 0 && colon < (int)payload.length() - 1) {
      uint8_t cell   = payload.substring(0, colon).toInt();
      char    symbol = payload[colon + 1];
      applyTTTOnlineMove(cell, symbol);
      Serial.print(F("TTT: remote played cell "));
      Serial.print(cell); Serial.print(F(" as ")); Serial.println(symbol);
    }
    return;
  }
  else Serial.println(raw);
}

//  ESP32 SERIAL  incoming from ESP32
void handleESPSerial() {
  if (espSerial.available()) {
    String msg = espSerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      //Serial.println(msg);
      handleProtocol(msg.c_str());
    }
  }
}

// ----- TIC TAC TOE GAME ------

// get center of a cell by index (0-8) 
void cellCenter(uint8_t idx, int16_t &cx, int16_t &cy) {
  uint8_t col = idx % 3;
  uint8_t row = idx / 3;
  cx = TTT_BOARD_X + col * TTT_CELL_W + TTT_CELL_W / 2;
  cy = TTT_BOARD_Y + row * TTT_CELL_H + TTT_CELL_H / 2;
}

// draw X at center
void drawTTTX(int16_t cx, int16_t cy) {
  int16_t off = 17;
  // two thick diagonal lines
  for (int8_t t = -2; t <= 2; t++) {
    tft.drawLine(cx - off + t, cy - off, cx + off + t, cy + off, DARK_ORANGE );
    tft.drawLine(cx - off + t, cy + off, cx + off + t, cy - off, DARK_ORANGE );
  }
}

//draw O at center
void drawTTTO(int16_t cx, int16_t cy) {
  for (uint8_t r = 16; r <= 22; r++)
    tft.drawCircle(cx, cy, r, CYAN);
}

// draw one cell
void drawTTTCell(uint8_t idx) {
  int16_t cx, cy;
  cellCenter(idx, cx, cy);

  // clear cell
  tft.fillRect(
    TTT_BOARD_X + (idx % 3) * TTT_CELL_W + 3,
    TTT_BOARD_Y + (idx / 3) * TTT_CELL_H + 3,
    TTT_CELL_W - 6,
    TTT_CELL_H - 6,
    DEEP_BLUE
  );

  if (tttBoard[idx] == 1)  drawTTTX(cx, cy);
  if (tttBoard[idx] == -1) drawTTTO(cx, cy);
}

// draw grid lines
void drawTTTBoard() {
  // board background
  tft.fillRoundRect(TTT_BOARD_X - 4, TTT_BOARD_Y - 4, TTT_BOARD_W + 8, TTT_BOARD_H + 8, 10, DEEP_BLUE);
  tft.drawRoundRect(TTT_BOARD_X - 4, TTT_BOARD_Y - 4, TTT_BOARD_W + 8, TTT_BOARD_H + 8, 10, SLATE_BLUE);

  // vertical lines 
  for (uint8_t i = 1; i <= 2; i++) {
    int16_t x = TTT_BOARD_X + i * TTT_CELL_W;
    for (int8_t t = -1; t <= 1; t++)
      tft.drawFastVLine(x + t, TTT_BOARD_Y, TTT_BOARD_H, SLATE_BLUE);
  }

  // horizontal lines
  for (uint8_t i = 1; i <= 2; i++) {
    int16_t y = TTT_BOARD_Y + i * TTT_CELL_H;
    for (int8_t t = -1; t <= 1; t++)
      tft.drawFastHLine(TTT_BOARD_X, y + t, TTT_BOARD_W, SLATE_BLUE);
  }

  // draw all cells 
  for (uint8_t i = 0; i < 9; i++) drawTTTCell(i);
}

// status text (whose turn / result)
void drawTTTStatus() {
  tft.fillRect(0, CONTENT_Y + 28, SCREEN_W, 20, BLACK);
  tft.setFont(NULL);
  tft.setTextSize(1);

  if (!tttOver) {
    // whose turn
    uint16_t turnColor = tttP1Turn ? DARK_ORANGE  : CYAN;
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(14, CONTENT_Y + 34);
    tft.print("Turn:");
    tft.setTextColor(turnColor);
    tft.setCursor(50, CONTENT_Y + 34);
    tft.print(tttP1Turn ? "P1" : "P2");
    tft.fillCircle(80, CONTENT_Y + 37, 3, turnColor);

  } else {
    if (tttWinner == 2) {
      tft.setTextColor(YELLOW);
      tft.setCursor(70, CONTENT_Y + 34);
      tft.print("DRAW!");
    } else {
      // figure out which player won based on their symbol
      bool p1won = (tttWinner == (tttP1Symbol == 'X' ? 1 : -1));
      tft.setTextColor(p1won ? DARK_ORANGE  : CYAN);
      tft.setCursor(50, CONTENT_Y + 34);
      tft.print(p1won ? "P1 WINS!" : "P2 WINS!");
    }
  }
}

// score display
void drawTTTScores() {
  tft.fillRect(0, CONTENT_Y + 4, SCREEN_W, 24, BLACK);
  tft.setFont(NULL);
  tft.setTextSize(1);

  // P1 label and score  left side
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(8, CONTENT_Y + 8);
  tft.print("P1");
  // show P1 symbol in their color if known
  if (tttP1Symbol != ' ') {
    tft.setTextColor(tttP1Symbol == 'X' ? DARK_ORANGE  : CYAN);
    tft.setCursor(22, CONTENT_Y + 8);
    tft.print("("); tft.print(tttP1Symbol); tft.print(")");
  }
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(50, CONTENT_Y + 8);
  tft.print(tttScoreP1);

  // divider
  tft.setTextSize(1);
  tft.setTextColor(DARKGRAY);
  tft.setCursor(SCREEN_W / 2 - 3, CONTENT_Y + 14);
  tft.print("-");

  // P2 label and score  right side
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(SCREEN_W - 40, CONTENT_Y + 8);
  tft.print("P2");
  if (tttP2Symbol != ' ') {
    tft.setTextColor(tttP2Symbol == 'X' ? DARK_ORANGE  : CYAN);
    tft.setCursor(SCREEN_W - 26, CONTENT_Y + 8);
    tft.print("("); tft.print(tttP2Symbol); tft.print(")");
  }
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(SCREEN_W - 60, CONTENT_Y + 8);
  tft.print(tttScoreP2);

  tft.setTextSize(1);
}

//draw winning line across board 
void drawTTTWinLine() {
  if (tttWinner == 0 || tttWinner == 2) return;

  int16_t x1, y1, x2, y2;
  cellCenter(tttWinLine[0], x1, y1);
  cellCenter(tttWinLine[2], x2, y2);

   // thick green line   
  for (int8_t t = -2; t <= 2; t++) {
    tft.drawLine(x1, y1 + t, x2, y2 + t, LIME_GREEN);
    tft.drawLine(x1 + t, y1, x2 + t, y2, LIME_GREEN);
  }
}

// restart button 
void drawTTTRestartBtn() {
  int16_t bx = TTT_BOARD_X + TTT_BOARD_W + 8;
  int16_t by = TTT_BOARD_Y;
  int16_t bw = SCREEN_W - bx - 4;
  int16_t bh = TTT_BOARD_H;

  tft.fillRoundRect(bx, by, bw, bh, 8, DARK_TEAL);
  tft.drawRoundRect(bx, by, bw, bh, 8, MAUVE);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);

   // write text vertically  one char per line   
  const char* label = "NEW GAME";
  int16_t charY = by + (bh - strlen(label) * 10) / 2;
  int16_t charX = bx + (bw - 6) / 2;
  for (uint8_t i = 0; i < strlen(label); i++) {
    tft.setCursor(charX, charY + i * 10);
    tft.print(label[i]);
  }
}

//TTT MODE SELECT SCREEN
void drawTTTModeSelect() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(YELLOW);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Tic Tac Toe");
  tft.drawFastHLine(0, CONTENT_Y + 24, SCREEN_W, SLATE_BLUE);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(20, CONTENT_Y + 60);
  tft.print("Choose Mode");
  tft.setFont(NULL);

  drawCard(20, CONTENT_Y + 85, SCREEN_W - 40, 50, DEEP_BLUE, YELLOW);
  tft.setTextColor(YELLOW);
  tft.setCursor(73, CONTENT_Y + 100);
  tft.setTextSize(2);
  tft.print("OFFLINE");
  tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(52, CONTENT_Y + 120);
  tft.print("Play on this screen");

  drawCard(20, CONTENT_Y + 150, SCREEN_W - 40, 50, DEEP_BLUE, CYAN);
  tft.setTextColor(CYAN);
  tft.setCursor(74, CONTENT_Y + 165);
  tft.setTextSize(2);
  tft.print("ONLINE");
  tft.setTextSize(1);
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(44, CONTENT_Y + 185);
  tft.print("Play online via LoRA");
}

void handleTTTModeSelectTouch(int tx, int ty) {
  // Offline
  if (touchInRect(tx, ty, 20, CONTENT_Y + 60, SCREEN_W - 40, 50)) {
    tttOnline      = false;
    tttSeriesP1    = 0;      // reset series  new series starting
    tttP1Symbol    = ' ';
    tttP2Symbol    = ' ';
    tttScoreP1 = 0;
    tttScoreP2 = 0;
    navigateTo(TTT_SYMBOL_PICK);
    return;
  }
  // Online
  if (touchInRect(tx, ty, 20, CONTENT_Y + 125, SCREEN_W - 40, 50)) {
    tttOnline      = true;
    tttSeriesP1    = 0;
    tttP1Symbol    = ' ';
    tttP2Symbol    = ' ';
    tttMySymbol    = ' ';
    tttScoreP1 = 0;
    tttScoreP2 = 0;
    navigateTo(TTT_ONLINE_REQUEST);
    return;
  }
}

//  REQUEST HANDLING FOR ONLINE TTT
void drawTTTOnlineRequest() {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(CYAN);
  tft.setTextSize(1);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Online TTT");
  tft.setFont(NULL);
  tft.drawFastHLine(0, CONTENT_Y + 24, SCREEN_W, SLATE_BLUE);

  // clear content area
  tft.fillRect(0, CONTENT_Y + 30, SCREEN_W,SCREEN_H - BOTTOM_H - CONTENT_Y - 30, BLACK);

  if (tttShowLeftMsg) {
    // other player left
    tft.setTextColor(RED);
    tft.setTextSize(1);
    tft.setCursor(20, CONTENT_Y + 80);
    tft.print("Opponent left the game.");
    tft.setCursor(20, CONTENT_Y + 96);
    tft.print("Returning to lobby...");
    return;
  }

  if (tttOnlineState == ONLINE_IDLE) {
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(STEEL_GRAY);
    tft.setTextSize(1);
    tft.setCursor(12, CONTENT_Y + 55);
    tft.print("Tap to request a game with");
    tft.setCursor(60, CONTENT_Y + 75);
    tft.print("remote player.");

    drawCard(30, CONTENT_Y + 105, SCREEN_W - 60, 44, DEEP_BLUE, CYAN);
    tft.setFont(NULL);
    tft.setTextColor(CYAN);
    tft.setTextSize(2);
    tft.setCursor(60, CONTENT_Y + 120);
    tft.print("REQUEST");
    tft.setTextSize(1);

  } else if (tttOnlineState == ONLINE_REQUESTING) {
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(ORANGE);
    tft.setCursor(20, CONTENT_Y + 70);
    tft.print("Waiting for opponent");
    tft.setCursor(20, CONTENT_Y + 90);
    tft.print("to accept...");

    // cancel button
    drawCard(30, CONTENT_Y + 120, SCREEN_W - 60, 44, DEEP_BLUE, RED);
    tft.setFont(NULL);
    tft.setTextColor(RED);
    tft.setTextSize(2);
    tft.setCursor(62, CONTENT_Y + 133);
    tft.print("CANCEL");

  } else if (tttOnlineState == ONLINE_RECEIVING) {
    tft.setTextColor(LIME_GREEN);
    tft.setTextSize(1);
    tft.setCursor(20, CONTENT_Y + 50);
    tft.print("Opponent wants to play!");

    drawCard(20, CONTENT_Y + 80, 85, 44, DEEP_BLUE, LIME_GREEN);
    tft.setTextColor(LIME_GREEN);
    tft.setTextSize(2);
    tft.setCursor(32, CONTENT_Y + 97);
    tft.print("YES");
    tft.setTextSize(1);

    drawCard(125, CONTENT_Y + 80, 85, 44, DEEP_BLUE, RED);
    tft.setTextColor(RED);
    tft.setTextSize(2);
    tft.setCursor(137, CONTENT_Y + 97);
    tft.print("NO");
    tft.setTextSize(1);
  }
}
//WARRNING POPUP WHILE LEAVING ONLINE TTT
void drawWarningPopup() {
  // semi-transparent overlay rectangle in center
  int px = 20, py = SCREEN_H / 2 - 50;
  int pw = SCREEN_W - 40, ph = 100;

  tft.fillRoundRect(px, py, pw, ph, 8, DEEP_BLUE);
  tft.drawRoundRect(px, py, pw, ph, 8, ORANGE);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(px + 10, py + 12);
  tft.print("Leave game/request?");
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(px + 10, py + 26);
  tft.print("Other player will be notified.");

  // YES button
  tft.fillRoundRect(px + 10, py + 52, 70, 30, 6, RED);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(px + 22, py + 59);
  tft.print("YES");

  // NO button
  tft.fillRoundRect(px + pw - 80, py + 52, 70, 30, 6, DEEP_BLUE);
  tft.drawRoundRect(px + pw - 80, py + 52, 70, 30, 6, SLATE_BLUE);
  tft.setCursor(px + pw - 68, py + 59);
  tft.setTextColor(WHITE);
  tft.print("NO");
  tft.setTextSize(1);
}
//leave game by pressing back or home button or else
void tttLeaveGame() {
  sendsignal("ESP2:TTT:LEFT");
  tttOnlineState   = ONLINE_IDLE;
  tttShowWarning   = false;
  tttOver          = false;
  tttMySymbol      = ' ';
  tttP1Symbol      = ' ';
  tttP2Symbol      = ' ';
  for (uint8_t i = 0; i < 9; i++) tttBoard[i] = 0;
  // go back to request screen
  navDepth = 0;
  navHistory[navDepth++] = HOME;
  navHistory[navDepth++] = TTT_MODE_SELECT;
  currentMode = TTT_ONLINE_REQUEST;
  drawScreen();
}

//Handling warning message touch
void handleWarningTouch(int tx, int ty) {
  int px = 20, py = SCREEN_H / 2 - 50;
  int pw = SCREEN_W - 40;

  // YES button area
  if (touchInRect(tx, ty, px + 10, py + 52, 70, 30)) {
    tttLeaveGame();
    return;
  }

  // NO button area
  if (touchInRect(tx, ty, px + pw - 80, py + 52, 70, 30)) {
    tttShowWarning = false;
    // redraw current screen to erase popup
    drawScreen();
    if (tttOnline && currentMode == TICTACTOE) drawTTTOnlineStatus();
    return;
  }
}

//handling online ttt game request/approval touch
void handleTTTOnlineRequestTouch(int tx, int ty) {
  if (tttOnlineState == ONLINE_IDLE) {
    // REQUEST button
    if (touchInRect(tx, ty, 30, CONTENT_Y + 80, SCREEN_W - 60, 44)) {
      tttOnlineState = ONLINE_REQUESTING;
      sendsignal("ESP2:TTT:REQ");
      drawTTTOnlineRequest();
    }

  } else if (tttOnlineState == ONLINE_REQUESTING) {
    // CANCEL button
    if (touchInRect(tx, ty, 30, CONTENT_Y + 120, SCREEN_W - 60, 44)) {
      tttOnlineState = ONLINE_IDLE;
      sendsignal("ESP2:TTT:LEFT");
      drawTTTOnlineRequest();
    }

  } else if (tttOnlineState == ONLINE_RECEIVING) {
    // YES  accept
    if (touchInRect(tx, ty, 20, CONTENT_Y + 80, 85, 44)) {
      tttOnlineState = ONLINE_IN_GAME;
      sendsignal("ESP2:TTT:REQ:ACCEPT");
      // go to symbol pick
      tttSeriesP1 = 0;
      tttThisGamePicker = 1;
      tttP1Symbol = ' ';
      tttP2Symbol = ' ';
      tttMySymbol = ' ';
      navigateTo(TTT_SYMBOL_PICK);
    }
    // NO  decline
    if (touchInRect(tx, ty, 125, CONTENT_Y + 80, 85, 44)) {
      tttOnlineState = ONLINE_IDLE;
      sendsignal("ESP2:TTT:REQ:DECLINE");
      drawTTTOnlineRequest();
    }
  }
}

// TTT SYMBOL PICKER SCREEN
void drawTTTSymbolPicker(bool isOnline) {
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(YELLOW);
  tft.setCursor(12, CONTENT_Y + 20);
  tft.print("Tic Tac Toe");
  tft.setFont(NULL);
  tft.drawFastHLine(0, CONTENT_Y + 24, SCREEN_W, SLATE_BLUE);

  tft.setTextSize(1);

  if (!isOnline) {
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(10, CONTENT_Y + 38);
    if (tttSeriesP1 == 0) {
    tft.print("First to pick = Player 1");
    } else if (tttThisGamePicker == 1) {
        tft.setTextColor(DARK_ORANGE );
        tft.print("Player 1: pick your symbol");
    } else {
        tft.setTextColor(CYAN);
        tft.print("Player 2: pick your symbol");
    }
  } else {
    // online
    tft.setTextColor(STEEL_GRAY);
    tft.setCursor(10, CONTENT_Y + 38);
    if (tttSeriesP1 == 0) {
      tft.print("Pick symbol  or wait for");
      tft.setCursor(10, CONTENT_Y + 50);
      tft.print("opponent (they get X default)");
    } else if (tttSeriesP1 == 1) {
      tft.setTextColor(CYAN);
      tft.print("Your turn to pick symbol");
    } else {
      tft.setTextColor(CYAN);
      tft.print("Waiting: opponent picks first");
      tft.setCursor(10, CONTENT_Y + 50);
      tft.setTextColor(DARKGRAY);
      tft.print("(you get remaining symbol)");
    }
  }

  // X button
  drawCard(20, CONTENT_Y + 70, 80, 70, DEEP_BLUE, DARK_ORANGE );
  tft.setTextColor(DARK_ORANGE );
  tft.setTextSize(3);
  tft.setCursor(50, CONTENT_Y + 97);
  tft.print("X");
  tft.setTextSize(1);

  // O button
  drawCard(140, CONTENT_Y + 70, 80, 70, DEEP_BLUE, CYAN);
  tft.setTextColor(CYAN);
  tft.setTextSize(3);
  tft.setCursor(170, CONTENT_Y + 97);
  tft.print("O");
  tft.setTextSize(1);

  // score carry-over display
  tft.setTextColor(STEEL_GRAY);
  tft.setCursor(10, CONTENT_Y + 160);
  tft.setTextSize(2);
  tft.print("Score  P1:");
  tft.setTextColor(WHITE);
  tft.print(tttScoreP1);
  tft.setTextColor(STEEL_GRAY);
  tft.print("  P2:");
  tft.setTextColor(WHITE);
  tft.print(tttScoreP2);
}

void handleTTTSymbolPickerTouch(int tx, int ty) {
  // only let the right player pick
  // online + remote picks first this game → Mega can't pick, must wait
  if (tttOnline && tttSeriesP1 == 2) return;

  char picked = ' ';
  if (touchInRect(tx, ty, 20, CONTENT_Y + 70, 80, 70))  picked = 'X';
  if (touchInRect(tx, ty, 140, CONTENT_Y + 70, 80, 70)) picked = 'O';
  if (picked == ' ') return;

  if (!tttOnline) {
    // offline
    if (tttSeriesP1 == 0) {
        // Game 1  first person to tap becomes P1 forever
        tttSeriesP1 = 1;
        tttThisGamePicker = 1;   // P1 picked first in game 1
        tttP1Symbol = picked;
        tttP2Symbol = (picked == 'X') ? 'O' : 'X';

    } else if (tttThisGamePicker == 1) {
        // P1 picks first this game
        tttP1Symbol = picked;
        tttP2Symbol = (picked == 'X') ? 'O' : 'X';

    } else {
        // P2 picks first this game
        tttP2Symbol = picked;
        tttP1Symbol = (picked == 'X') ? 'O' : 'X';
    }

    tttStartGame();
} else {
    // online  Mega is picking
    tttMySymbol = picked;
    if (tttSeriesP1 == 0) {
      tttSeriesP1 = 1;   // Mega is Player 1 for the series
      tttP1Symbol = picked;
      tttP2Symbol = (picked == 'X') ? 'O' : 'X';
    } else {
      tttP1Symbol = picked;
      tttP2Symbol = (picked == 'X') ? 'O' : 'X';
    }
    // tell remote what Mega picked
    char packet[16];
    snprintf(packet, sizeof(packet), "ESP2:TTT:PICK:%c", picked);
    sendsignal(packet);
    // wait for remote to acknowledge before starting
    // game starts when remote sends back TTT:PICK:
    drawTTTOnlineStatus();
  }
}

// shows a small banner at top of TTT screen in online mode
void drawTTTOnlineStatus() {
  tft.fillRect(0, CONTENT_Y + 26, SCREEN_W, 22, BLACK);
  tft.setFont(NULL); tft.setTextSize(1);
  if (tttMySymbol == ' ') {
    tft.setTextColor(ORANGE);
    tft.setCursor(10, CONTENT_Y + 34);
    tft.print("Online: waiting for opponent...");
  } else {
    tft.setTextColor(CYAN);
    tft.setCursor(10, CONTENT_Y + 34);
    tft.print("You:");
    tft.setTextColor(tttMySymbol == 'X' ? DARK_ORANGE  : CYAN);
    tft.print(tttMySymbol);
    // whose turn  P1's symbol moves first
    bool myGo = (tttMySymbol == tttP1Symbol) ? tttP1Turn : !tttP1Turn;
    tft.setTextColor(CYAN);
    tft.print("  ");
    tft.setTextColor(myGo ? LIME_GREEN : DARKGRAY);
    tft.print(myGo ? "YOUR TURN" : "Their turn");
  }
}

//TTT START GAME  called once both symbols known
void tttStartGame() {
  for (uint8_t i = 0; i < 9; i++) tttBoard[i] = 0;
  tttOver          = false;
  tttWinner        = 0;
  tttAutoResetting = false;

  // whoever picked first this game goes first
  tttP1Turn = (tttThisGamePicker == 1);
  // if P1 picked first → P1 starts
  // if P2 picked first → P2 starts (tttP1Turn = false)

  navigateTo(TICTACTOE);
  if (tttOnline) drawTTTOnlineStatus();
}

//TTT GAME END  register win, schedule auto reset
void tttHandleGameEnd() {
  // scores already updated before this is called
  tttAutoResetting  = true;
  tttAutoResetTimer = millis();
  // in next game, player1 always picks first again
  // (series player1 never changes once set in game 1)
}

// full screen draw
void drawTTTScreen() {
   // header   
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(MAUVE);
  tft.setCursor(12, CONTENT_Y + 18);
  tft.print("Tic Tac Toe");
  tft.setFont(NULL);
  tft.drawFastHLine(0, CONTENT_Y + 22, SCREEN_W, SLATE_BLUE);

  drawTTTScores();
  drawTTTStatus();
  drawTTTBoard();
  drawTTTRestartBtn();
}

//  check for winner
// returns 1=X wins, -1=O wins, 2=draw, 0=ongoing
int8_t checkTTTWinner() {
  // all winning combinations 
  const uint8_t lines[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},   // rows
    {0,3,6},{1,4,7},{2,5,8},   // cols
    {0,4,8},{2,4,6}            // diagonals
  };

  for (uint8_t i = 0; i < 8; i++) {
    int8_t a = tttBoard[lines[i][0]];
    int8_t b = tttBoard[lines[i][1]];
    int8_t c = tttBoard[lines[i][2]];
    if (a != 0 && a == b && b == c) {
      tttWinLine[0] = lines[i][0];
      tttWinLine[1] = lines[i][1];
      tttWinLine[2] = lines[i][2];
      return a;   // 1 or -1
    }
  }

  // check draw
  for (uint8_t i = 0; i < 9; i++)
    if (tttBoard[i] == 0) return 0;  // still moves left 

  return 2;   //game draw
}

// reset board 
void resetTTTBoard() {
  for (uint8_t i = 0; i < 9; i++) tttBoard[i] = 0;
  tttOver   = false;
  tttWinner = 0;
  tttP1Turn  = true;
  drawTTTBoard();
  drawTTTStatus();
  drawTTTRestartBtn();
}

//  touch handler for TTT
void handleTTTTouch(int tx, int ty) {
  // restart button
  int16_t bx = TTT_BOARD_X + TTT_BOARD_W + 20;
  int16_t by = TTT_BOARD_Y;
  int16_t bw = SCREEN_W - bx - 4;
  if (touchInRect(tx, ty, bx, by, bw, TTT_BOARD_H-20)) {
    resetTTTBoard();
    return;
  }

  // ignore touches outside board or when game over
  if (tttOver) return;
  if (!touchInRect(tx, ty,TTT_BOARD_X, TTT_BOARD_Y,TTT_BOARD_W, TTT_BOARD_H)) return;

  // find which cell was tapped
  uint8_t col = (tx - TTT_BOARD_X) / TTT_CELL_W;
  uint8_t row = (ty - TTT_BOARD_Y) / TTT_CELL_H;
  uint8_t idx = row * 3 + col;

  if (idx > 8 || tttBoard[idx] != 0) return;  // occupied 

  // online mode: only allow Mega to place its own symbol on its turn 
  if (tttOnline) {
    // only move if it's Mega's turn
    // P1's symbol moves when tttP1Turn==true
    bool myGo = (tttMySymbol == tttP1Symbol) ? tttP1Turn : !tttP1Turn;
    if (!myGo) return;
    tttBoard[idx] = (tttMySymbol == 'X') ? 1 : -1;
    drawTTTCell(idx);
    char packet[16];
    snprintf(packet, sizeof(packet), "ESP2:TTT:%d:%c", idx, tttMySymbol);
    sendsignal(packet);
    drawTTTOnlineStatus();
  } else {
    // offline  P1's symbol is tttP1Symbol, P2's is tttP2Symbol
    // tttP1Turn here means "P1's turn" since P1 always goes first
    tttBoard[idx] = tttP1Turn ? (tttP1Symbol == 'X' ? 1 : -1): (tttP2Symbol == 'X' ? 1 : -1);
    drawTTTCell(idx);
  }

  // check result 
  tttWinner = checkTTTWinner();
  if (tttWinner != 0) {
    tttOver = true;
    if (tttWinner == (tttP1Symbol == 'X' ? 1 : -1))  tttScoreP1++;
    else if (tttWinner != 2) tttScoreP2++;
    drawTTTScores();
    drawTTTStatus();
    if (tttWinner != 2) drawTTTWinLine();
    tttHandleGameEnd();   // schedule auto-reset
  } else {
    tttP1Turn = !tttP1Turn;
    drawTTTStatus();
  }
}

// TOUCH
void handleTouch() {
  // if warning popup is showing, only handle warning touches
  if (tttShowWarning) {
    TSPoint p = ts.getPoint();
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return;
    int tx = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_W - 1);
    int ty = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_H - 1);
    handleWarningTouch(tx, ty);
    return;
  }
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return;

  int tx = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_W - 1);
  int ty = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_H - 1);
  tx = constrain(tx, 0, SCREEN_W - 1);
  ty = constrain(ty, 0, SCREEN_H - 1);

  //bottom nav
  if (ty >= SCREEN_H - BOTTOM_H) {
  // if in online game or request  show warning instead
    if (tttOnline && (currentMode == TICTACTOE ||
        currentMode == TTT_SYMBOL_PICK ||
        (currentMode == TTT_ONLINE_REQUEST &&
        tttOnlineState != ONLINE_IDLE))) {
      tttShowWarning = true;
      drawWarningPopup();
      return;
    }
    // keyboard screen  back cancels, home also cancels then goes home
    if (currentMode == KEYBOARD) {
      if (tx >= 23 && tx <= 60) {
        kbFinish(false);
      } else if (tx >= 98 && tx <= 140) {
        kbBuffer[0] = '\0';
        kbLen       = 0;
        kbTarget    = KB_NONE;
        toolbarVisible = false;
        navigateHome();
      }
      return;
    }
    //back button icon touch area
    if (tx >= 23 && tx <= 60) {
      navigateBack();
    //home button touch area
    } else if (tx >= 98 && tx <= 140) {
      toolbarVisible = false;
      navigateHome();
    }
    return;
  }

  if (ty < STATUS_H) return;

  if (currentMode == HOME) {
    for (uint8_t i = 0; i < ICON_COUNT; i++) {
      if (touchInRect(tx, ty, icons[i].x, icons[i].y, ICON_W, ICON_H)) {
        if (icons[i].target == TICTACTOE) {
          navigateTo(TTT_MODE_SELECT);  // go to mode picker first
        } else {
          navigateTo(icons[i].target);
        }
        return;
      }
    }
    return;
  }
  
  if (currentMode == TTT_MODE_SELECT) { handleTTTModeSelectTouch(tx, ty); return; }
  if (currentMode == TTT_SYMBOL_PICK) { handleTTTSymbolPickerTouch(tx, ty); return; }
  if (currentMode == TTT_ONLINE_REQUEST) { handleTTTOnlineRequestTouch(tx, ty); return; }
  if (currentMode == DRAW) { handleDrawTouch(tx, ty); return; }
  if (currentMode == TICTACTOE) { handleTTTTouch(tx, ty); return; }
  if (currentMode == GPS) {
    // next button
    if (gpsPage < 3 &&
        touchInRect(tx, ty, GPS_NEXT_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H)) {
      gpsPage++;
      drawGPSScreen();
      return;
    }
    // prev button
    if (gpsPage > 0 &&
        touchInRect(tx, ty, GPS_PREV_X, GPS_BTN_Y, GPS_BTN_W, GPS_BTN_H)) {
      gpsPage--;
      drawGPSScreen();
      return;
    }
    return;
  }
  if (currentMode == NET)          { handleNetTouch(tx, ty);         return; }
  if (currentMode == NET_WIFI)     { handleNetWifiTouch(tx, ty);     return; }
  if (currentMode == NET_BT)       { handleNetBTTouch(tx, ty);       return; }
  if (currentMode == NET_INTERNET) { handleNetInternetTouch(tx, ty); return; }
  if (currentMode == CLOCK) { handleClockTouch(tx, ty); return; }
  if (currentMode == KEYBOARD) { handleKeyboardTouch(tx, ty); return; }
  if (currentMode == CHAT) {
    if (touchInRect(tx, ty, SCREEN_W - 64, CONTENT_Y + 6, 56, 20)) {
      startKeyboard("Type message", false, KB_CHAT_MSG);
      return;
    }
    return;
  }
  
}

//SETUP
void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  gpsSerial.begin(9600);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  uint16_t id = tft.readID();
  if (id == 0xD3D3 || id == BLACK || id == WHITE) id = 0x9341;
  tft.begin(id);
  tft.setRotation(0);
  tft.fillScreen(BLACK);

  drawScreen();
  Serial.println("Mega is Ready");
}

//LOOP

void loop() {
  handleTouch();
  handleESPSerial();

  //handling mega's written serial msgs 
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      // wifi password entry via serial
      if (wifiAwaitingPass) {
        strncpy(wifiPassword, msg.c_str(), 63);
        wifiAwaitingPass = false;
        char packet[128];
        snprintf(packet, sizeof(packet), "ESP1:NET:WIFI:CONNECT:%s:%s",wifiPendingSSID, wifiPassword);
        sendsignal(packet);
        wifiPassword[0] = '\0';
        snprintf(netStatusMsg, sizeof(netStatusMsg),"Connecting to %s...", wifiPendingSSID);
        netWaiting = true;
        if (currentMode == NET_WIFI) drawNetStatusBar(netStatusMsg, ORANGE);
        Serial.println("Password sent. Connecting...");
        return;
      }
      // http URL entry via serial
      if (httpWaiting && msg.startsWith("http")) {
        strncpy(httpPendingURL, msg.c_str(), 95);
        char packet[112];
        snprintf(packet, sizeof(packet), "ESP1:NET:HTTP:GET:%s", httpPendingURL);
        sendsignal(packet);
        snprintf(netStatusMsg, sizeof(netStatusMsg), "Fetching...");
        if (currentMode == NET_INTERNET) drawNetInternetScreen();
        return;
      }
      sendsignal(msg);
    }
  }

  if (currentMode == TEMP) updateTempScreen();
  if (currentMode == GPS)  updateGPSScreen();
  if (currentMode == CLOCK && clockPage == 0) updateClockPage0();

  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  // dismiss left game message after 2 seconds
  if (tttShowLeftMsg && millis() - tttLeftMsgTimer >= 2000UL) {
    tttShowLeftMsg = false;
    drawTTTOnlineRequest();
  }
  // TTT auto-reset 1 second after game ends
  if (tttAutoResetting && millis() - tttAutoResetTimer >= 1000UL) {
    tttAutoResetting = false;
    if (!tttOnline) {

    // flip who picks first next game
    tttThisGamePicker = (tttThisGamePicker == 1) ? 2 : 1;

    // clear symbols for fresh pick
    tttP1Symbol = ' ';
    tttP2Symbol = ' ';

    navigateBack();
    }else {
      // online: go back to symbol picker, alternate who picks first
      // if Mega was P1 last game, remote picks first next game
      tttThisGamePicker = (tttThisGamePicker == 1) ? 2 : 1;
      tttMySymbol = ' ';
      tttP1Symbol = ' ';
      tttP2Symbol = ' ';
      navigateBack();
    }
  }
  updateClockBackground();
}