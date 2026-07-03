/*
  ESP1 — LoRa Bridge + WiFi + BLE
  Mega Serial2 (16/17) ↔ ESP1 ↔ LoRa ↔ ESP2
  ESP1 also handles WiFi and BLE on behalf of Mega
    
*/

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <HTTPClient.h>

//  LoRa pins  
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 2

//  UART to Mega  
#define MEGA_SERIAL  Serial2
#define MEGA_RX      16
#define MEGA_TX      17
#define MEGA_BAUD    9600

// BLE scan duration  
#define BLE_SCAN_SECS  5

// state  
bool     wifiConnected   = false;
String   wifiIP          = "";
BLEScan* pBLEScan        = nullptr;
bool     bleConnected    = false;
String   bleConnectedName = "";

//FORWARD DECLARATIONS
    
void megaSend(const String& msg);
void loraSend(const String& msg);
void handleESP1Command();
void doWifiScan();
void doWifiConnect(const String& ssid, const String& pass);
void doWifiDisconnect();
void doBLEScan();
void doBLEConnect(const String& name);
void doBLEDisconnect();
void doHttpGet(const String& url);
void doPing(const String& host);

//SEND TO MEGA
    
void megaSend(const String& msg) {
  MEGA_SERIAL.println(msg);
  Serial.println(msg);
}

//SEND OVER LORA
    
void loraSend(const String& msg) {
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  Serial.println(msg);
}

//ESP1 INCOMING COMMAND HANDLER
    
void handleESP1Command() {

  // check Mega UART
  if (MEGA_SERIAL.available()) {
    String msg = MEGA_SERIAL.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      Serial.println(msg);

      if (msg.startsWith("MEGA:ESP1:")) {
        String msg1 = msg.substring(10);

        if (msg1 == "NET:WIFI:SCAN") { doWifiScan(); return; }
        if (msg1.startsWith("NET:WIFI:CONNECT:")) {
          String payload = msg1.substring(17);
          int colon = payload.indexOf(':');
          if (colon > 0) {
            String ssid = payload.substring(0, colon);
            String pass = payload.substring(colon + 1);
            doWifiConnect(ssid, pass);
          }
          return;
        }
        if (msg1 == "NET:WIFI:DISCONNECT") { doWifiDisconnect(); return; }
        if (msg1 == "NET:WIFI:FETCH")      { doWifiFetch();      return; }
        if (msg1 == "NET:BT:SCAN")          { doBLEScan();        return; }
        if (msg1.startsWith("NET:BT:CONNECT:")) { doBLEConnect(msg1.substring(15)); return; }
        if (msg1 == "NET:BT:DISCONNECT")    { doBLEDisconnect();  return; }
        if (msg1 == "NET:BT:FETCH")         { doBLEFetch();       return; }
        if (msg1.startsWith("NET:HTTP:GET:"))  { doHttpGet(msg1.substring(13));  return; }
        if (msg1.startsWith("NET:HTTP:PING:")) { doPing(msg1.substring(14));     return; }

      } else if (msg.startsWith("MEGA:ESP2:")) {
        loraSend(msg);
      } else {
        Serial.println(msg);
      }
    }
  }

  // check LoRa (non-blocking)
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String lmsg = "";
    while (LoRa.available()) lmsg += (char)LoRa.read();
    lmsg.trim();
    if (lmsg.length() > 0) {
      Serial.println(lmsg);
      if (lmsg.startsWith("ESP2:MEGA:")) megaSend(lmsg);
      // ESP2:ESP1: handling can go here later
    }
  }
}

//WIFI FUNCTIONS
    
// stored wifi results between SCAN and FETCH 
String wifiScanResults = "";
uint8_t wifiScanCount  = 0;

void doWifiScan() {
  Serial.println(F("[WiFi] Scan started"));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();   // blocking ~2-3 seconds 

  wifiScanResults = "";
  wifiScanCount   = 0;

  if (n <= 0) {
    Serial.println(F("[WiFi] No networks found"));
    return;
  }

  uint8_t send_count = min(n, 10);
  wifiScanCount = send_count;

  for (int i = 0; i < send_count; i++) {
    wifiScanResults += WiFi.SSID(i);
    wifiScanResults += ",";
    wifiScanResults += String(WiFi.RSSI(i));
    if (i < send_count - 1) wifiScanResults += ";";
  }
  WiFi.scanDelete();
  Serial.print(F("[WiFi] Found: ")); Serial.println(wifiScanCount);
  doWifiFetch(); 
}

void doWifiFetch() {
  String result = "ESP1:MEGA:NET:WIFI:SCAN:RESULT:";
  result += String(wifiScanCount) + ":" + wifiScanResults;
  megaSend(result);
  wifiScanResults = "";
  wifiScanCount   = 0;
}

void doWifiConnect(const String& ssid, const String& pass) {
  Serial.print(F("[WiFi] Connecting to: ")); Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiIP = WiFi.localIP().toString();
    String response = "ESP1:MEGA:NET:WIFI:CONNECTED:" + ssid + ":" + wifiIP;
    megaSend(response);
    Serial.print(F("[WiFi] Connected. IP: ")); Serial.println(wifiIP);
  } else {
    wifiConnected = false;
    megaSend("ESP1:MEGA:NET:WIFI:FAIL:Wrong password or timeout");
    Serial.println(F("[WiFi] Connection failed"));
  }
}

