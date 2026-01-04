/*
  PID+Bot firmware (single-file sketch)
  - Serves dashboard from SPIFFS at 192.168.4.1
  - HTTP GET endpoints (no WebSockets):
      /mode?value=manual|pid
      /move?dir=F|B|L|R|S
      /pid?kp=..&ki=..&kd=..
      /accel?enable=1|0
      /telemetry  (JSON placeholders)
  - Motor driver: TB6612FNG (dual H-bridge)
  - Compatible with ESP8266 and ESP32 cores

  Notes:
  - Place `index.html` in SPIFFS/LittleFS before uploading or use the Arduino "Upload File System" tool.
  - Adjust pin mapping below to match your wiring to TB6612FNG.

  Created to work as a minimal, low-memory HTTP control surface for the mobile dashboard.
*/

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
typedef ESP8266WebServer WebServerClass;
#else
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
typedef WebServer WebServerClass;
#endif

#include <ArduinoJson.h>

// -------------------- Configurable pins (change to match your wiring) --------------------
// TB6612FNG: AIN1/AIN2 -> control motor A direction, PWMA -> speed A
//              BIN1/BIN2 -> control motor B direction, PWMB -> speed B
//              STBY -> standby (active HIGH)

// Default pins for NodeMCU / generic ESP boards (example mapping)
const uint8_t PIN_AIN1 = 12; // D6 on many boards
const uint8_t PIN_AIN2 = 13; // D7
const uint8_t PIN_PWMA = 14; // D5 (PWM)

const uint8_t PIN_BIN1 = 5;  // D1
const uint8_t PIN_BIN2 = 4;  // D2
const uint8_t PIN_PWMB = 15; // D8 (PWM)

const uint8_t PIN_STBY = 16; // D0 - standby

// PWM limits: we will scale 0..255
const uint8_t MAX_PWM = 255;
const uint8_t BASE_SPEED_PWM = 230; // default base speed for PID mode (user can change in firmware)

// -------------------- Globals --------------------
WebServerClass server(80);

String apSSID = "PID-BOT";
String apPass = ""; // empty = open AP; set a password if desired

volatile bool accelEnabled = false;
volatile bool isPidMode = false;

// PID parameters
volatile float Kp = 1.0f;
volatile float Ki = 0.0f;
volatile float Kd = 0.0f;

// Telemetry placeholders
volatile int sensorL = 0;
volatile int sensorR = 0;
volatile float lastError = 0.0f;
volatile int motorLeftSpd = 0;
volatile int motorRightSpd = 0;
volatile float pidOutput = 0.0f;

// Mode enum-ish
enum MoveDir { DIR_STOP, DIR_FWD, DIR_BWD, DIR_LEFT, DIR_RIGHT };

// -------------------- Motor control helpers --------------------
void tbStandby(bool on){
  digitalWrite(PIN_STBY, on ? HIGH : LOW);
}

// low-level motor A/B control
void motorA(int pwm, bool forward){
  digitalWrite(PIN_AIN1, forward ? HIGH : LOW);
  digitalWrite(PIN_AIN2, forward ? LOW : HIGH);
#if defined(ESP8266)
  analogWrite(PIN_PWMA, pwm);
#else
  ledcWrite(0, pwm);
#endif
  motorLeftSpd = pwm;
}

void motorB(int pwm, bool forward){
  digitalWrite(PIN_BIN1, forward ? HIGH : LOW);
  digitalWrite(PIN_BIN2, forward ? LOW : HIGH);
#if defined(ESP8266)
  analogWrite(PIN_PWMB, pwm);
#else
  ledcWrite(1, pwm);
#endif
  motorRightSpd = pwm;
}

void motorsStop(){
  // Brake both motors by setting both direction pins low and 0 PWM
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
#if defined(ESP8266)
  analogWrite(PIN_PWMA, 0);
#else
  ledcWrite(0, 0);
#endif

  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
#if defined(ESP8266)
  analogWrite(PIN_PWMB, 0);
#else
  ledcWrite(1, 0);
#endif

  motorLeftSpd = motorRightSpd = 0;
}

// High-level movement commands based on direction
void moveDirection(MoveDir dir){
  tbStandby(true);
  switch(dir){
    case DIR_FWD:
      motorA(BASE_SPEED_PWM, true);
      motorB(BASE_SPEED_PWM, true);
      break;
    case DIR_BWD:
      motorA(BASE_SPEED_PWM, false);
      motorB(BASE_SPEED_PWM, false);
      break;
    case DIR_LEFT:
      // left turn: left motor slow/back, right motor forward
      motorA(BASE_SPEED_PWM/2, false);
      motorB(BASE_SPEED_PWM, true);
      break;
    case DIR_RIGHT:
      motorA(BASE_SPEED_PWM, true);
      motorB(BASE_SPEED_PWM/2, false);
      break;
    case DIR_STOP:
    default:
      motorsStop();
  }
}

