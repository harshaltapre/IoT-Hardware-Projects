#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <RTClib.h>

// Pin definitions
#define RELAY_PIN D0  // Relay module base pin
#define SDA_PIN D1    // SDA for RTC
#define SCL_PIN D2    // SCL for RTC

// WiFi credentials for hotspot
const char* ssid = "Timer NodeMCU";
const char* password = "12345678";

// Create objects
ESP8266WebServer server(80);
RTC_DS3231 rtc;

// Timer variables
struct Timer {
  int hour;
  int minute;
  bool isOn;
  bool enabled;
};

Timer onTimer = {8, 0, true, false};   // Default 8:00 AM
Timer offTimer = {20, 0, false, false}; // Default 8:00 PM
bool relayState = false;
bool autoMode = true;
unsigned long lastTimerCheck = 0;
bool lastMinuteProcessed[24][60] = {false}; // Track processed minutes
String lastDebugMessage = "";

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== NodeMCU Timer Control System Starting ===");
  
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;
  Serial.println("Relay pin initialized and set to OFF");
  
  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("I2C initialized on pins SDA:" + String(SDA_PIN) + " SCL:" + String(SCL_PIN));
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found! Check wiring.");
    Serial.println("System will continue but time functions won't work");
    while(1) {
      Serial.println("RTC ERROR - Please check connections");
      delay(5000);
    }
  }
  
  // Check and set RTC time if needed
  DateTime now = rtc.now();
  if (now.year() < 2020 || rtc.lostPower()) {
    Serial.println("RTC lost power or invalid time, setting to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    now = rtc.now();
  }
  
  Serial.println("RTC initialized successfully");
  Serial.println("Current RTC time: " + formatDateTime(now));
  
  // Setup WiFi Access Point
  Serial.println("Setting up WiFi Access Point...");
  WiFi.softAP(ssid, password);
  delay(2000);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("WiFi AP started successfully!");
  Serial.println("Network Name: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.println("IP Address: " + IP.toString());
  Serial.println("Open browser and go to: http://" + IP.toString());
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settime", HTTP_POST, handleSetTime);
  server.on("/settimer", HTTP_POST, handleSetTimer);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/debug", HTTP_GET, handleDebug);
  
  // Enable CORS for better browser compatibility
  server.enableCORS(true);
  
  server.begin();
  Serial.println("HTTP server started successfully");
  Serial.println("=== System Ready - All components initialized ===");
  
  // Initial relay state update
  updateRelay();
  
  // Print initial status
  printSystemStatus();
}

void loop() {
  server.handleClient();
  
  // Check timers every second in auto mode
  unsigned long currentMillis = millis();
  if (currentMillis - lastTimerCheck >= 1000) {
    lastTimerCheck = currentMillis;
    
    if (autoMode) {
      checkAndExecuteTimers();
    }
  }
  
  yield(); // Allow ESP8266 to handle background tasks
  delay(10); // Small delay for system stability
}

void handleRoot() {
  DateTime now = rtc.now();
  
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>";
  html += "<title>Smart Timer NodeMCU</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }";
  html += "body { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 10px; }";
  html += ".container { max-width: 950px; margin: 0 auto; background: rgba(255,255,255,0.98); padding: 25px; border-radius: 15px; box-shadow: 0 20px 40px rgba(0,0,0,0.3); }";
  
  // Header styles
  html += ".header { text-align: center; margin-bottom: 25px; border-bottom: 3px solid #3498db; padding-bottom: 15px; }";
  html += "h1 { color: #2c3e50; font-size: 2.5em; margin-bottom: 8px; text-shadow: 2px 2px 4px rgba(0,0,0,0.1); }";
  html += ".subtitle { color: #7f8c8d; font-size: 1.2em; font-weight: 500; }";
  html += ".connection-info { background: #ecf0f1; padding: 10px; border-radius: 8px; margin: 10px 0; font-size: 0.9em; color: #555; }";
  
  // Section styles
  html += ".section { margin: 20px 0; padding: 20px; background: #f8f9fa; border-radius: 12px; border-left: 4px solid #3498db; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h2 { color: #2c3e50; margin-bottom: 15px; font-size: 1.4em; display: flex; align-items: center; }";
  html += ".section-icon { margin-right: 10px; font-size: 1.3em; }";
  
  // Status card styles
  html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 15px 0; }";
  html += ".status-card { padding: 18px; border-radius: 10px; text-align: center; color: white; font-weight: 600; box-shadow: 0 6px 15px rgba(0,0,0,0.2); transition: transform 0.2s ease; }";
  html += ".status-card:hover { transform: translateY(-2px); }";
  html += ".status-card.time { background: linear-gradient(135deg, #3498db, #2980b9); }";
  html += ".status-card.relay-on { background: linear-gradient(135deg, #e74c3c, #c0392b); animation: pulse-red 2s infinite; }";
  html += ".status-card.relay-off { background: linear-gradient(135deg, #27ae60, #229954); }";
  html += ".status-card.mode-auto { background: linear-gradient(135deg, #9b59b6, #8e44ad); }";
  html += ".status-card.mode-manual { background: linear-gradient(135deg, #f39c12, #e67e22); }";
  html += ".status-card.timer-enabled { background: linear-gradient(135deg, #16a085, #1abc9c); }";
  html += ".status-card.timer-disabled { background: linear-gradient(135deg, #95a5a6, #7f8c8d); }";
  html += ".status-title { font-size: 0.9em; opacity: 0.9; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".status-value { font-size: 1.4em; font-weight: 700; }";
  html += ".status-sub { font-size: 0.8em; opacity: 0.8; margin-top: 5px; }";
  
  // Animation styles
  html += "@keyframes pulse-red { 0% { box-shadow: 0 0 0 0 rgba(231, 76, 60, 0.7); } 70% { box-shadow: 0 0 0 10px rgba(231, 76, 60, 0); } 100% { box-shadow: 0 0 0 0 rgba(231, 76, 60, 0); } }";
  html += "@keyframes pulse-blue { 0% { box-shadow: 0 0 0 0 rgba(52, 152, 219, 0.7); } 70% { box-shadow: 0 0 0 10px rgba(52, 152, 219, 0); } 100% { box-shadow: 0 0 0 0 rgba(52, 152, 219, 0); } }";
  
  // Button styles
  html += ".btn-group { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 12px; margin: 18px 0; }";
  html += "button { padding: 14px 22px; font-size: 15px; font-weight: 600; border: none; border-radius: 8px; cursor: pointer; transition: all 0.3s ease; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".btn-success { background: linear-gradient(135deg, #27ae60, #229954); color: white; }";
  html += ".btn-success:hover:not(:disabled) { background: linear-gradient(135deg, #229954, #1e8449); transform: translateY(-2px); box-shadow: 0 5px 15px rgba(39,174,96,0.4); }";
  html += ".btn-danger { background: linear-gradient(135deg, #e74c3c, #c0392b); color: white; }";
  html += ".btn-danger:hover:not(:disabled) { background: linear-gradient(135deg, #c0392b, #a93226); transform: translateY(-2px); box-shadow: 0 5px 15px rgba(231,76,60,0.4); }";
  html += ".btn-primary { background: linear-gradient(135deg, #3498db, #2980b9); color: white; }";
  html += ".btn-primary:hover:not(:disabled) { background: linear-gradient(135deg, #2980b9, #1f5f99); transform: translateY(-2px); box-shadow: 0 5px 15px rgba(52,152,219,0.4); }";
  html += ".btn-warning { background: linear-gradient(135deg, #f39c12, #e67e22); color: white; }";
  html += ".btn-warning:hover:not(:disabled) { background: linear-gradient(135deg, #e67e22, #d35400); transform: translateY(-2px); box-shadow: 0 5px 15px rgba(243,156,18,0.4); }";
  html += ".btn-purple { background: linear-gradient(135deg, #9b59b6, #8e44ad); color: white; }";
  html += ".btn-purple:hover:not(:disabled) { background: linear-gradient(135deg, #8e44ad, #7d3c98); transform: translateY(-2px); box-shadow: 0 5px 15px rgba(155,89,182,0.4); }";
  html += "button:disabled { opacity: 0.6; cursor: not-allowed; transform: none !important; box-shadow: none !important; }";
  
  // Form styles
  html += ".form-row { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 15px; margin: 15px 0; }";
  html += "label { display: block; margin-bottom: 6px; font-weight: 600; color: #2c3e50; font-size: 0.95em; }";
  html += "input[type='date'], input[type='time'] { width: 100%; padding: 12px; border: 2px solid #bdc3c7; border-radius: 6px; font-size: 15px; transition: all 0.3s ease; }";
  html += "input[type='date']:focus, input[type='time']:focus { border-color: #3498db; outline: none; box-shadow: 0 0 8px rgba(52,152,219,0.3); background: #f8f9fa; }";
  html += ".checkbox-wrapper { display: flex; align-items: center; margin: 12px 0; padding: 8px; background: #ecf0f1; border-radius: 6px; }";
  html += "input[type='checkbox'] { width: 20px; height: 20px; margin-right: 10px; accent-color: #3498db; }";
  html += ".checkbox-wrapper label { margin-bottom: 0; font-size: 0.95em; }";
  
  // Timer configuration styles
  html += ".timer-config { background: #ffffff; padding: 18px; border-radius: 10px; margin: 12px 0; border: 2px solid #ecf0f1; transition: border-color 0.3s ease; }";
  html += ".timer-config:hover { border-color: #3498db; }";
  html += ".timer-title { font-weight: 700; color: #2c3e50; margin-bottom: 12px; font-size: 1.1em; display: flex; align-items: center; }";
  html += ".timer-icon { margin-right: 8px; }";
  
  // Alert styles
  html += ".alert { padding: 12px; border-radius: 6px; margin: 10px 0; font-weight: 500; display: none; }";
  html += ".alert-success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".alert-error { background: #f8d7da; color: #721c24; border: 1px solid #f1aeb5; }";
  
  // Responsive design
  html += "@media (max-width: 768px) {";
  html += "  .container { padding: 15px; margin: 5px; }";
  html += "  h1 { font-size: 2em; }";
  html += "  .status-grid, .btn-group, .form-row { grid-template-columns: 1fr; }";
  html += "  .status-card { padding: 15px; }";
  html += "}";
  html += "</style>";
  
  // JavaScript for functionality
  html += "<script>";
  html += "let updateInterval;";
  html += "let isUpdating = false;";
  
  // Status update function
  html += "function updateStatus() {";
  html += "  if (isUpdating) return;";
  html += "  isUpdating = true;";
  html += "  fetch('/status', { method: 'GET', cache: 'no-cache', headers: { 'Cache-Control': 'no-cache' } })";
  html += "  .then(response => {";
  html += "    if (!response.ok) throw new Error('Network response was not ok');";
  html += "    return response.json();";
  html += "  })";
  html += "  .then(data => {";
  html += "    document.getElementById('currentTime').textContent = data.time;";
  html += "    document.getElementById('relayStatus').textContent = data.relay ? 'ON' : 'OFF';";
  html += "    document.getElementById('relayStatus').className = 'status-card ' + (data.relay ? 'relay-on' : 'relay-off');";
  html += "    document.getElementById('modeStatus').textContent = data.autoMode ? 'AUTO' : 'MANUAL';";
  html += "    document.getElementById('modeStatus').className = 'status-card ' + (data.autoMode ? 'mode-auto' : 'mode-manual');";
  html += "    const toggleBtn = document.getElementById('toggleBtn');";
  html += "    const modeBtn = document.getElementById('modeBtn');";
  html += "    toggleBtn.textContent = data.relay ? 'TURN OFF' : 'TURN ON';";
  html += "    toggleBtn.className = 'btn-' + (data.relay ? 'danger' : 'success');";
  html += "    toggleBtn.disabled = data.autoMode;";
  html += "    modeBtn.textContent = data.autoMode ? 'SWITCH TO MANUAL' : 'SWITCH TO AUTO';";
  html += "    modeBtn.className = 'btn-' + (data.autoMode ? 'warning' : 'purple');";
  html += "    if (data.onTimer) {";
  html += "      document.getElementById('onTimerStatus').textContent = String(data.onTimer.hour).padStart(2,'0') + ':' + String(data.onTimer.minute).padStart(2,'0');";
  html += "      document.getElementById('onTimerStatus').className = 'status-card ' + (data.onTimer.enabled ? 'timer-enabled' : 'timer-disabled');";
  html += "      document.getElementById('onTimerEnabled').textContent = data.onTimer.enabled ? 'ENABLED' : 'DISABLED';";
  html += "    }";
  html += "    if (data.offTimer) {";
  html += "      document.getElementById('offTimerStatus').textContent = String(data.offTimer.hour).padStart(2,'0') + ':' + String(data.offTimer.minute).padStart(2,'0');";
  html += "      document.getElementById('offTimerStatus').className = 'status-card ' + (data.offTimer.enabled ? 'timer-enabled' : 'timer-disabled');";
  html += "      document.getElementById('offTimerEnabled').textContent = data.offTimer.enabled ? 'ENABLED' : 'DISABLED';";
  html += "    }";
  html += "    isUpdating = false;";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Status update failed:', error);";
  html += "    isUpdating = false;";
  html += "  });";
  html += "}";
  
  // Utility functions
  html += "function showAlert(message, type = 'success') {";
  html += "  const alert = document.getElementById('alertBox');";
  html += "  alert.textContent = message;";
  html += "  alert.className = 'alert alert-' + type;";
  html += "  alert.style.display = 'block';";
  html += "  setTimeout(() => { alert.style.display = 'none'; }, 3000);";
  html += "}";
  
  // Control functions
  html += "function toggleRelay() {";
  html += "  fetch('/toggle', { method: 'GET', cache: 'no-cache' })";
  html += "  .then(response => response.text())";
  html += "  .then(data => {";
  html += "    showAlert('Relay switched ' + data);";
  html += "    setTimeout(updateStatus, 300);";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Toggle failed:', error);";
  html += "    showAlert('Toggle failed', 'error');";
  html += "  });";
  html += "}";
  
  html += "function toggleMode() {";
  html += "  fetch('/mode', { method: 'GET', cache: 'no-cache' })";
  html += "  .then(response => response.text())";
  html += "  .then(data => {";
  html += "    showAlert('Mode changed to ' + data);";
  html += "    setTimeout(updateStatus, 300);";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Mode change failed:', error);";
  html += "    showAlert('Mode change failed', 'error');";
  html += "  });";
  html += "}";
  
  html += "function submitTimeForm() {";
  html += "  const form = document.getElementById('timeForm');";
  html += "  const formData = new FormData(form);";
  html += "  fetch('/settime', { method: 'POST', body: formData })";
  html += "  .then(response => {";
  html += "    if (response.ok) {";
  html += "      showAlert('System time updated successfully!');";
  html += "      setTimeout(updateStatus, 500);";
  html += "    } else {";
  html += "      throw new Error('Server responded with error');";
  html += "    }";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Time update failed:', error);";
  html += "    showAlert('Time update failed', 'error');";
  html += "  });";
  html += "  return false;";
  html += "}";
  
  html += "function submitTimerForm() {";
  html += "  const form = document.getElementById('timerForm');";
  html += "  const formData = new FormData(form);";
  html += "  fetch('/settimer', { method: 'POST', body: formData })";
  html += "  .then(response => {";
  html += "    if (response.ok) {";
  html += "      showAlert('Timer settings saved successfully!');";
  html += "      setTimeout(updateStatus, 500);";
  html += "    } else {";
  html += "      throw new Error('Server responded with error');";
  html += "    }";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Timer update failed:', error);";
  html += "    showAlert('Timer update failed', 'error');";
  html += "  });";
  html += "  return false;";
  html += "}";
  
  // Initialize page
  html += "window.addEventListener('load', function() {";
  html += "  updateStatus();";
  html += "  updateInterval = setInterval(updateStatus, 2000);";
  html += "});";
  
  html += "window.addEventListener('beforeunload', function() {";
  html += "  if (updateInterval) clearInterval(updateInterval);";
  html += "});";
  html += "</script>";
  html += "</head><body>";
  
  html += "<div class='container'>";
  
  // Header
  html += "<div class='header'>";
  html += "<h1>🏠 Smart Timer Control</h1>";
  html += "<div class='subtitle'>NodeMCU ESP8266 Automation System</div>";
  html += "<div class='connection-info'>";
  html += "Connected to: <strong>" + String(ssid) + "</strong> | ";
  html += "Current Status: <span style='color: #27ae60; font-weight: bold;'>ONLINE</span>";
  html += "</div>";
  html += "</div>";
  
  // Alert box
  html += "<div id='alertBox' class='alert'></div>";
  
  // System Status Section
  html += "<div class='section'>";
  html += "<h2><span class='section-icon'>📊</span>System Status</h2>";
  html += "<div class='status-grid'>";
  html += "<div class='status-card time'>";
  html += "<div class='status-title'>Current Time</div>";
  html += "<div class='status-value' id='currentTime'>" + formatDateTime(now) + "</div>";
  html += "<div class='status-sub'>Real-time Clock</div>";
  html += "</div>";
  html += "<div class='status-card " + String(relayState ? "relay-on" : "relay-off") + "' id='relayStatus'>";
  html += "<div class='status-title'>Relay Status</div>";
  html += "<div class='status-value'>" + String(relayState ? "ON" : "OFF") + "</div>";
  html += "<div class='status-sub'>Hardware State</div>";
  html += "</div>";
  html += "<div class='status-card " + String(autoMode ? "mode-auto" : "mode-manual") + "' id='modeStatus'>";
  html += "<div class='status-title'>Control Mode</div>";
  html += "<div class='status-value'>" + String(autoMode ? "AUTO" : "MANUAL") + "</div>";
  html += "<div class='status-sub'>Operation Mode</div>";
  html += "</div>";
  html += "</div>";
  
  // Control Buttons
  html += "<div class='btn-group'>";
  html += "<button id='toggleBtn' class='btn-" + String(relayState ? "danger" : "success") + "' ";
  html += "onclick='toggleRelay()' " + String(autoMode ? "disabled" : "") + ">";
  html += String(relayState ? "⚡ TURN OFF" : "🔌 TURN ON");
  html += "</button>";
  html += "<button id='modeBtn' class='btn-" + String(autoMode ? "warning" : "purple") + "' onclick='toggleMode()'>";
  html += String(autoMode ? "🔧 SWITCH TO MANUAL" : "⚙️ SWITCH TO AUTO");
  html += "</button>";
  html += "</div>";
  html += "</div>";
  
  // Time Update Section
  html += "<div class='section'>";
  html += "<h2><span class='section-icon'>🕐</span>Update System Time</h2>";
  html += "<form id='timeForm' onsubmit='return submitTimeForm()'>";
  html += "<div class='form-row'>";
  html += "<div>";
  html += "<label for='date'>📅 Date:</label>";
  html += "<input type='date' id='date' name='date' value='" + formatDate(now) + "' required>";
  html += "</div>";
  html += "<div>";
  html += "<label for='time'>⏰ Time:</label>";
  html += "<input type='time' id='time' name='time' value='" + formatTime(now) + "' required>";
  html += "</div>";
  html += "</div>";
  html += "<button type='submit' class='btn-primary'>🔄 UPDATE SYSTEM TIME</button>";
  html += "</form>";
  html += "</div>";
  
  // Timer Configuration Section
  html += "<div class='section'>";
  html += "<h2><span class='section-icon'>⏲️</span>Timer Configuration</h2>";
  html += "<form id='timerForm' onsubmit='return submitTimerForm()'>";
  
  html += "<div class='timer-config'>";
  html += "<div class='timer-title'><span class='timer-icon'>🟢</span>Turn ON Timer</div>";
  html += "<div class='form-row'>";
  html += "<div>";
  html += "<label for='on_time'>Activation Time:</label>";
  html += "<input type='time' id='on_time' name='on_time' value='" + formatTimerTime(onTimer) + "'>";
  html += "</div>";
  html += "<div class='checkbox-wrapper'>";
  html += "<input type='checkbox' id='on_enabled' name='on_enabled' " + String(onTimer.enabled ? "checked" : "") + ">";
  html += "<label for='on_enabled'>Enable ON Timer</label>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='timer-config'>";
  html += "<div class='timer-title'><span class='timer-icon'>🔴</span>Turn OFF Timer</div>";
  html += "<div class='form-row'>";
  html += "<div>";
  html += "<label for='off_time'>Deactivation Time:</label>";
  html += "<input type='time' id='off_time' name='off_time' value='" + formatTimerTime(offTimer) + "'>";
  html += "</div>";
  html += "<div class='checkbox-wrapper'>";
  html += "<input type='checkbox' id='off_enabled' name='off_enabled' " + String(offTimer.enabled ? "checked" : "") + ">";
  html += "<label for='off_enabled'>Enable OFF Timer</label>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<button type='submit' class='btn-primary'>💾 SAVE TIMER SETTINGS</button>";
  html += "</form>";
  html += "</div>";
  
  // Timer Status Overview
  html += "<div class='section'>";
  html += "<h2><span class='section-icon'>📋</span>Active Timer Settings</h2>";
  html += "<div class='status-grid'>";
  html += "<div class='status-card " + String(onTimer.enabled ? "timer-enabled" : "timer-disabled") + "' id='onTimerStatus'>";
  html += "<div class='status-title'>🟢 ON Timer</div>";
  html += "<div class='status-value'>" + formatTimerTime(onTimer) + "</div>";
  html += "<div class='status-sub' id='onTimerEnabled'>" + String(onTimer.enabled ? "ENABLED" : "DISABLED") + "</div>";
  html += "</div>";
  html += "<div class='status-card " + String(offTimer.enabled ? "timer-enabled" : "timer-disabled") + "' id='offTimerStatus'>";
  html += "<div class='status-title'>🔴 OFF Timer</div>";
  html += "<div class='status-value'>" + formatTimerTime(offTimer) + "</div>";
  html += "<div class='status-sub' id='offTimerEnabled'>" + String(offTimer.enabled ? "ENABLED" : "DISABLED") + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  Serial.println("Main page served to client");
}

void handleSetTime() {
  Serial.println("=== TIME UPDATE REQUEST ===");
  
  if (!server.hasArg("date") || !server.hasArg("time")) {
    Serial.println("ERROR: Missing date or time parameters");
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  String dateStr = server.arg("date");
  String timeStr = server.arg("time");
  
  Serial.println("Received - Date: " + dateStr + ", Time: " + timeStr);
  
  // Parse date (YYYY-MM-DD)
  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();
  
  // Parse time (HH:MM)
  int hour = timeStr.substring(0, 2).toInt();
  int minute = timeStr.substring(3, 5).toInt();
  
  // Validate input ranges
  if (year < 2020 || year > 2050 || month < 1 || month > 12 || 
      day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    Serial.println("ERROR: Invalid date/time parameters");
    Serial.println("Year: " + String(year) + ", Month: " + String(month) + ", Day: " + String(day));
    Serial.println("Hour: " + String(hour) + ", Minute: " + String(minute));
    server.send(400, "text/plain", "Invalid parameters");
    return;
  }
  
  // Store old time for comparison
  DateTime oldTime = rtc.now();
  Serial.println("Old RTC time: " + formatDateTime(oldTime));
  
  // Create and set new DateTime
  DateTime newTime(year, month, day, hour, minute, 0);
  rtc.adjust(newTime);
  
  // Wait a moment and verify
  delay(100);
  DateTime verifyTime = rtc.now();
  
  // Clear minute processing array to allow immediate timer execution
  memset(lastMinuteProcessed, false, sizeof(lastMinuteProcessed));
  
  Serial.println("NEW RTC time set to: " + formatDateTime(verifyTime));
  Serial.println("Time update completed successfully");
  
  lastDebugMessage = "Time updated: " + formatDateTime(verifyTime);
  server.send(200, "text/plain", "Time updated successfully");
}

void handleSetTimer() {
  Serial.println("=== TIMER SETTINGS UPDATE ===");
  
  bool updated = false;
  
  // Handle ON timer
  if (server.hasArg("on_time")) {
    String timeStr = server.arg("on_time");
    Serial.println("ON Timer time received: " + timeStr);
    
    if (timeStr.length() >= 5) {
      int hour = timeStr.substring(0, 2).toInt();
      int minute = timeStr.substring(3, 5).toInt();
      
      if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        onTimer.hour = hour;
        onTimer.minute = minute;
        onTimer.enabled = server.hasArg("on_enabled");
        updated = true;
        Serial.println("ON Timer updated: " + String(hour) + ":" + String(minute) + " (Enabled: " + String(onTimer.enabled) + ")");
      } else {
        Serial.println("ERROR: Invalid ON timer time values");
      }
    }
  }
  
  // Handle OFF timer
  if (server.hasArg("off_time")) {
    String timeStr = server.arg("off_time");
    Serial.println("OFF Timer time received: " + timeStr);
    
    if (timeStr.length() >= 5) {
      int hour = timeStr.substring(0, 2).toInt();
      int minute = timeStr.substring(3, 5).toInt();
      
      if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
        offTimer.hour = hour;
        offTimer.minute = minute;
        offTimer.enabled = server.hasArg("off_enabled");
        updated = true;
        Serial.println("OFF Timer updated: " + String(hour) + ":" + String(minute) + " (Enabled: " + String(offTimer.enabled) + ")");
      } else {
        Serial.println("ERROR: Invalid OFF timer time values");
      }
    }
  }
  
  if (updated) {
    // Clear minute processing to allow immediate execution if current time matches
    memset(lastMinuteProcessed, false, sizeof(lastMinuteProcessed));
    
    Serial.println("=== FINAL TIMER CONFIGURATION ===");
    Serial.println("ON Timer:  " + formatTimerTime(onTimer) + " - " + (onTimer.enabled ? "ENABLED" : "DISABLED"));
    Serial.println("OFF Timer: " + formatTimerTime(offTimer) + " - " + (offTimer.enabled ? "ENABLED" : "DISABLED"));
    Serial.println("Auto Mode: " + String(autoMode ? "ON" : "OFF"));
    
    lastDebugMessage = "Timers updated - ON: " + formatTimerTime(onTimer) + "(" + String(onTimer.enabled ? "EN" : "DIS") + ") OFF: " + formatTimerTime(offTimer) + "(" + String(offTimer.enabled ? "EN" : "DIS") + ")";
    server.send(200, "text/plain", "Timer settings updated");
  } else {
    Serial.println("ERROR: No valid timer data received");
    server.send(400, "text/plain", "No valid timer data");
  }
}

void handleToggle() {
  Serial.println("=== MANUAL RELAY TOGGLE REQUEST ===");
  
  if (autoMode) {
    Serial.println("Toggle DENIED - System in AUTO mode");
    server.send(400, "text/plain", "Cannot toggle in AUTO mode");
    return;
  }
  
  // Toggle relay state
  relayState = !relayState;
  updateRelay();
  
  String status = relayState ? "ON" : "OFF";
  Serial.println("Manual toggle executed: Relay " + status);
  
  lastDebugMessage = "Manual toggle: " + status;
  server.send(200, "text/plain", status);
}

void handleMode() {
  Serial.println("=== MODE CHANGE REQUEST ===");
  
  String oldMode = autoMode ? "AUTO" : "MANUAL";
  autoMode = !autoMode;
  String newMode = autoMode ? "AUTO" : "MANUAL";
  
  // Clear minute processing when switching modes
  memset(lastMinuteProcessed, false, sizeof(lastMinuteProcessed));
  
  Serial.println("Mode changed: " + oldMode + " → " + newMode);
  
  if (autoMode) {
    Serial.println("AUTO mode activated - Timers will now control relay");
    Serial.println("Current timer settings:");
    Serial.println("  ON Timer:  " + formatTimerTime(onTimer) + " - " + (onTimer.enabled ? "ENABLED" : "DISABLED"));
    Serial.println("  OFF Timer: " + formatTimerTime(offTimer) + " - " + (offTimer.enabled ? "ENABLED" : "DISABLED"));
  } else {
    Serial.println("MANUAL mode activated - Use toggle button to control relay");
  }
  
  lastDebugMessage = "Mode: " + oldMode + " → " + newMode;
  server.send(200, "text/plain", newMode);
}

void handleStatus() {
  DateTime now = rtc.now();
  
  String json = "{";
  json += "\"time\":\"" + formatDateTime(now) + "\",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"autoMode\":" + String(autoMode ? "true" : "false") + ",";
  json += "\"onTimer\":{";
  json += "\"hour\":" + String(onTimer.hour) + ",";
  json += "\"minute\":" + String(onTimer.minute) + ",";
  json += "\"enabled\":" + String(onTimer.enabled ? "true" : "false");
  json += "},";
  json += "\"offTimer\":{";
  json += "\"hour\":" + String(offTimer.hour) + ",";
  json += "\"minute\":" + String(offTimer.minute) + ",";
  json += "\"enabled\":" + String(offTimer.enabled ? "true" : "false");
  json += "}";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleDebug() {
  DateTime now = rtc.now();
  String debug = "Time: " + formatDateTime(now) + " | ";
  debug += "Relay: " + String(relayState ? "ON" : "OFF") + " | ";
  debug += "Mode: " + String(autoMode ? "AUTO" : "MANUAL") + " | ";
  debug += "Last: " + lastDebugMessage;
  
  server.send(200, "text/plain", debug);
}

void checkAndExecuteTimers() {
  DateTime now = rtc.now();
  int currentHour = now.hour();
  int currentMinute = now.minute();
  
  // Prevent multiple executions within the same minute
  if (lastMinuteProcessed[currentHour][currentMinute]) {
    return;
  }
  
  bool timerExecuted = false;
  String executionLog = "";
  
  // Check ON timer
  if (onTimer.enabled && currentHour == onTimer.hour && currentMinute == onTimer.minute) {
    if (!relayState) {
      relayState = true;
      updateRelay();
      executionLog = "AUTO ON Timer executed at " + String(currentHour) + ":" + String(currentMinute);
      Serial.println("🟢 " + executionLog);
      lastDebugMessage = "Auto ON: " + formatTime(now);
      timerExecuted = true;
    } else {
      Serial.println("ON Timer triggered but relay already ON");
    }
  }
  
  // Check OFF timer
  if (offTimer.enabled && currentHour == offTimer.hour && currentMinute == offTimer.minute) {
    if (relayState) {
      relayState = false;
      updateRelay();
      executionLog = "AUTO OFF Timer executed at " + String(currentHour) + ":" + String(currentMinute);
      Serial.println("🔴 " + executionLog);
      lastDebugMessage = "Auto OFF: " + formatTime(now);
      timerExecuted = true;
    } else {
      Serial.println("OFF Timer triggered but relay already OFF");
    }
  }
  
  // Mark this minute as processed
  if (timerExecuted || (onTimer.enabled && currentHour == onTimer.hour && currentMinute == onTimer.minute) || 
      (offTimer.enabled && currentHour == offTimer.hour && currentMinute == offTimer.minute)) {
    lastMinuteProcessed[currentHour][currentMinute] = true;
    
    // Auto-clear processed flags after 90 seconds to handle day rollover
    // This happens automatically when time is set or mode is changed
  }
}

void updateRelay() {
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  Serial.println("🔌 Relay hardware updated: Pin " + String(RELAY_PIN) + " = " + String(relayState ? "HIGH (ON)" : "LOW (OFF)"));
}

void printSystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.println("Current Time: " + formatDateTime(rtc.now()));
  Serial.println("Relay State: " + String(relayState ? "ON" : "OFF"));
  Serial.println("Control Mode: " + String(autoMode ? "AUTO" : "MANUAL"));
  Serial.println("ON Timer: " + formatTimerTime(onTimer) + " - " + String(onTimer.enabled ? "ENABLED" : "DISABLED"));
  Serial.println("OFF Timer: " + formatTimerTime(offTimer) + " - " + String(offTimer.enabled ? "ENABLED" : "DISABLED"));
  Serial.println("WiFi Status: AP Mode - " + String(ssid));
  Serial.println("IP Address: " + WiFi.softAPIP().toString());
  Serial.println("==================");
}

String formatDateTime(DateTime dt) {
  char buffer[20];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", 
          dt.day(), dt.month(), dt.year(), 
          dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

String formatDate(DateTime dt) {
  char buffer[11];
  sprintf(buffer, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
  return String(buffer);
}

String formatTime(DateTime dt) {
  char buffer[6];
  sprintf(buffer, "%02d:%02d", dt.hour(), dt.minute());
  return String(buffer);
}

String formatTimerTime(Timer t) {
  char buffer[6];
  sprintf(buffer, "%02d:%02d", t.hour, t.minute);
  return String(buffer);
}