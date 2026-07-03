/* 
    ESP32 Remote Node — LoRa Bridge + WiFi AP + Webpage Hub
    
    WiFi AP  : SSID = Hub  |  Pass = 12345678
    IP       : 192.168.4.1
    
    Protocol (LoRa / Serial):
      CHAT:message          → chat message
      TTT:cell:symbol       → tic tac toe move  (cell 0-8, symbol X or O)
      TTT:RESET             → reset game
      TTT:PICK:X / TTT:PICK:O → player side picked
    Anything not matching protocol → printed to Serial only
*/

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>

// LoRa pins
    
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 2

// WiFi AP credentials
    
const char* AP_SSID = "Hub";
const char* AP_PASS = "12345678";

WebServer server(80);

//State shared between web handlers and LoRa
    
// Chat: ring buffer of last 20 messages
#define MAX_CHAT 20
struct ChatEntry {
  String text;
  String from;   // "Mega" or "Web"
};
ChatEntry chatLog[MAX_CHAT];
int chatHead = 0;   // next write index
int chatCount = 0;

//TTT globals
enum WebOnlineState {
  WEB_IDLE,
  WEB_REQUESTING,
  WEB_RECEIVING,
  WEB_IN_GAME
};
WebOnlineState webOnlineState = WEB_IDLE;
bool webShowLeftMsg = false;

// Tic Tac Toe board
// board[i] = ' ', 'X', or 'O'
char board[9] = {' ',' ',' ',' ',' ',' ',' ',' ',' '};
char webSymbol  = ' ';   // picked by web player at start
char megaSymbol = ' ';
bool    gameActive  = false;
bool    webTurn     = false;
String  gameStatus  = "Pick your side to start";
uint8_t tttScoreP1   = 0;
uint8_t tttScoreP2   = 0;
uint32_t tttResetTimer    = 0;
bool     tttPendingReset  = false;
// series tracking — who picks first
// 0=not set 1=web is P1 2=mega is P1
uint8_t tttSeriesP1 = 0;
char    tttP1Symbol = ' ';
char    tttWebSymbol_actual = ' ';  // web player's actual symbol this game

//Helpers
    
void addChat(const String& text, const String& from) {
  chatLog[chatHead % MAX_CHAT] = {text, from};
  chatHead++;
  if (chatCount < MAX_CHAT) chatCount++;
}

void resetBoard() {
  for (int i = 0; i < 9; i++) board[i] = ' ';
  gameActive  = false;
  webTurn     = false;
  webSymbol   = ' ';
  megaSymbol  = ' ';
  gameStatus  = "Pick your side to start";
}

