# ESP8266 Timer Control System

Web-based timer control for ESP8266 NodeMCU with automatic relay switching and real-time clock.

## Features

- Automatic relay control with scheduled timers
- Manual relay override
- Web interface for configuration
- Real-time clock with DS3231 module
- WiFi access point mode

## Hardware Required

- ESP8266 NodeMCU
- DS3231 RTC module
- Relay module
- Connecting wires

## Wiring

| Component | ESP8266 Pin |
|-----------|-------------|
| Relay Signal | D0 |
| RTC SDA | D1 |
| RTC SCL | D2 |
| RTC VCC | 3.3V |
| RTC GND | GND |

## Setup

1. Install Arduino libraries:
   - ESP8266WiFi
   - ESP8266WebServer
   - RTClib

2. Upload code to ESP8266

3. Connect to WiFi network:
   - Network: "Timer NodeMCU"
   - Password: "12345678"

4. Open browser to: http://192.168.4.1

## Usage

### Web Interface

- **Set Time**: Update system date and time
- **Configure Timers**: Set ON/OFF times and enable/disable
- **Manual Control**: Toggle relay in manual mode
- **Mode Switch**: Change between AUTO and MANUAL modes

### Default Settings

- ON Timer: 8:00 AM (disabled)
- OFF Timer: 8:00 PM (disabled)
- Mode: AUTO
- Relay: OFF

## Operation Modes

**AUTO Mode**: Timers control relay automatically
**MANUAL Mode**: User controls relay via web interface

## Troubleshooting

- **RTC Error**: Check I2C wiring (SDA/SCL pins)
- **No WiFi**: Reset ESP8266 and reconnect
- **Relay Not Working**: Verify D0 connection and relay power
- **Timers Not Running**: Enable AUTO mode and check timer settings

## Configuration

Change WiFi credentials in code:
```cpp
const char* ssid = "Timer NodeMCU";
const char* password = "12345678";
```

Modify default timer values:
```cpp
Timer onTimer = {8, 0, true, false};   // 8:00 AM
Timer offTimer = {20, 0, false, false}; // 8:00 PM
```