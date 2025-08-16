# ESP32 TFT Real-Time Clock

A custom-made animated digital desk clock built with ESP32 NodeMCU-32S, 1.8-inch ST7735 TFT display, and RTC module. Features smooth animations, multiple clock faces, and an interactive design that makes telling time more exciting.

---

## Project Images

### Clock in Action
![Clock Display](images/clock_display.jpg)

### Hardware Setup  
![Hardware Setup](images/hardware_setup.jpg)

---

## Features

- Real-time clock display using DS3231/DS1307 RTC module
- Animated transitions between different clock faces  
- USB powered - runs entirely on ESP32 NodeMCU-32S
- Colorful TFT graphics with high contrast display
- Compact design - simple breadboard build
- Auto face-changing - keeps the display fresh and engaging

---

## Hardware Required

| Component | Description |
|-----------|-------------|
| **ESP32 NodeMCU-32S** | Main microcontroller |
| **1.8" ST7735 TFT Display** | 128x160 color display |
| **DS3231/DS1307 RTC Module** | Real-time clock with battery backup |
| **Jumper Wires** | For connections |
| **Breadboard** | For prototyping (optional) |

---

## Wiring Connections

### TFT Display (ST7735) to ESP32
| TFT Pin | ESP32 Pin | Description |
|---------|-----------|-------------|
| VCC     | 3.3V      | Power supply |
| GND     | GND       | Ground |
| CS      | GPIO 5    | Chip Select |
| RST     | GPIO 4    | Reset |
| DC      | GPIO 2    | Data/Command |
| SCLK    | GPIO 18   | SPI Clock |
| MOSI    | GPIO 23   | SPI Data |

### RTC Module to ESP32
| RTC Pin | ESP32 Pin | Description |
|---------|-----------|-------------|
| VCC     | 3.3V      | Power supply |
| GND     | GND       | Ground |
| SDA     | GPIO 21   | I2C Data |
| SCL     | GPIO 22   | I2C Clock |

---

## How It Works

1. **RTC Module** maintains accurate time even when ESP32 is powered off (thanks to backup battery)
2. **ESP32** reads time data from RTC via I2C communication
3. **TFT Display** renders the current time with animated clock faces
4. **Auto-cycling** between different visual styles keeps the display engaging
5. **USB Power** makes it completely portable - just plug and go

---

## Getting Started

### Prerequisites
- Arduino IDE installed
- ESP32 board package added to Arduino IDE

### Required Libraries
Install these libraries through Arduino IDE Library Manager:

```
- WiFi.h (ESP32 Core)
- WebServer.h (ESP32 Core) 
- Adafruit_GFX.h
- Adafruit_ST7735.h
- RTClib.h
```

### Setup Steps
1. **Clone or Download** this repository
2. **Install libraries** listed above
3. **Wire components** according to the connection table
4. **Upload** the .ino file to your ESP32
5. **Power via USB** and enjoy your animated clock

---

## Project Structure

```
ESP32-TFT-Clock/
├── ESP32_TFT_Clock.ino     # Main Arduino sketch
├── README.md               # This file
└── images/                 # Project photos
    ├── clock_display.jpg
    └── hardware_setup.jpg
```

---

## Clock Faces

The clock automatically cycles through different visual styles:
- Digital display with large numbers
- Animated transitions between face changes
- Color variations to keep it visually interesting
- Smooth updates every second

---

## Customization

You can easily modify:
- **Clock face designs** - edit the display functions
- **Animation speed** - adjust timing variables  
- **Color schemes** - change the color definitions
- **Face rotation timing** - modify the auto-change intervals

---

## Troubleshooting

**Display not working?**
- Check all wiring connections
- Verify 3.3V power supply
- Ensure correct GPIO pins are used

**Time not accurate?**
- RTC module needs initial time setting
- Check I2C connections (SDA/SCL)
- Verify RTC battery is installed

**Compilation errors?**
- Install all required libraries
- Select correct ESP32 board in Arduino IDE

---

## License

This project is open source. Feel free to modify and share.

---

## Author

**Harshal Tapre**  
GitHub: [https://github.com/harshaltapre](https://github.com/harshaltapre)  
LinkedIn: [https://www.linkedin.com/in/harshal-tapre-650a1b251/](https://www.linkedin.com/in/harshal-tapre-650a1b251/)

---

## Contributing

Found a bug or have an improvement? Feel free to:
- Open an issue
- Submit a pull request
- Share your custom clock faces

---

If you found this project helpful, please give it a star on GitHub.
