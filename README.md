# Mega Multitasker

A handheld multi-application device built on the Arduino Mega 2560 with a 2.4" TFT touchscreen. The device runs several independent applications through a custom touch-based UI, with wireless communication handled by two ESP32 modules over LoRa and WiFi.

---

## Hardware

|                Item             |             Notes                |
|---------------------------------|----------------------------------|
| Arduino Mega 2560               | Any clone works                  |
| MCUFRIEND 2.4" TFT shield       | 240×320, SPFD5408 or compatible  |
| ESP32 WROOM-32 × 2              | One for ESP1 (bridge), one for ESP2 (web hub) |
| Ra-02 SX1278 LoRa module × 2    | 433 MHz, one per ESP32           |
| NEO-6M GPS module               | With antenna (here used ceramic one)|
| DHT11 sensor                    |
| Passive buzzer or small speaker |
| Logic Level Converter           |I2C Bi-Directional (4-channel used here)|
| Breadboard and jumper wires     | Until PCB is ready               |
| USB cables                      | One for Mega(male type-A to male type-B), one per ESP32 during flashing (male type-A to male type micro-B)|
| 5V power supply                 | USB or regulated, min 1A         |
---

## Architecture

```
Mega 2560
  |-- Serial1 --> NEO-6M GPS
  |-- Serial2 --> Level Shifter --> ESP1 (LoRa bridge + WiFi STA + BLE central)
  |-- TFT Shield (8-bit parallel, D0-D9, A0-A4)

ESP1 <-- LoRa 433MHz --> ESP2
                          |-- WiFi AP (SSID: Hub)
                          |-- Web server on 192.168.4.1
```

For now all inter-device communication follows a structured protocol described in `docs/protocol.md`. ESP1 acts as a router — messages from 'Arduino mega' addressed to ESP2 are forwarded over LoRa through ESP1 , messages addressed to ESP1 are handled locally.


## Getting Started

See [`docs/getting_started.md`](docs/getting_started.md) for full hardware wiring, library installation, flashing order, and troubleshooting.

---

## Applications

**Clock** — Analog clock face with GPS-synced time (IST). Includes stopwatch and dual alarm with buzzer.

**GPS** — Four pages of GPS data: fix status and coordinates, altitude and speed, course and heading, HDOP and signal statistics.

**Temperature** — DHT11 temperature and humidity with exponential moving average smoothing.

**Chat** — LoRa-based text messaging between the Mega device and the ESP2 web interface. Messages appear in real time on both the TFT screen and the browser. Text entry via on-screen QWERTY keyboard.

**Tic Tac Toe** — Offline two-player mode on the touchscreen, and online mode over LoRa against a player on the ESP2 web interface. Includes symbol selection, series score tracking, and auto-reset between games.

**Network** — WiFi scanning and connection, BLE device scanning, and basic HTTP GET via ESP1. Paginated device lists with connect and disconnect controls on screen. WiFi password entry via on-screen keyboard.

**Draw** — Freehand drawing canvas with color palette and clear button.

**Web Interface (ESP2)** — Self-contained HTML page served from ESP2, accessible from any browser connected to the Hub WiFi network. Supports chat and online Tic Tac Toe. No external dependencies, polls server state every second.

---

## Input

Text entry across all applications is handled by a shared on-screen QWERTY keyboard. Any application that requires text input calls into the keyboard, which returns the typed string on confirmation. Currently used for WiFi password entry and chat message composition. Additional applications can use the same input system without modification.

---

## Pin Reference

### Mega 2560

|   Function                |         Pin(s)           |
|---------------------------|--------------------------| 
| TFT data bus              | D0 - D7                  |
| TFT control               | D8 (RS), D9 (WR), A0 (CS), A1 (RST), A4 (RD) |
| Touch XP / YM             | D8, D9                   |
| Touch XM / YP             | A2, A3                   |
| DHT11                     | D22                      |
| Speaker or Passive Buzzer | D23                      |
| GPS (Serial1)             | D18 TX, D19 RX           |
| ESP1 (Serial2)            | D16 TX, D17 RX           |

### ESP1 (LoRa + WiFi + BLE)

|            Function          |    GPIO       |
|------------------------------|---------------|
| UART to Mega                 |RX: 16, TX: 17 |
| LoRa NSS / RST / DIO0        | 5, 14, 2      |
| LoRa SPI (SCK / MISO / MOSI) | 18, 19, 23    |


**Note:** All ESP32 IO pins operate at 3.3V. A level shifter or resistor divider is required on the UART TX line from the Mega (5V) to each ESP32 RX pin. Connect Mega 5V → HV, ESP32 3.3V → LV, Mega TX2 (D16) → HV1, ESP32 RX2 (GPIO16) → LV1. ESP32 TX2 (GPIO17) connects directly to Mega RX2 (D17) — no shifting required for that direction.

### ESP2 (LoRa + WiFi)

|            Function          |    GPIO       |
|------------------------------|---------------|
| LoRa NSS / RST / DIO0        | 5, 14, 2      |
| LoRa SPI (SCK / MISO / MOSI) | 18, 19, 23    |

---

## Protocol

All messages follow the format:

```
SENDER:RECEIVER:TYPE:payload
```

Examples:

```
MEGA:ESP2:CHAT:hello
MEGA:ESP1:NET:WIFI:SCAN
ESP1:MEGA:NET:WIFI:SCAN:RESULT:3:HomeNet,-65;Work,-72;Guest,-80
ESP2:MEGA:TTT:4:X
```

Full protocol specification is in `docs/protocol.md`.

---

## Libraries

**Arduino Mega**
- Adafruit_GFX
- MCUFRIEND_kbv
- TouchScreen
- DHT sensor library
- TinyGPS++

**ESP1**
- LoRa (Sandeep Mistry)
- Built-in: WiFi, BLEDevice, BLEScan, HTTPClient

**ESP2**
- LoRa (Sandeep Mistry)
- Built-in: WiFi, WebServer
---

## Status

Core features are working and tested. The following are in progress or have known issues:

- TTT online mode has intermittent state sync issues between Mega and ESP2
- PCB design not yet complete — currently on breadboard
- Circuit diagram will be added once PCB design is finalised

---

## Repository Structure

```
mega-multitasker/
    firmware/
        mega/
            mega_multitasker.ino
        esp1/
            esp1_bridge.ino
        esp2/
            esp2_web_hub.ino
    hardware/
        connections.md
        pcb/
    docs/
        protocol.md
    README.md
    LICENSE
```

## License

MIT