char checkWinner() {
  const int lines[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
  for (auto& l : lines) {
    if (board[l[0]] != ' ' &&
        board[l[0]] == board[l[1]] &&
        board[l[1]] == board[l[2]])
      return board[l[0]];
  }
  // check draw
  bool full = true;
  for (int i = 0; i < 9; i++) if (board[i] == ' ') { full = false; break; }
  return full ? 'D' : ' ';
}
    
//Html web interface
const char PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LoRa Hub</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#0d1117;color:#e6edf3;font-family:'Segoe UI',sans-serif;min-height:100vh}
  header{background:#161b22;border-bottom:1px solid #30363d;padding:14px 20px;display:flex;align-items:center;gap:12px}
  header h1{font-size:1.2rem;font-weight:600;color:#58a6ff}
  .dot{width:10px;height:10px;border-radius:50%;background:#3fb950;box-shadow:0 0 6px #3fb950}
  nav{display:flex;gap:16px;padding:20px 16px;background:#0d1117;justify-content:center;flex-wrap:wrap}
  .appicon{display:flex;flex-direction:column;align-items:center;gap:8px;cursor:pointer;width:80px}
  .appicon .iconbox{width:64px;height:64px;border-radius:16px;display:flex;align-items:center;justify-content:center;font-size:2rem;transition:.15s;box-shadow:0 4px 12px rgba(0,0,0,.4)}
  .appicon .iconbox:hover{transform:scale(1.08)}
  .appicon .iconbox.active{outline:3px solid #58a6ff;outline-offset:2px}
  .appicon span{font-size:.75rem;color:#8b949e;text-align:center}
  .page{display:none;padding:16px;max-width:480px;margin:0 auto}
  .page.show{display:block}

  //  CHAT  
  #chatBox{height:340px;overflow-y:auto;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:10px;display:flex;flex-direction:column;gap:8px;margin-bottom:10px;scroll-behavior:smooth}
  .bub{max-width:75%;padding:8px 12px;border-radius:14px;font-size:.88rem;line-height:1.4;word-break:break-word}
  .bub.web{align-self:flex-end;background:#1f6feb;color:#fff;border-bottom-right-radius:4px}
  .bub.mega{align-self:flex-start;background:#21262d;color:#e6edf3;border-bottom-left-radius:4px}
  .bub .who{font-size:.7rem;opacity:.6;margin-bottom:2px}
  .row{display:flex;gap:8px}
  #chatInput{flex:1;background:#21262d;border:1px solid #30363d;border-radius:8px;padding:9px 12px;color:#e6edf3;font-size:.9rem;outline:none}
  #chatInput:focus{border-color:#58a6ff}
  .btn{padding:9px 18px;border:none;border-radius:8px;cursor:pointer;font-size:.9rem;font-weight:600;transition:.15s}
  .btn-blue{background:#1f6feb;color:#fff}
  .btn-blue:hover{background:#388bfd}
  .btn-green{background:#238636;color:#fff}
  .btn-green:hover{background:#2ea043}
  .btn-red{background:#da3633;color:#fff}
  .btn-red:hover{background:#f85149}
  .btn-gray{background:#21262d;color:#e6edf3;border:1px solid #30363d}
  .btn-gray:hover{background:#30363d}

  //  TTT  
  #tttWrap{text-align:center}
  #pickWrap{margin:20px 0}
  #pickWrap p{color:#8b949e;margin-bottom:12px}
  #pickWrap .row{justify-content:center;gap:12px}
  #statusMsg{margin:12px 0;font-size:.95rem;min-height:1.4em;color:#f0883e}
  #board{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;max-width:240px;margin:0 auto 16px}
  .cell{aspect-ratio:1;background:#21262d;border:1px solid #30363d;border-radius:8px;font-size:2rem;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:.15s;color:#58a6ff}
  .cell:hover:not(.taken){background:#30363d}
  .cell.taken{cursor:default}
  .cell.X{color:#f85149}
  .cell.O{color:#3fb950}
  #resetBtn{margin-top:6px}
  #scoreBar{display:flex;justify-content:center;gap:24px;margin-bottom:10px;font-size:1rem;font-weight:600}
  .sc-x{color:#f85149}
  .sc-o{color:#3fb950}
  .sc-label{color:#8b949e;font-weight:400}
  nav.hidden{display:none}
  .backbar{display:none;padding:10px 16px;background:#161b22;border-bottom:1px solid #30363d}
  .backbar.show{display:block}
  .btn-back{background:none;border:none;color:#58a6ff;font-size:.95rem;cursor:pointer;padding:4px 0;display:flex;align-items:center;gap:6px}
  .btn-back:hover{color:#388bfd}
  #chatWrap{display:flex;flex-direction:column;height:calc(100vh - 120px);min-height:300px}
  #chatBox{flex:1;overflow-y:auto;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:10px;display:flex;flex-direction:column;gap:8px;margin-bottom:10px;scroll-behavior:smooth;min-height:120px}
  #chatInputRow{flex-shrink:0;padding-bottom:env(keyboard-inset-height,0px);transition:padding .2s}
</style>
</head>
<body>
<header>
  <div class="dot"></div>
  <h1>Hub Interface</h1>
</header>
<nav>
  <div class="appicon" onclick="tab('chat',this)">
    <div class="iconbox active" id="icon-chat" style="background:linear-gradient(135deg,#1f6feb,#388bfd)">💬</div>
    <span>Chat</span>
  </div>
  <div class="appicon" onclick="tab('ttt',this)">
    <div class="iconbox" id="icon-ttt" style="background:linear-gradient(135deg,#238636,#2ea043)">⭕</div>
    <span>Tic Tac Toe</span>
  </div>
</nav>

<div class="backbar" id="backbar">
  <button class="btn-back" onclick="goHome()">&#8592; Back</button>
</div>

<!--  CHAT  -->
<div id="chatPage" class="page">
  <div id="chatWrap">
    <div id="chatBox"></div>
    <div class="row" id="chatInputRow">
      <input id="chatInput" placeholder="Type a message…" onkeydown="if(event.key==='Enter')sendChat()">
      <button class="btn btn-blue" onclick="sendChat()">Send</button>
    </div>
  </div>
</div>

<!--  TTT  -->
<div id="tttPage" class="page">
  <!-- request screen -->
  <div id="requestWrap">
    <div id="idleScreen">
      <p style="color:#8b949e;margin-bottom:16px">Request a game with Mega</p>
      <button class="btn btn-blue" onclick="requestGame()">Request Game</button>
    </div>
    <div id="waitingScreen" style="display:none">
      <p style="color:#f0883e;margin-bottom:16px">Waiting for Mega to accept...</p>
      <button class="btn btn-red" onclick="cancelRequest()">Cancel</button>
    </div>
    <div id="receivingScreen" style="display:none">
      <p style="color:#3fb950;margin-bottom:16px">Mega wants to play!</p>
      <div class="row" style="justify-content:center;gap:12px">
        <button class="btn btn-green" onclick="acceptGame()">Accept</button>
        <button class="btn btn-red" onclick="declineGame()">Decline</button>
      </div>
    </div>
    <div id="declinedScreen" style="display:none">
      <p style="color:#f85149">Request declined.</p>
    </div>
    <div id="leftScreen" style="display:none">
      <p style="color:#f85149">Opponent left the game.</p>
      <p style="color:#8b949e;margin-top:8px">Returning to lobby...</p>
    </div>
  </div>

  <!-- pick side -->
  <div id="pickWrap" style="display:none">
    <p style="color:#8b949e;margin-bottom:12px">Choose your side</p>
    <div class="row" style="justify-content:center;gap:12px">
      <button class="btn btn-red" onclick="pickSide('X')">Play as X</button>
      <button class="btn btn-green" onclick="pickSide('O')">Play as O</button>
    </div>
  </div>

  <!-- game board -->
  <div id="gameWrap" style="display:none">
    <div id="scoreBar">
      <span><span class="sc-label">P1 </span><span id="scoreP1">0</span></span>
      <span style="color:#30363d">|</span>
      <span><span class="sc-label">P2 </span><span id="scoreP2">0</span></span>
    </div>
    <div id="statusMsg">…</div>
    <div id="board"></div>
  </div>

  <!-- warning popup overlay -->
  <div id="warningPopup" style="display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.6);z-index:100;display:none;align-items:center;justify-content:center">
    <div style="background:#161b22;border:1px solid #f0883e;border-radius:12px;padding:24px;max-width:300px;width:90%;text-align:center">
      <p style="margin-bottom:8px;font-weight:600">Leave game?</p>
      <p style="color:#8b949e;font-size:.85rem;margin-bottom:20px">Other player will be notified.</p>
      <div class="row" style="justify-content:center;gap:12px">
        <button class="btn btn-red" onclick="confirmLeave()">Yes, Leave</button>
        <button class="btn btn-gray" onclick="cancelLeave()">No, Stay</button>
      </div>
    </div>
  </div>
</div>

<script>
//  state  
let mySymbol = '';
let boardState = Array(9).fill(' ');
let myTurn = false;
let active = false;
let lastChatHead = 0;
let webOnlineState = 0; 


//  tab switch  
function tab(name, el) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('show'));
  document.querySelectorAll('.iconbox').forEach(b => b.classList.remove('active'));
  document.getElementById(name + 'Page').classList.add('show');
  const box = el.classList.contains('iconbox') ? el : el.querySelector('.iconbox');
  if (box) box.classList.add('active');
  // hide nav, show back button
  document.getElementById('backbar').classList.add('show');
  document.querySelector('nav').classList.add('hidden');
}

//  CHAT  
function sendChat() {
  const inp = document.getElementById('chatInput');
  const txt = inp.value.trim();
  if (!txt) return;
  inp.value = '';
  fetch('/chat/send', {
    method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: 'msg=' + encodeURIComponent(txt)
  });
}

function renderChat(entries) {
  const box = document.getElementById('chatBox');
  box.innerHTML = '';
  entries.forEach(e => {
    const d = document.createElement('div');
    d.className = 'bub ' + (e.from === 'Web' ? 'web' : 'mega');
    d.innerHTML = '<div class="who">' + e.from + '</div>' + escHtml(e.text);
    box.appendChild(d);
  });
  box.scrollTop = box.scrollHeight;
}

function escHtml(s) {
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

//  TTT  
function requestGame() {
  fetch('/ttt/request', {method:'POST'});
  // immediately update UI don't wait for poll
  showOnlyInRequest('waitingScreen');
}

function cancelRequest() {
  fetch('/ttt/left', {method:'POST'});
  showOnlyInRequest('idleScreen');
}

function acceptGame() {
  fetch('/ttt/accept', {method:'POST'});
  document.getElementById('requestWrap').style.display = 'none';
  document.getElementById('pickWrap').style.display = 'block';
}

function declineGame() {
  fetch('/ttt/decline', {method:'POST'});
  showOnlyInRequest('idleScreen');
}

function pickSide(sym) {
  mySymbol = sym;
  document.getElementById('pickWrap').style.display = 'none';
  document.getElementById('gameWrap').style.display = 'block';
  fetch('/ttt/pick', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'sym='+sym
  });
}

function makeMove(idx) {
  if (!myTurn || !active || boardState[idx] !== ' ') return;
  fetch('/ttt/move', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'cell='+idx
  });
}

// warning popup
let warningShowing = false;

function showWarning() {
  warningShowing = true;
  const p = document.getElementById('warningPopup');
  p.style.display = 'flex';
}

function cancelLeave() {
  warningShowing = false;
  document.getElementById('warningPopup').style.display = 'none';
}

function confirmLeave() {
  warningShowing = false;
  document.getElementById('warningPopup').style.display = 'none';
  mySymbol = '';
  boardState = Array(9).fill(' ');
  active = false;
  // hide game/pick screens, show request screen
  document.getElementById('gameWrap').style.display    = 'none';
  document.getElementById('pickWrap').style.display    = 'none';
  document.getElementById('requestWrap').style.display = 'block';
  showOnlyInRequest('idleScreen');
  fetch('/ttt/left', {method:'POST'});
}

// intercept back button and home nav during game
function goHome() {
  // show warning if in game OR if in any online state (requesting/receiving/in-game)
  if (active || webOnlineState >= 1) {
    showWarning();
    return;
  }
  document.querySelectorAll('.page').forEach(p => p.classList.remove('show'));
  document.getElementById('backbar').classList.remove('show');
  document.querySelector('nav').classList.remove('hidden');
  // reset local TTT state
  mySymbol = '';
  boardState = Array(9).fill(' ');
}

//  POLL  
async function poll() {
  try {
    const r = await fetch('/state');
    const d = await r.json();

    // chat — always update
    renderChat(d.chat);

    // update global state
    webOnlineState = d.onlineState || 0;
    myTurn  = (d.webTurn === true || d.webTurn === 'true');
    active  = (d.gameActive === true || d.gameActive === 'true');

    // always update TTT screens based on state
    updateTTTScreens(d);

  } catch(e) {
    console.log('Poll error:', e);  // log but don't die
  }
  setTimeout(poll, 1000);  // always reschedule
}

function updateTTTScreens(d) {
  const state = d.onlineState || 0;

  // handle left message first
  if (d.leftMsg) {
    document.getElementById('requestWrap').style.display = 'block';
    document.getElementById('pickWrap').style.display   = 'none';
    document.getElementById('gameWrap').style.display   = 'none';
    showOnlyInRequest('leftScreen');
    return;
  }

  if (state === 0) {
    // idle — show request screen
    document.getElementById('requestWrap').style.display = 'block';
    document.getElementById('pickWrap').style.display   = 'none';
    document.getElementById('gameWrap').style.display   = 'none';
    if (d.gameStatus === 'REQUEST_DECLINED') {
      showOnlyInRequest('declinedScreen');
    } else {
      showOnlyInRequest('idleScreen');
    }

  } else if (state === 1) {
    // we sent request — waiting
    document.getElementById('requestWrap').style.display = 'block';
    document.getElementById('pickWrap').style.display   = 'none';
    document.getElementById('gameWrap').style.display   = 'none';
    showOnlyInRequest('waitingScreen');

  } else if (state === 2) {
    // we received request — show accept/decline
    document.getElementById('requestWrap').style.display = 'block';
    document.getElementById('pickWrap').style.display   = 'none';
    document.getElementById('gameWrap').style.display   = 'none';
    showOnlyInRequest('receivingScreen');

  } else if (state === 3) {
    // in game
    document.getElementById('requestWrap').style.display = 'none';

    if (!mySymbol) {
      // symbol not picked yet
      document.getElementById('pickWrap').style.display = 'block';
      document.getElementById('gameWrap').style.display = 'none';
    } else {
      document.getElementById('pickWrap').style.display = 'none';
      document.getElementById('gameWrap').style.display = 'block';
      boardState = d.board.split('');
      myTurn  = (d.webTurn === true || d.webTurn === 'true');
      active  = (d.gameActive === true || d.gameActive === 'true');
      document.getElementById('statusMsg').textContent = d.gameStatus;
      renderBoard();
      if (d.scoreP1 !== undefined)
        document.getElementById('scoreP1').textContent = d.scoreP1;
      if (d.scoreP2 !== undefined)
        document.getElementById('scoreP2').textContent = d.scoreP2;
    }
  }
}

function showOnlyInRequest(id) {
  ['idleScreen','waitingScreen','receivingScreen','declinedScreen','leftScreen']
    .forEach(s => {
      document.getElementById(s).style.display = s === id ? 'block' : 'none';
    });
}

poll();

//  keep input row visible when keyboard opens  
const chatInput = document.getElementById('chatInput');
let chatBoxNaturalHeight = 0;

chatInput.addEventListener('focus', () => {
  // small delay lets browser finish keyboard animation
  setTimeout(() => {
    const box = document.getElementById('chatBox');
    // shrink chatBox so input stays visible, min 120px so it never disappears
    const vh = window.visualViewport ? window.visualViewport.height : window.innerHeight;
    const wrap = document.getElementById('chatWrap');
    const wrapTop = wrap.getBoundingClientRect().top;
    const inputRowH = document.getElementById('chatInputRow').offsetHeight + 20;
    const newBoxH = Math.max(120, vh - wrapTop - inputRowH - 20);
    box.style.height = newBoxH + 'px';
    box.style.flex = 'none';
    box.scrollTop = box.scrollHeight;
  }, 300);
});

chatInput.addEventListener('blur', () => {
  // restore when keyboard closes
  const box = document.getElementById('chatBox');
  box.style.height = '';
  box.style.flex = '1';
});

// keep scroll at bottom when new messages arrive
const observer = new MutationObserver(() => {
  const box = document.getElementById('chatBox');
  box.scrollTop = box.scrollHeight;
});
observer.observe(document.getElementById('chatBox'), { childList: true });

function renderBoard() {
  const bd = document.getElementById('board');
  bd.innerHTML = '';
  boardState.forEach((v, i) => {
    const c = document.createElement('div');
    c.className = 'cell' + (v !== ' ' ? ' taken ' + v : '');
    c.textContent = v !== ' ' ? v : '';
    if (v === ' ' && myTurn && active) c.onclick = () => makeMove(i);
    bd.appendChild(c);
  });
}

</script>
</body>
</html>
)rawhtml";

// Route: GET /
    
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

// Route: GET /state  — JSON snapshot for polling
    
void handleState() {
  // build chat array
  String chatArr = "[";
  int start = (chatCount < MAX_CHAT) ? 0 : chatHead % MAX_CHAT;
  for (int i = 0; i < chatCount; i++) {
    ChatEntry& e = chatLog[(start + i) % MAX_CHAT];
    if (i) chatArr += ",";
    chatArr += "{\"from\":\"" + e.from + "\",\"text\":\"";
    // escape quotes in text
    String t = e.text;
    t.replace("\"", "\\\"");
    chatArr += t + "\"}";
  }
  chatArr += "]";

  // build board string (9 chars)
  String boardStr = "\"";
  for (int i = 0; i < 9; i++) boardStr += board[i];
  boardStr += "\"";

  String gs = gameStatus;
  gs.replace("\"", "\\\"");

  String json = "{";
  json += "\"chat\":"        + chatArr + ",";
  json += "\"board\":"       + boardStr + ",";
  json += "\"webTurn\":"     + String(webTurn      ? "true" : "false") + ",";
  json += "\"onlineState\":" + String((int)webOnlineState) + ",";
  json += "\"leftMsg\":"     + String(webShowLeftMsg ? "true" : "false") + ",";
  json += "\"gameActive\":"  + String(gameActive   ? "true" : "false") + ",";
  json += "\"gameStatus\":\"" + gs + "\",";
  json += "\"scoreP1\":"     + String(tttScoreP1) + ",";
  json += "\"scoreP2\":"     + String(tttScoreP2);
  json += "}";
  server.send(200, "application/json", json);

}

//Lora communication(sender) function
void loraSend(const String& msg) {
  LoRa.beginPacket();
  LoRa.print("ESP2:"+msg);
  LoRa.endPacket();
  Serial.println("ESP2:"+msg);
}

//Route: POST /chat/send 
void handleChatSend() {
  if (!server.hasArg("msg")) { server.send(400, "text/plain", "bad"); return; }
  String msg = server.arg("msg");
  msg.trim();
  if (msg.length() == 0) { server.send(200, "text/plain", "ok"); return; }

  addChat(msg, "Web");

  // send over LoRa
  String packet = "MEGA:CHAT:" + msg;
  loraSend(packet);
  server.send(200, "text/plain", "ok");
}

//Route: post/ttt/online/request 

void handleTTTRequest() {
  webOnlineState = WEB_REQUESTING;
  gameStatus = "WAITING_ACCEPT";
  loraSend("MEGA:TTT:REQ");
  server.send(200, "text/plain", "ok");
}

void handleTTTLeft() {
  webOnlineState = WEB_IDLE;
  resetBoard();
  gameStatus = "Pick your side to start";
  loraSend("MEGA:TTT:LEFT");
  server.send(200, "text/plain", "ok");
}

void handleTTTAccept() {
  webOnlineState = WEB_IN_GAME;
  gameStatus = "REQUEST_ACCEPTED";
  loraSend("MEGA:TTT:REQ:ACCEPT");
  server.send(200, "text/plain", "ok");
}

void handleTTTDecline() {
  webOnlineState = WEB_IDLE;
  gameStatus = "Pick your side to start";
  loraSend("MEGA:TTT:REQ:DECLINE");
  server.send(200, "text/plain", "ok");
}

// Route: POST /ttt/pick   (sym=X or O)
    
void handleTTTPick() {
  if (!server.hasArg("sym")) { server.send(400, "text/plain", "bad"); return; }
  String sym = server.arg("sym");
  sym.trim();
  if (sym != "X" && sym != "O") { server.send(400, "text/plain", "bad"); return; }

  webSymbol = sym[0];
  tttWebSymbol_actual = webSymbol;

  // series tracking
  if (tttSeriesP1 == 0) {
    tttSeriesP1 = 1;        // web picked first — web is P1 for series
    tttP1Symbol = webSymbol;
  }
  megaSymbol = (webSymbol == 'X') ? 'O' : 'X';

  resetBoard();
  webSymbol             = sym[0];
  tttWebSymbol_actual   = webSymbol;
  megaSymbol            = (webSymbol == 'X') ? 'O' : 'X';
  gameActive            = true;
  // P1's symbol goes first
  webTurn = (webSymbol == tttP1Symbol);
  gameStatus = webTurn ? "Your turn" : "Mega's turn";

  String packet = "MEGA:TTT:PICK:" + sym;
  loraSend(packet);
  server.send(200, "text/plain", "ok");
}

//Route: POST /ttt/move   (cell=0-8)
    
void handleTTTMove() {
  if (!server.hasArg("cell")) { server.send(400, "text/plain", "bad"); return; }
  int cell = server.arg("cell").toInt();
  if (cell < 0 || cell > 8 || board[cell] != ' ' || !webTurn || !gameActive) {
    server.send(400, "text/plain", "invalid");
    return;
  }

  board[cell] = webSymbol;
  webTurn     = false;

  // check result
  char w = checkWinner();
  if (w == webSymbol) {
    gameStatus = "You win!";
    gameActive = false;
    if (tttSeriesP1 == 1) tttScoreP1++;
    else                  tttScoreP2++;
    tttPendingReset = true;
    tttResetTimer   = millis();
  } else if (w == 'D') {
    gameStatus = "Draw!";
    gameActive = false;
    tttPendingReset = true;
    tttResetTimer   = millis();
  } else {
    gameStatus = "Mega's turn";
  }

  // send move over LoRa
  String packet = "MEGA:TTT:" + String(cell) + ":" + String(webSymbol);
  loraSend(packet);
  server.send(200, "text/plain", "ok");
}

// Route: POST /ttt/reset
void handleTTTReset() {
  resetBoard();
  loraSend("MEGA:TTT:RESET");
  server.send(200, "text/plain", "ok");
}

// SETUP
    
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600); 

  //  LoRa  
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    while (1);
  }
  Serial.println("LoRa ready");

  //  WiFi AP  
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP started — IP: ");
  Serial.println(WiFi.softAPIP());

  //  Web routes  
  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/state",       HTTP_GET,  handleState);
  server.on("/chat/send",   HTTP_POST, handleChatSend);
  server.on("/ttt/request",  HTTP_POST, handleTTTRequest);
  server.on("/ttt/left", HTTP_POST, handleTTTLeft);
  server.on("/ttt/accept",   HTTP_POST, handleTTTAccept);
  server.on("/ttt/decline",  HTTP_POST, handleTTTDecline);
  server.on("/ttt/pick",    HTTP_POST, handleTTTPick);
  server.on("/ttt/move",    HTTP_POST, handleTTTMove);
  server.on("/ttt/reset",   HTTP_POST, handleTTTReset);
  server.begin();

  resetBoard();
}

// LOOP
    
void loop() {
  // clear left message after 2 seconds
  if (webShowLeftMsg && millis() - tttResetTimer >= 2000UL) {
    webShowLeftMsg = false;
    webOnlineState = WEB_IDLE;
  }
  // auto-reset board 1 second after game ends
  if (tttPendingReset && millis() - tttResetTimer >= 1000UL) {
    tttPendingReset = false;
    resetBoard();
    // alternate who picks first next game
    if (tttSeriesP1 == 1) tttSeriesP1 = 2;
    else if (tttSeriesP1 == 2) tttSeriesP1 = 1;
    webSymbol  = ' ';
    megaSymbol = ' ';
    tttP1Symbol = ' ';
    gameStatus = "Pick your side to start";
  }

  server.handleClient();

  //  Incoming LoRa packet  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();
    msg.trim();
    Serial.println(msg);
    String full = msg;
    int firstColon  = full.indexOf(':');
    int secondColon = full.indexOf(':', firstColon + 1);
    if (secondColon < 0) return;
    String msg1 = full.substring(secondColon + 1);

    //  CHAT:message  
    if (msg1.startsWith("CHAT:")) {
      String text = msg1.substring(5);
      addChat(text, "Mega");
    }

    //  TTT:cell:symbol  or  TTT:RESET  or  TTT:PICK:sym  
    else if (msg1.startsWith("TTT:")) {
      String payload = msg1.substring(4);

      if (payload == "REQ") {
        webOnlineState = WEB_RECEIVING;
        gameStatus = "INCOMING_REQUEST";

      } else if (payload == "REQ:ACCEPT") {
        webOnlineState = WEB_IN_GAME;
        gameStatus = "REQUEST_ACCEPTED";

      } else if (payload == "REQ:DECLINE") {
        webOnlineState = WEB_IDLE;
        gameStatus = "REQUEST_DECLINED";

      } else if (payload == "LEFT") {
        webOnlineState = WEB_IDLE;
        resetBoard();
        webShowLeftMsg = true;
        gameStatus = "OPPONENT_LEFT";

      } else if (payload == "RESET") {
        resetBoard();
        Serial.println("TTT: board reset by Mega");

      } else if (payload.startsWith("PICK:")) {
        char sym = payload[5];
        if (sym == 'X' || sym == 'O') {
          megaSymbol = sym;
          webSymbol  = (sym == 'X') ? 'O' : 'X';
          gameActive = true;
          webTurn    = (webSymbol == 'X');
          gameStatus = webTurn ? "Your turn" : "Mega's turn";
        }

      } else {
        // TTT:cell:symbol  e.g. TTT:4:O
        int colon = payload.indexOf(':');
        if (colon > 0) {
          int  cell   = payload.substring(0, colon).toInt();
          char symbol = payload[colon + 1];

          if (cell >= 0 && cell <= 8 && board[cell] == ' ' &&
              (symbol == 'X' || symbol == 'O')) {
            board[cell] = symbol;

            char w = checkWinner();
            if (w == megaSymbol) {
              gameStatus = "Mega wins!";
              gameActive = false;
              if (tttSeriesP1 == 2) tttScoreP1++;
              else                  tttScoreP2++;
              tttPendingReset = true;
              tttResetTimer   = millis();
            } else if (w == 'D') {
              gameStatus = "Draw!";
              gameActive = false;
              tttPendingReset = true;
              tttResetTimer   = millis();
            } else {
              webTurn = true;
              gameStatus = "Your turn";
            }

            Serial.print("TTT: Mega played cell ");
            Serial.print(cell); Serial.print(" as "); Serial.println(symbol);
          }
        }
      }
    }
  }

  //  Serial Monitor → LoRa (raw debug, unchanged behaviour)  
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      loraSend(msg);

    }
  }
}