// -------------------- HTTP handlers --------------------
void handleRoot(){
  // Serve index.html from SPIFFS
  if(SPIFFS.exists("/index.html")){
    File f = SPIFFS.open("/index.html","r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(200, "text/plain", "Upload index.html to SPIFFS");
  }
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}

void handleMode(){
  if(!server.hasArg("value")) { server.send(400,"text/plain","missing value"); return; }
  String v = server.arg("value");
  if(v == "manual"){
    isPidMode = false;
    motorsStop();
  } else if(v == "pid"){
    isPidMode = true;
    // in PID mode we expect sensors to drive motors; here we set base speed
    // actual PID loop not implemented in this minimal example
    motorA(BASE_SPEED_PWM, true);
    motorB(BASE_SPEED_PWM, true);
  }
  server.send(200, "text/plain", "OK");
}

void handleMove(){
  if(!server.hasArg("dir")) { server.send(400,"text/plain","missing dir"); return; }
  String d = server.arg("dir");
  if(d == "F") moveDirection(DIR_FWD);
  else if(d == "B") moveDirection(DIR_BWD);
  else if(d == "L") moveDirection(DIR_LEFT);
  else if(d == "R") moveDirection(DIR_RIGHT);
  else moveDirection(DIR_STOP);
  server.send(200, "text/plain", "OK");
}

void handlePid(){
  bool changed = false;
  if(server.hasArg("kp")) { Kp = server.arg("kp").toFloat(); changed = true; }
  if(server.hasArg("ki")) { Ki = server.arg("ki").toFloat(); changed = true; }
  if(server.hasArg("kd")) { Kd = server.arg("kd").toFloat(); changed = true; }
  if(changed) server.send(200, "text/plain", "OK"); else server.send(400,"text/plain","missing params");
}

void handleAccel(){
  if(!server.hasArg("enable")) { server.send(400,"text/plain","missing enable"); return; }
  String v = server.arg("enable");
  accelEnabled = (v == "1");
  server.send(200, "text/plain", accelEnabled ? "ENABLED" : "DISABLED");
}

void handleTelemetry(){
  // Minimal JSON response for UI placeholders
  StaticJsonDocument<256> doc;
  doc["sL"] = sensorL;
  doc["sR"] = sensorR;
  doc["err"] = lastError;
  doc["spdL"] = motorLeftSpd;
  doc["spdR"] = motorRightSpd;
  doc["pidOut"] = pidOutput;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// -------------------- Setup --------------------
void setupFileSystem(){
  if(!SPIFFS.begin(true)){
    // failed to mount
  }
}

void setupPins(){
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);

#if defined(ESP8266)
  // ESP8266 analogWrite frequency uses pin-specific timers; just use analogWrite
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT);
#else
  // ESP32: use ledc channels for PWM
  ledcSetup(0, 20000, 8); // channel 0, 20kHz, 8-bit
  ledcAttachPin(PIN_PWMA, 0);
  ledcSetup(1, 20000, 8);
  ledcAttachPin(PIN_PWMB, 1);
#endif

  tbStandby(true);
  motorsStop();
}

void setupWiFiAP(){
  WiFi.mode(WIFI_AP);
  if(apPass.length()>0) WiFi.softAP(apSSID.c_str(), apPass.c_str());
  else WiFi.softAP(apSSID.c_str());
}

void setupServer(){
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/move", HTTP_GET, handleMove);
  server.on("/pid", HTTP_GET, handlePid);
  server.on("/accel", HTTP_GET, handleAccel);
  server.on("/telemetry", HTTP_GET, handleTelemetry);
  server.begin();
}

void setup(){
  Serial.begin(115200);
  delay(100);
  setupFileSystem();
  setupPins();
  setupWiFiAP();
  setupServer();
  Serial.println();
  Serial.print("AP "); Serial.print(apSSID); Serial.println(" started");
  Serial.println("IP: 192.168.4.1");
}

// -------------------- Main loop --------------------
unsigned long lastPidRun = 0;
void loop(){
  server.handleClient();

  // Placeholder PID loop: in real project, read sensors (line sensors, accel), compute error
  // and set motor PWMs accordingly. Keep loop lightweight.
  if(isPidMode){
    unsigned long now = millis();
    if(now - lastPidRun >= 50){ // 20Hz loop
      lastPidRun = now;
      // Read sensors here (ADC/digital) and compute pidOutput
      // Example placeholders (user should replace with real sensor reads):
      // sensorL = analogRead(A0); sensorR = analogRead(A1);
      float error = 0.0; // compute from sensors
      lastError = error;
      // PID compute (simple form)
      static float integ = 0, prev = 0;
      integ += error * 0.05; // dt ~50ms
      float deriv = (error - prev) / 0.05;
      pidOutput = Kp*error + Ki*integ + Kd*deriv;
      prev = error;

      // apply pidOutput to motors (map to pwm)
      int outL = constrain((int)(BASE_SPEED_PWM - pidOutput), 0, MAX_PWM);
      int outR = constrain((int)(BASE_SPEED_PWM + pidOutput), 0, MAX_PWM);
      // direction assumed forward for line follower; adapt as needed
      motorA(outL, true);
      motorB(outR, true);
    }
  }
}
