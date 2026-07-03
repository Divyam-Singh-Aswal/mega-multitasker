# Communication Protocol

All messages exchanged between Mega, ESP1, and ESP2 follow a single format:

```
SENDER:RECEIVER:TYPE:payload
```

The first two fields identify who sent the message and who it is addressed to. The third field is the message type, which determines how the receiver handles it. Everything after the third colon is the payload, which can itself contain colons.

---

## Devices

| Identifier |                  Device          |
|------------|----------------------------------|
| MEGA       | Arduino Mega 2560                |
| ESP1       | ESP32 — LoRa bridge, WiFi, BLE   |
| ESP2       | ESP32 — Web server, LoRa receiver|

---

## Message Types

### CHAT

A plain text message between two parties.

```
MEGA:ESP2:CHAT:hello from mega
ESP2:MEGA:CHAT:hello from browser
```

The payload is the message text. No escaping is applied — colons in the message text are not supported.

---

### TTT

Tic Tac Toe game messages. All TTT messages between Mega and ESP2 are routed through ESP1 over LoRa.

**Game request and response:**

```
MEGA:ESP2:TTT:REQ
ESP2:MEGA:TTT:REQ:ACCEPT
ESP2:MEGA:TTT:REQ:DECLINE
MEGA:ESP2:TTT:LEFT
ESP2:MEGA:TTT:LEFT
```

**Symbol selection:**

```
MEGA:ESP2:TTT:PICK:X
ESP2:MEGA:TTT:PICK:O
```

Each side sends their chosen symbol. The other side receives the remaining symbol implicitly.

**Board move:**

```
MEGA:ESP2:TTT:4:X
ESP2:MEGA:TTT:6:O
```

Payload is `cell:symbol` where cell is 0-8 (row-major order, 0 = top-left, 8 = bottom-right) and symbol is X or O.

**Reset:**

```
MEGA:ESP2:TTT:RESET
```

---

### NET

Network commands handled locally by ESP1. These messages are never forwarded to ESP2.

**WiFi:**

```
MEGA:ESP1:NET:WIFI:SCAN
MEGA:ESP1:NET:WIFI:FETCH
MEGA:ESP1:NET:WIFI:CONNECT:NetworkName:password
MEGA:ESP1:NET:WIFI:DISCONNECT

ESP1:MEGA:NET:WIFI:SCAN:RESULT:3:HomeNet,-65;Work,-72;Guest,-80
ESP1:MEGA:NET:WIFI:CONNECTED:NetworkName:192.168.1.5
ESP1:MEGA:NET:WIFI:FAIL:Wrong password or timeout
ESP1:MEGA:NET:WIFI:DISCONNECTED
```

Scan is split into two steps. Mega sends SCAN to start, waits a fixed duration, then sends FETCH to retrieve results. Results are a comma-separated name/RSSI pair per network, semicolon-delimited.

**BLE:**

```
MEGA:ESP1:NET:BT:SCAN
MEGA:ESP1:NET:BT:FETCH
MEGA:ESP1:NET:BT:CONNECT:DeviceName
MEGA:ESP1:NET:BT:DISCONNECT

ESP1:MEGA:NET:BT:SCAN:RESULT:2:Device One;Device Two
ESP1:MEGA:NET:BT:CONNECTED:DeviceName
ESP1:MEGA:NET:BT:FAIL:reason
ESP1:MEGA:NET:BT:DISCONNECTED
```

Same two-step scan approach as WiFi. Device names are semicolon-delimited. If a device has no name, its MAC address is used instead.

**HTTP:**

```
MEGA:ESP1:NET:HTTP:GET:http://192.168.1.1
MEGA:ESP1:NET:HTTP:PING:8.8.8.8

ESP1:MEGA:NET:HTTP:RESPONSE:response text here
ESP1:MEGA:NET:HTTP:PING:8.8.8.8 reachable
```

HTTP response is trimmed to 100 characters before sending to Mega. Newlines are stripped.

---

## Notes

- Messages are newline-terminated (`\n`)
- Maximum practical message length is limited by the LoRa packet size and the Mega's serial buffer — keep payloads short
- Colons within the payload (after the third colon) are valid except in plain text fields like CHAT, where a colon would be misread as a delimiter by naive parsers
- ESP1 determines routing by inspecting the RECEIVER field. Messages addressed to ESP1 are handled locally. Messages addressed to ESP2 are sent over LoRa as-is.
