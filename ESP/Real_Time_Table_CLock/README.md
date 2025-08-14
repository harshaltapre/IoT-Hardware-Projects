# ⏰ ESP32 TFT Real-Time Clock

This project is a **custom-made animated desk clock** built using an **ESP32 NodeMCU-32S**, **1.8-inch ST7735 TFT display**, and an **RTC module**.  
It features **smooth animations**, **multiple clock faces**, and a **fun, interactive design** that makes telling time more exciting.

---

## 📸 Project Images

### Clock Faces & Animations
![Clock Face](images/face.png)

### Clock in Action
![Clock Display](images/Hardwareimage.jpg)

### Hardware Setup
![Hardware Setup](images/hardare.jpg)

---

## 📌 Features
✅ Real-time clock display using an RTC module  
✅ Animated transitions between different clock faces  
✅ Runs entirely on ESP32 NodeMCU-32S (USB powered)  
✅ Colorful, high-contrast TFT graphics  
✅ Simple and compact breadboard build  

---

## 🛠 Hardware Required
- **ESP32 NodeMCU-32S**
- **1.8-inch ST7735 TFT Display**
- **DS3231 / DS1307 RTC Module**
- **Jumper Wires**
- **Breadboard** (optional for prototyping)

---

## 🔌 Wiring Diagram

**TFT Display to ESP32**
| TFT Pin | ESP32 Pin |
|---------|-----------|
| VCC     | 3.3V      |
| GND     | GND       |
| CS      | GPIO 5    |
| RST     | GPIO 4    |
| DC      | GPIO 2    |
| SCLK    | GPIO 18   |
| MOSI    | GPIO 23   |

**RTC Module to ESP32**
| RTC Pin | ESP32 Pin |
|---------|-----------|
| VCC     | 3.3V      |
| GND     | GND       |
| SDA     | GPIO 21   |
| SCL     | GPIO 22   |

---

## 📷 How It Works
1. **RTC Module** keeps accurate time even when the ESP32 is powered off.  
2. **ESP32** reads the time and sends it to the **TFT display**.  
3. Clock **faces change with animations** to make the display interactive.  
4. Device runs fully **USB powered**, making it portable.

---

## 🚀 Getting Started
1. Install **Arduino IDE** and add **ESP32 board support**.
2. Install these libraries:
   - `WiFi.h`
   - `WebServer.h`
   - `Adafruit_GFX.h`
   - `Adafruit_ST7735.h`
   - `RTClib.h`
3. Connect components according to the wiring table above.
4. Upload the `.ino` file to your ESP32.
5. Enjoy your custom **animated real-time clock**.

---

## 👨‍💻 Author
**Harshal Tapre**  
🔗 [GitHub](https://github.com/harshaltapre) | [LinkedIn](https://www.linkedin.com/in/harshal-tapre-650a1b251/)

---

💡 *Tip:* Keep all images in an `images` folder inside your GitHub repo for them to display correctly.