void doWifiDisconnect() {
  WiFi.disconnect();
  wifiConnected = false;
  wifiIP = "";
  megaSend("ESP1:MEGA:NET:WIFI:DISCONNECTED");
  Serial.println(F("[WiFi] Disconnected"));
}

//BLE FUNCTIONS

// stored BLE results 
String bleScanResults = "";
uint8_t bleScanCount  = 0;

class MegaBLECallback : public BLEAdvertisedDeviceCallbacks {
  public: void onResult(BLEAdvertisedDevice device) override {
    if (bleScanCount >= 10) return;
    String name = device.getName().c_str();
    if (name.length() == 0)
      name = device.getAddress().toString().c_str();
    // dedup check 
    if (bleScanResults.indexOf(name) >= 0) return;
    if (bleScanCount > 0) bleScanResults += ";";
    bleScanResults += name;
    bleScanCount++;
    Serial.print(F("[BLE] Found: ")); Serial.println(name);
  }
};

MegaBLECallback bleCB;

void doBLEScan() {
  Serial.println(F("[BLE] Scan started"));
  bleScanResults = "";
  bleScanCount   = 0;

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&bleCB, true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(BLE_SCAN_SECS, false); 
  pBLEScan->stop();
  pBLEScan->clearResults();
  Serial.print(F("[BLE] Scan done. Found: ")); Serial.println(bleScanCount);
  doBLEFetch();
}

void doBLEFetch() {
  String result = "ESP1:MEGA:NET:BT:SCAN:RESULT:";
  result += String(bleScanCount) + ":" + bleScanResults;
  megaSend(result);
  bleScanResults = "";
  bleScanCount   = 0;
}

void doBLEConnect(const String& name) {
  // For BLE central connection a full GATT client is needed.This sends back a placeholder — extend with BLEClient when you have a specific BLE peripheral to target. 
  Serial.print(F("[BLE] Connect attempt: "));
  Serial.println(name);
  // placeholder — replace with actual BLEClient connect logic 
  bleConnected     = true;
  bleConnectedName = name;
  megaSend("ESP1:MEGA:NET:BT:CONNECTED:" + name);
}

void doBLEDisconnect() {
  bleConnected = false;
  bleConnectedName = "";
  megaSend("ESP1:MEGA:NET:BT:DISCONNECTED");
  Serial.println(F("[BLE] Disconnected"));
}

//HTTP FUNCTIONS
    
void doHttpGet(const String& url) {
  if (!wifiConnected) {
    megaSend("ESP1:MEGA:NET:HTTP:RESPONSE:No WiFi");
    return;
  }
  Serial.print(F("[HTTP] GET: ")); Serial.println(url);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(5000);
  int code = http.GET();

  if (code > 0) {
    String body = http.getString();
    // trim to 100 chars so it fits the Mega buffer 
    if (body.length() > 100) body = body.substring(0, 100);
    // remove newlines 
    body.replace("\n", " ");
    body.replace("\r", "");
    megaSend("ESP1:MEGA:NET:HTTP:RESPONSE:" + body);
    Serial.print(F("[HTTP] Code: ")); Serial.println(code);
  } else {
    megaSend("ESP1:MEGA:NET:HTTP:RESPONSE:Error " + String(code));
    Serial.print(F("[HTTP] Error: ")); Serial.println(code);
  }
  http.end();
}

void doPing(const String& host) {
  // ESP32 Arduino core doesn't include ping by default. We do a quick HTTP check to 8.8.8.8 as a reachability test. 
  Serial.print(F("[Ping] Testing: "));
  Serial.println(host);
  if (!wifiConnected) {
    megaSend("ESP1:MEGA:NET:HTTP:PING:No WiFi");
    return;
  }
  // simple TCP connect test on port 80 
  WiFiClient client;
  bool reachable = client.connect(host.c_str(), 80, 2000);
  client.stop();
  if (reachable) {
    megaSend("ESP1:MEGA:NET:HTTP:PING:" + host + " reachable");
  } else {
    // try DNS at least 
    megaSend("ESP1:MEGA:NET:HTTP:PING:No response from " + host);
  }
}

//SETUP
    
void setup() {
  Serial.begin(115200);
  MEGA_SERIAL.begin(MEGA_BAUD, SERIAL_8N1, MEGA_RX, MEGA_TX);
  delay(500);

  Serial.println(F("\n=== ESP1 LoRa+WiFi+BLE Bridge ==="));

  // LoRa 
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println(F("LoRa init FAILED"));
    megaSend("LORA_FAIL");
    while (1) delay(1000);
  }
  Serial.println(F("LoRa OK"));

  // WiFi — start in station mode, not connected yet 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println(F("WiFi ready"));

  // BLE 
  BLEDevice::init("mega");
  Serial.println(F("BLE ready"));

}

//LOOP
    
void loop() {
  handleESP1Command();
}