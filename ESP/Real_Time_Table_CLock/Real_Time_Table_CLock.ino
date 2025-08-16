
 * RTC Module (DS3231):
 * - VCC -> 3.3V (or 5V if your module requires it)
 * - GND -> GND
 * - SCL -> GPIO22
 * - SDA -> GPIO21
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

// Pin definitions for TFT
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define TFT_SCLK  18
#define TFT_MOSI  23

// I2C pins for RTC
#define SDA_PIN   21
#define SCL_PIN   22

// Display and RTC objects
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
RTC_DS3231 rtc;

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160

// Color definitions
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF
#define ORANGE   0xFD20
#define PURPLE   0x780F
#define PINK     0xFE19
#define LIME     0x07FF
#define NAVY     0x000F
#define MAROON   0x7800

// RTC status variables
bool rtcFound = false;
bool useInternalTime = false;
unsigned long startTime = 0;

// Display mode and timing variables
enum ClockStyle { 
  DIGITAL_CLASSIC = 0, 
  ANALOG_MODERN = 1, 
  NEON_STYLE = 2, 
  RETRO_LCD = 3, 
  MATRIX_STYLE = 4 
};

ClockStyle currentClockStyle = DIGITAL_CLASSIC;

unsigned long lastStyleSwitch = 0;
unsigned long lastTimeUpdate = 0;
const unsigned long STYLE_DISPLAY_TIME = 8000;  // 8 seconds per style - more stable
const unsigned long TIME_UPDATE_INTERVAL = 500;  // Update every 500ms - smoother

// Time tracking to prevent unnecessary redraws
int lastHour = -1;
int lastMinute = -1;
int lastSecond = -1;
bool forceRedraw = true;  // Force redraw on style change

// Animation variables (minimal and controlled)
int subtleAnimation = 0;

// WiFi credentials - UPDATE THESE
const char* ssid = "harshal";
const char* password = "harshal27";

// Function prototypes
void setupDisplay();
void setupRTC();
void displayStartupAnimation();
void updateDisplay();
void drawClockStyle(ClockStyle style);
void drawDigitalClassic();
void drawAnalogModern();
void drawNeonStyle();
void drawRetroLCD();
void drawMatrixStyle();
void smoothStyleTransition();
DateTime getCurrentTime();
void syncTimeWithNTP();
void updateTimeOnly();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n==================================================");
  Serial.println("Stable ESP32 TFT Clock v5.0 - No Flickering");
  Serial.println("==================================================");
  
  startTime = millis();
  
  setupDisplay();
  setupRTC();
  
  if (rtcFound) {
    syncTimeWithNTP();
  }
  
  displayStartupAnimation();
  
  Serial.println("=== Setup Complete! ===");
  lastStyleSwitch = millis();
  lastTimeUpdate = millis();
  forceRedraw = true;
}

void loop() {
  unsigned long currentTime = millis();
  
  // Switch between different clock styles (less frequent)
  if (currentTime - lastStyleSwitch > STYLE_DISPLAY_TIME) {
    Serial.println("Switching clock style...");
    smoothStyleTransition();
    currentClockStyle = (ClockStyle)((currentClockStyle + 1) % 5);
    Serial.println("New style: " + String(currentClockStyle));
    lastStyleSwitch = currentTime;
    forceRedraw = true;  // Force complete redraw on style change
  }
  
  // Update time display (more frequent but smart)
  if (currentTime - lastTimeUpdate > TIME_UPDATE_INTERVAL) {
    updateDisplay();
    lastTimeUpdate = currentTime;
  }
  
  // Minimal subtle animation increment
  if (currentTime % 2000 == 0) {
    subtleAnimation = (subtleAnimation + 1) % 60;
  }
  
  delay(100); // Stable delay
}

void setupDisplay() {
  Serial.println("Initializing stable TFT display...");
  
  tft.initR(INITR_BLACKTAB);
  delay(100);
  
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  
  Serial.println("✓ Stable display initialized");
}

void setupRTC() {
  Serial.println("Setting up RTC...");
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  
  if (rtc.begin()) {
    rtcFound = true;
    Serial.println("✓ DS3231 RTC initialized successfully");
    
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    rtcFound = false;
    useInternalTime = true;
    Serial.println("✗ RTC not found, using internal timer");
  }
}

void syncTimeWithNTP() {
  Serial.println("Attempting NTP sync...");
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    
    if (getLocalTime(&timeinfo)) {
      rtc.adjust(DateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      ));
      Serial.println("✓ Time synchronized with NTP");
    }
    
    WiFi.disconnect();
  }
}

void displayStartupAnimation() {
  tft.fillScreen(BLACK);
  
  // Simple, stable startup
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(25, 60);
  tft.println("STABLE");
  delay(800);
  
  tft.setTextColor(YELLOW);
  tft.setCursor(15, 80);
  tft.println("TFT CLOCK");
  delay(800);
  
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(45, 105);
  tft.println("v5.0");
  delay(1000);
  
  // Simple fade out
  tft.fillScreen(BLACK);
  delay(300);
}

