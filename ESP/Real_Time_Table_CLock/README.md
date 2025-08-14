# ⏰ ESP32 TFT Real-Time Clock

This project is a custom-made digital clock built with ESP32 NodeMCU-32S, a 1.8-inch ST7735 TFT display, and a Real-Time Clock (RTC) module.  
It features smooth animations and multiple clock faces that change over time — making it not just functional, but fun to watch.

---

## 📸 Project Images

### Clock in Action
![Clock Display](imagesclock_display.jpg)

### Hardware Setup
![Hardware Setup](imageshardware_setup.jpg)

---

## 📌 Features
✅ Real-time clock display using an RTC module  
✅ Animated transitions between different clock faces  
✅ Runs entirely on ESP32 NodeMCU-32S (USB powered)  
✅ Bright and colorful display with ST7735 TFT  

---

## 🛠 Hardware Required
- ESP32 NodeMCU-32S
- 1.8-inch ST7735 TFT Display
- DS3231 or DS1307 RTC Module
- Jumper wires
- Breadboard (optional)

---

## 🔌 Wiring Diagram

 TFT Pin   ESP32 Pin 
---------------------
 VCC       3.3V      
 GND       GND       
 CS        GPIO 5    
 RST       GPIO 4    
 DC        GPIO 2    
 SCLK      GPIO 18   
 MOSI      GPIO 23   

RTC Module Connection
 RTC Pin   ESP32 Pin 
---------------------
 VCC       3.3V      
 GND       GND       
 SDA       GPIO 21   
 SCL       GPIO 22   

---

## 📷 How It Works
1. The RTC module keeps track of accurate time even when the ESP32 is off.
2. The TFT display shows the current time in different designs, changing automatically.
3. The ESP32 updates the display every second with smooth animations.

---

## 🚀 Getting Started
1. Install Arduino IDE and add ESP32 board support.
2. Install the following libraries
   - `WiFi.h`
   - `WebServer.h`
   - `Adafruit_GFX.h`
   - `Adafruit_ST7735.h`
   - `RTClib.h`
3. Connect the components according to the wiring table above.
4. Upload the `.ino` file to your ESP32.
5. Power it via USB and enjoy your custom digital clock.

---

## 👨‍💻 Author
Harshal Tapre  
🔗 [GitHub](httpsgithub.comharshaltapre)  [LinkedIn](httpslinkedin.cominharshaltapre)

---

 💡 Tip Put your images in a folder called `images` in your GitHub repo, and update the file names in the `![ ](images...)` links above so they show correctly.
