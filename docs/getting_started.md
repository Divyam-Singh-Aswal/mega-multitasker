# Getting Started

This guide walks you through setting up Mega Multitasker from scratch — wiring the hardware, installing dependencies, flashing the firmware, and verifying that everything works before you start using the device.

---

## What you need

**Hardware**

| Item                          | Notes                                         |
|-------------------------------|-----------------------------------------------|
| Arduino Mega 2560             | Any clone works |
| MCUFRIEND 2.4" TFT shield     | 240×320, SPFD5408 or compatible |
| ESP32 WROOM-32 × 2            | One for ESP1 (bridge), one for ESP2 (web hub) |
| Ra-02 SX1278 LoRa module × 2  | 433 MHz, one per ESP32 |
| NEO-6M GPS module             | With antenna(ceramic for this one) |
| DHT11 sensor                  |
| Passive buzzer or  speaker    |
| I2C Bi-Directional Logic Level Converter (4-channel)|
| Breadboard and jumper wires   | Until PCB is ready |
| USB cables                    | One for Mega(male type-A to male type-B), one per ESP32 during flashing (male type-A to male type micro-B)|
| 5V power supply               | USB or regulated, min 1A |

**Software**

- [Arduino IDE 2.x](https://www.arduino.cc/en/software) or Arduino CLI
- [ESP-IDF Arduino core](https://github.com/espressif/arduino-esp32) — install via Arduino Boards Manager, search `esp32`

---

## Step 1 — Install libraries

Open Arduino IDE, go to **Sketch → Include Library → Manage Libraries**, and install each of the following:

**For the Mega sketch:**

| Library | Author |
|---|---|
| Adafruit GFX Library | Adafruit |
| MCUFRIEND_kbv | David Prentice |
| TouchScreen | Adafruit |
| DHT sensor library | Adafruit |
| TinyGPS++ | Mikal Hart |

**For both ESP sketches:**

| Library | Author |
|---|---|
| LoRa | Sandeep Mistry |

The ESP32 built-in libraries (WiFi, WebServer, BLEDevice, BLEScan, HTTPClient) are included automatically once the ESP32 board core is installed — no separate installation needed.

---

## Step 2 — Wire the hardware

### Level shifting

The Mega runs at 5V logic. Both ESP32 modules run at 3.3V. An I2C Bi-Directional Logic Level Converter (4-channel) is used to safely shift signals between the two voltage domains.

### Level Shifter Connections

| Direction | Mega Pin | Level Shifter | ESP32 Pin | Notes |
|-----------|----------|---------------|-----------|-------|
| Power | 5V | HV | — | Mega side high voltage |
| Power | GND | GND | GND | Common ground |
| Data | D16 (TX2) | HV1 → LV1 | GPIO16 (RX2) | 5V → 3.3V shifted |
| Data | D17 (RX2) | — | GPIO17 (TX2) | Direct, no shift needed |
| Power | — | LV | 3.3V | ESP32 side low voltage |

**Key point:** Only shift Mega TX → ESP32 RX. The reverse direction works directly.

Only the Mega TX → ESP32 RX direction requires level shifting (5V down to 3.3V). The ESP32 TX → Mega RX direction (3.3V into a 5V-tolerant Mega pin) is connected directly without a shifter.

### Mega connections

| Signal      | Mega Pin     | Connects to |
|-------------|--------------|-------------|
| TFT shield  | D0–D9, A0–A4 | Shield plugs directly onto headers |
| DHT11 data  | D22          | DHT11 data pin |
| Speaker     | D23          | Speaker positive, GND to negative |
| GPS TX      | D19 (RX1)    | NEO-6M TX |
| GPS RX      | D18 (TX1)    | NEO-6M RX |
| ESP1 RX     | D17 (RX2)    | ESP1 GPIO17 |
| ESP1 TX     | D16 (TX2)    | ESP1 GPIO16 *(via level shifter ,mega--> HV1, esp --> LV1 )* |

### ESP1 connections

| Signal     | ESP1 GPIO | Connects to |
|------------|-----------|-------------|
| UART RX    | 16        | Mega D16 *(via level shifter ,esp --> LV1 ,mega--> HV1)* |
| UART TX    | 17        | Mega D17 |
| LoRa NSS   | 5         | Ra-02 NSS |
| LoRa RST   | 14        | Ra-02 RST |
| LoRa DIO0  | 2         | Ra-02 DIO0 |
| LoRa SCK   | 18        | Ra-02 SCK |
| LoRa MISO  | 19        | Ra-02 MISO |
| LoRa MOSI  | 23        | Ra-02 MOSI |
| 3.3V       | 3V3       | Ra-02 VCC, ESP1 EN |
| GND        | GND       | Ra-02 GND |

> LoRa Ra-02 is strictly 3.3V. Never connect its VCC or any signal line to 5V.

### ESP2 connections

ESP2 has its own Ra-02 module wired identically to ESP1's LoRa connections above. ESP2 has no UART connection to the Mega — it communicates only over LoRa.

| Signal     | ESP2 GPIO | Connects to |
|------------|-----------|-------------|
| LoRa NSS   | 5         | Ra-02 NSS |
| LoRa RST   | 14        | Ra-02 RST |
| LoRa DIO0  | 2         | Ra-02 DIO0 |
| LoRa SCK   | 18        | Ra-02 SCK |
| LoRa MISO  | 19        | Ra-02 MISO |
| LoRa MOSI  | 23        | Ra-02 MOSI |
| 3.3V       | 3V3       | Ra-02 VCC, ESP2 EN |
| GND        | GND       | Ra-02 GND |

---

## Step 3 — Flash the firmware

Flash each device separately. Do not connect the Mega UART lines to the ESP32s during flashing — disconnect Serial2 wires from the Mega before flashing ESP1, then reconnect afterward.

### Flashing ESP2 first

ESP2 is standalone and has no dependency on Mega or ESP1 being present. Flash it first so it is ready to receive.

1. Open `firmware/esp2/esp2_web_hub.ino` in Arduino IDE
2. Select board: **ESP32 Dev Module**
3. Select the correct COM port
4. Click Upload
5. Open Serial Monitor at **115200 baud**
6. You should see:

```
LoRa ready
AP started — IP: 192.168.4.1
Web server started
```

### Flashing ESP1

1. Disconnect the Serial2 wires between Mega and ESP1
2. Open `firmware/esp1/esp1_bridge.ino`
3. Select board: **ESP32 Dev Module**
4. Upload
5. Open Serial Monitor at **115200 baud**
6. You should see:

```
LoRa Bridge Starting...
LoRa Ready
```

Reconnect the Serial2 wires after flashing.

### Flashing the Mega

1. Open `firmware/mega/mega_multitasker.ino`
2. Select board: **Arduino Mega or Mega 2560**
3. Select **ATmega2560** processor
4. Select the correct COM port
5. Upload
6. Open Serial Monitor at **9600 baud**
7. You should see:

```
Boot OK
```

The TFT should light up and show the home screen with app icons.

---

## Step 4 — Verify LoRa link

With both ESP1 and ESP2 powered and their Serial Monitors open:

1. On the Mega Serial Monitor, type `MEGA:ESP2:CHAT:hello` and press Enter
2. In ESP1's Serial Monitor you should see the message forwarded over LoRa
3. In ESP2's Serial Monitor you should see it received
4. Connect a phone or laptop to the **Hub** WiFi network (password: `12345678`)
5. Open a browser and go to `http://192.168.4.1`
6. The message should appear in the Chat tab

If nothing appears on ESP2, check:
- Both Ra-02 modules are on the same frequency (433 MHz)
- LoRa VCC is 3.3V not 5V
- NSS/RST/DIO0 pins are wired correctly on both ends

---

## Step 5 — Verify GPS

1. Power on the Mega with the GPS module connected and antenna pointing toward a window or outside
2. Navigate to the GPS app on the TFT
3. The status card will show **Searching...** until a fix is acquired
4. First fix can take 1–5 minutes outdoors, longer indoors

If GPS never gets a fix indoors, this is normal. The NEO-6M needs a clear sky view.

---

## Step 6 — Verify sensors

**DHT11:** Navigate to the Temperature app. You should see a live reading within a few seconds. If it shows `--.-C`, check the data wire on D22 and connection.

**Speaker:** On startup the device plays a short boot chime. If you hear nothing, check the wire on D23 and verify speaker connection or your buzzer is passive (active buzzers do not respond to `tone()`) if you are using a buzzer.

---

## Using the web interface

Once connected to **Hub** WiFi:

- Open `http://192.168.4.1` in any browser
- **Chat tab** — send messages to the Mega, receive replies in real time
- **Tic Tac Toe tab** — request a game with the Mega, pick a side, and play

The page polls the server every second — no manual refresh needed.

---

## Communication protocol

All messages between devices follow the format:

```
SENDER:RECEIVER:TYPE:payload
```

For example:

```
MEGA:ESP2:CHAT:hello
MEGA:ESP1:NET:WIFI:SCAN
ESP2:MEGA:TTT:4:X
```

ESP1 routes messages automatically — anything addressed to `ESP2` is sent over LoRa, anything addressed to `ESP1` is handled locally. Full protocol reference is in `docs/protocol.md`.

---

## Troubleshooting

**TFT shows nothing or white screen**
- Check that the shield is fully seated on the Mega headers
- Try uploading and opening Serial Monitor — the boot message confirms the Mega is running even if the display is not

**ESP1 Serial Monitor shows nothing after boot**
- Check Serial Monitor baud is set to 115200
- Press the EN (reset) button on the ESP32

**Chat messages not appearing on ESP2 webpage**
- Open browser console (F12) and check for JSON parse errors — this usually means the ESP2 state JSON has a formatting issue
- Confirm ESP2 Serial Monitor shows `[LoRa IN]:` lines when messages are sent

**TFT touch not responding or registering wrong position**
- The touchscreen shares pins with the TFT data bus (XP=D8, YM=D9)
- If touch coordinates are inverted, the `TS_MINX/MAXX/MINY/MAXY` calibration values in the sketch may need adjusting for your specific shield revision

**LoRa not connecting**
- Confirm both modules are 433 MHz (not 868 or 915)
- Confirm VCC is 3.3V with a multimeter before powering on

---

## Known issues

- TTT online mode has intermittent state sync issues — if the game gets stuck, use the reset option on either side
- GPS clock sync requires a valid fix; until then the clock shows a default time
- PCB design is not yet complete — all connections are currently on breadboard

---

## What to do next

Once everything is working on breadboard, suggested next steps:

1. Read `docs/protocol.md` for the full message format if you want to add new features
2. Add your own app by creating a new `ScreenMode` entry and a matching draw and touch handler
3. Contribute fixes or improvements — open an issue or pull request on GitHub