DateTime getCurrentTime() {
  if (rtcFound) {
    return rtc.now();
  } else {
    unsigned long elapsed = millis() - startTime;
    DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
    return DateTime(compileTime.unixtime() + elapsed/1000);
  }
}

void updateDisplay() {
  DateTime now = getCurrentTime();
  
  // Only redraw if time changed or forced
  if (forceRedraw || now.hour() != lastHour || now.minute() != lastMinute || now.second() != lastSecond) {
    drawClockStyle(currentClockStyle);
    
    lastHour = now.hour();
    lastMinute = now.minute();
    lastSecond = now.second();
    forceRedraw = false;
  }
}

void drawClockStyle(ClockStyle style) {
  switch(style) {
    case DIGITAL_CLASSIC:
      drawDigitalClassic();
      break;
    case ANALOG_MODERN:
      drawAnalogModern();
      break;
    case NEON_STYLE:
      drawNeonStyle();
      break;
    case RETRO_LCD:
      drawRetroLCD();
      break;
    case MATRIX_STYLE:
      drawMatrixStyle();
      break;
  }
}

void drawDigitalClassic() {
  if (forceRedraw) {
    tft.fillScreen(BLACK);
    
    // Static border - no animation
    tft.drawRect(2, 2, SCREEN_WIDTH-4, SCREEN_HEIGHT-4, CYAN);
    tft.drawRect(3, 3, SCREEN_WIDTH-6, SCREEN_HEIGHT-6, BLUE);
  }
  
  DateTime now = getCurrentTime();
  
  // Clear only the time area
  tft.fillRect(5, 35, SCREEN_WIDTH-10, 40, BLACK);
  
  // Large digital time
  char timeStr[9];
  int hour = now.hour();
  bool isPM = hour >= 12;
  
  if (hour == 0) hour = 12;
  else if (hour > 12) hour -= 12;
  
  sprintf(timeStr, "%2d:%02d:%02d", hour, now.minute(), now.second());
  
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(8, 45);
  tft.println(timeStr);
  
  // AM/PM
  tft.setTextColor(CYAN);
  tft.setTextSize(1);
  tft.setCursor(95, 50);
  tft.println(isPM ? "PM" : "AM");
  
  // Date (only redraw if forced)
  if (forceRedraw) {
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    tft.setTextColor(YELLOW);
    tft.setTextSize(1);
    tft.setCursor(20, 85);
    tft.println(days[now.dayOfTheWeek()]);
    
    tft.setTextColor(WHITE);
    tft.setCursor(15, 100);
    tft.print(months[now.month()-1]);
    tft.print(" ");
    tft.print(now.day());
    tft.print(", ");
    tft.println(now.year());
    
    // Status
    tft.setTextColor(rtcFound ? GREEN : ORANGE);
    tft.setCursor(20, 125);
    if (rtcFound) {
      float temp = rtc.getTemperature();
      tft.print("RTC: ");
      tft.print(temp, 1);
      tft.println("C");
    } else {
      tft.println("Internal Timer");
    }
  }
}

void drawAnalogModern() {
  DateTime now = getCurrentTime();
  
  if (forceRedraw) {
    tft.fillScreen(BLACK);
  }
  
  // Clear clock area only
  tft.fillCircle(SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, 50, BLACK);
  
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2 - 10;
  int radius = 45;
  
  // Static outer circle
  tft.drawCircle(centerX, centerY, radius + 2, WHITE);
  tft.drawCircle(centerX, centerY, radius + 1, CYAN);
  
  // Draw hour markers (static)
  for (int i = 0; i < 12; i++) {
    float angle = (i * 30 - 90) * PI / 180;
    int x1 = centerX + (radius - 8) * cos(angle);
    int y1 = centerY + (radius - 8) * sin(angle);
    int x2 = centerX + (radius - 3) * cos(angle);
    int y2 = centerY + (radius - 3) * sin(angle);
    
    uint16_t markerColor = (i % 3 == 0) ? WHITE : CYAN;
    tft.drawLine(x1, y1, x2, y2, markerColor);
  }
  
  // Calculate hand positions
  float hourAngle = ((now.hour() % 12) * 30 + now.minute() * 0.5 - 90) * PI / 180;
  float minuteAngle = (now.minute() * 6 - 90) * PI / 180;
  float secondAngle = (now.second() * 6 - 90) * PI / 180;
  
  // Hour hand (thick)
  int hourX = centerX + 20 * cos(hourAngle);
  int hourY = centerY + 20 * sin(hourAngle);
  tft.drawLine(centerX, centerY, hourX, hourY, WHITE);
  tft.drawLine(centerX+1, centerY, hourX+1, hourY, WHITE);
  tft.drawLine(centerX, centerY+1, hourX, hourY+1, WHITE);
  
  // Minute hand
  int minuteX = centerX + 35 * cos(minuteAngle);
  int minuteY = centerY + 35 * sin(minuteAngle);
  tft.drawLine(centerX, centerY, minuteX, minuteY, CYAN);
  tft.drawLine(centerX+1, centerY, minuteX+1, minuteY, CYAN);
  
  // Second hand
  int secondX = centerX + 38 * cos(secondAngle);
  int secondY = centerY + 38 * sin(secondAngle);
  tft.drawLine(centerX, centerY, secondX, secondY, RED);
  
  // Center dot
  tft.fillCircle(centerX, centerY, 3, WHITE);
  tft.fillCircle(centerX, centerY, 2, BLACK);
  tft.fillCircle(centerX, centerY, 1, WHITE);
  
  // Digital time at bottom (only if forced)
  if (forceRedraw || now.minute() != lastMinute) {
    tft.fillRect(35, 135, 60, 15, BLACK);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
    tft.setTextColor(YELLOW);
    tft.setTextSize(1);
    tft.setCursor(45, 140);
    tft.println(timeStr);
  }
}

void drawNeonStyle() {
  DateTime now = getCurrentTime();
  
  if (forceRedraw) {
    tft.fillScreen(BLACK);
    
    // Static neon border
    tft.drawRect(1, 1, SCREEN_WIDTH-2, SCREEN_HEIGHT-2, MAGENTA);
    tft.drawRect(2, 2, SCREEN_WIDTH-4, SCREEN_HEIGHT-4, CYAN);
  }
  
  // Clear time area
  tft.fillRect(5, 40, SCREEN_WIDTH-10, 35, BLACK);
  
  // Neon-style time
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  
  // Glow effect (draw text multiple times with slight offsets)
  tft.setTextColor(MAGENTA);
  tft.setTextSize(2);
  tft.setCursor(6, 51);
  tft.println(timeStr);
  
  tft.setTextColor(CYAN);
  tft.setCursor(7, 50);
  tft.println(timeStr);
  
  // Main bright text
  tft.setTextColor(WHITE);
  tft.setCursor(8, 49);
  tft.println(timeStr);
  
  // Date in neon style (only if forced)
  if (forceRedraw) {
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    
    tft.setTextColor(MAGENTA);
    tft.setTextSize(1);
    tft.setCursor(26, 91);
    tft.print("DAY ");
    tft.println(now.day());
    
    tft.setTextColor(CYAN);
    tft.setCursor(30, 105);
    tft.println(months[now.month()-1]);
    
    tft.setCursor(25, 120);
    tft.println(now.year());
  }
}

void drawRetroLCD() {
  DateTime now = getCurrentTime();
  
  if (forceRedraw) {
    // Retro green background
    tft.fillScreen(BLACK);
    tft.fillRect(5, 5, SCREEN_WIDTH-10, SCREEN_HEIGHT-10, 0x0020); // Very dark green
    
    // LCD-style border
    tft.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GREEN);
    tft.drawRect(1, 1, SCREEN_WIDTH-2, SCREEN_HEIGHT-2, LIME);
    tft.drawRect(4, 4, SCREEN_WIDTH-8, SCREEN_HEIGHT-8, GREEN);
  }
  
  // Clear time area
  tft.fillRect(8, 30, SCREEN_WIDTH-16, 35, 0x0020);
  
  // Retro time display
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  
  tft.setTextColor(LIME);
  tft.setTextSize(2);
  tft.setCursor(8, 35);
  tft.println(timeStr);
  
  // Static cursor
  tft.fillRect(110, 50, 8, 2, GREEN);
  
  // Date (only if forced)
  if (forceRedraw) {
    tft.setTextColor(GREEN);
    tft.setTextSize(1);
    tft.setCursor(15, 75);
    tft.printf("%04d-%02d-%02d", now.year(), now.month(), now.day());
    
    const char* days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
    tft.setCursor(20, 95);
    tft.println(days[now.dayOfTheWeek()]);
    
    // Status line
    tft.setTextColor(LIME);
    tft.setCursor(10, 125);
    tft.print("READY");
    
    tft.setCursor(80, 125);
    tft.print("*");
  }
}

void drawMatrixStyle() {
  DateTime now = getCurrentTime();
  
  if (forceRedraw) {
    tft.fillScreen(BLACK);
    
    // Static matrix-style background pattern
    for (int x = 0; x < SCREEN_WIDTH; x += 8) {
      for (int y = 35; y < SCREEN_HEIGHT; y += 12) {
        if (random(10) < 2) { // 20% chance
          char c = random(33, 126);
          tft.setTextColor(0x0320); // Very dark green
          tft.setTextSize(1);
          tft.setCursor(x, y);
          tft.print(c);
        }
      }
    }
  }
  
  // Clear time area
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, BLACK);
  
  // Matrix-style time display (always on top)
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  
  tft.setTextColor(0x07E0); // Bright green
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("MATRIX TIME:");
  
  tft.setTextColor(LIME);
  tft.setTextSize(2);
  tft.setCursor(15, 15);
  tft.println(timeStr);
}

void smoothStyleTransition() {
  // Simple fade transition - no flickering
  for (int fade = 0; fade < 3; fade++) {
    tft.fillScreen(BLACK);
    delay(100);
  }
}