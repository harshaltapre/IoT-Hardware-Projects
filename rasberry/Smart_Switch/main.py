import machine
import network
import socket
import time
import gc
import ujson

# === Pin Configuration ===
PIR_PIN = 2      # PIR sensor on GP2
RELAY_PIN = 3    # Relay on GP3
LDR_PIN = 26     # LDR on GP26
MOTION_LED_PIN = 8   # Motion detection LED on GP8
LDR_LED_PIN = 9      # LDR detection LED on GP9

# Initialize Hardware
pir_sensor = machine.Pin(PIR_PIN, machine.Pin.IN, machine.Pin.PULL_DOWN)
relay = machine.Pin(RELAY_PIN, machine.Pin.OUT)
ldr = machine.ADC(LDR_PIN)
motion_led = machine.Pin(MOTION_LED_PIN, machine.Pin.OUT)
ldr_led = machine.Pin(LDR_LED_PIN, machine.Pin.OUT)

# System Configuration
LDR_THRESHOLD = 40000   # Adjust this based on your room lighting (higher = needs darker to trigger)
RELAY_ACTIVE_HIGH = True  # Set to True if relay turns ON with HIGH signal

# Timeout Configuration
# PIR Motion: 30 seconds after no motion detected
# LDR Darkness: 30 seconds after brightness detected
# Note: LDR readings - Higher values = Darker environment
# Typical values: Bright room ~5000-15000, Dark room ~45000-65000
# Adjust LDR_THRESHOLD based on your environment testing

# === Controller Class ===
class LightController:
    def __init__(self):
        self.auto_mode = True  # DEFAULT: Always start in automatic mode
        self.motion_detected = False
        self.is_dark = False
        self.relay_on = False
        self.last_motion_check = 0
        self.last_darkness_check = 0  # Separate timer for LDR
        self.motion_timeout = 30000  # 30 seconds timeout for motion
        self.darkness_timeout = 30000  # 30 seconds timeout for darkness
        self.last_light_state = False
        
        # Initialize LEDs to OFF
        motion_led.value(0)
        ldr_led.value(0)
        
        # Initialize relay to OFF
        self.set_relay(False)
        
        print("System initialized in AUTOMATIC mode")
        print("System will work without user interaction")
        print("Web dashboard available for manual control if needed")

    def read_sensors(self):
        """Read PIR and LDR"""
        # Read PIR sensor for motion detection
        self.motion_detected = pir_sensor.value() == 1
        
        # Read LDR value and determine if it's dark
        ldr_value = ldr.read_u16()
        # LDR gives higher values in darkness, lower values in bright light
        # Adjust threshold as needed: higher value = darker environment needed to trigger
        self.is_dark = ldr_value > LDR_THRESHOLD
        
        # Control LEDs based on sensor readings
        self.update_indicator_leds()

    def update_indicator_leds(self):
        """Update LED indicators based on sensor states"""
        # Motion LED (GP4) - ON when motion is detected (hardware trigger indication)
        if self.motion_detected:
            motion_led.value(1)  # LED ON to show motion detected
        else:
            motion_led.value(0)  # LED OFF when no motion
        
        # LDR LED (GP5) - ON when it's dark (LDR doesn't detect sufficient light)
        if self.is_dark:
            ldr_led.value(1)     # LED ON to show dark environment detected
        else:
            ldr_led.value(0)     # LED OFF when sufficient light is available

    def set_relay(self, state):
        """Turn relay ON/OFF"""
        relay.value(state if RELAY_ACTIVE_HIGH else not state)
        self.relay_on = state
        self.last_light_state = state  # Track light state for LDR display

    def handle_auto_mode(self):
        """Automatic mode logic - Both PIR and LDR with individual 30-second timeouts"""
        current_time = time.ticks_ms()
        self.read_sensors()  # Always read sensors to update LED indicators

        if self.auto_mode:
            # Handle PIR Motion Detection
            if self.motion_detected:
                # Motion detected - INSTANTLY turn ON relay and update timer
                self.last_motion_check = current_time
                self.set_relay(True)
            else:
                # No motion detected - RESET timer immediately for 30-second countdown
                if self.last_motion_check != 0:  # Only reset if it was previously active
                    self.last_motion_check = current_time
            
            # Handle LDR Darkness Detection  
            if self.is_dark:
                # Darkness detected - INSTANTLY turn ON relay and update timer
                self.last_darkness_check = current_time
                self.set_relay(True)
            else:
                # Bright light detected - RESET timer immediately for 30-second countdown
                if self.last_darkness_check != 0:  # Only reset if it was previously active
                    self.last_darkness_check = current_time
            
            # Check timeouts for turning OFF relay
            motion_timeout_reached = time.ticks_diff(current_time, self.last_motion_check) > self.motion_timeout
            darkness_timeout_reached = time.ticks_diff(current_time, self.last_darkness_check) > self.darkness_timeout
            
            # Turn OFF relay only if BOTH conditions are met:
            # 1. No current motion AND motion timeout reached (30 seconds)
            # 2. No current darkness AND darkness timeout reached (30 seconds)
            if (not self.motion_detected and motion_timeout_reached) and \
               (not self.is_dark and darkness_timeout_reached):
                self.set_relay(False)

    def toggle_relay(self):
        """Manual toggle"""
        if not self.auto_mode:
            self.set_relay(not self.relay_on)
            return True
        return False

    def toggle_auto_mode(self):
        """Switch Auto/Manual"""
        self.auto_mode = not self.auto_mode
        if self.auto_mode:
            self.handle_auto_mode()
        return self.auto_mode

    def get_status(self):
        """Return system status as dict"""
        return {
            "motion": self.motion_detected,
            "dark": self.is_dark,
            "relay": self.relay_on,
            "auto": self.auto_mode
        }

# === Web UI ===
def create_webpage():
    return """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Smart Light Control</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body { font-family: Arial, sans-serif; background: #eef2f7; margin: 0; padding: 0; }
            .container { max-width: 600px; margin: 40px auto; padding: 20px; }
            .card { background: #fff; border-radius: 10px; padding: 20px; box-shadow: 0 3px 6px rgba(0,0,0,0.15); }
            h1 { text-align: center; font-size: 22px; margin-bottom: 20px; }
            .status-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 20px; }
            .status-item { padding: 15px; background: #f8f9fa; border-radius: 8px; text-align: center; }
            .label { font-size: 13px; color: #6c757d; margin-bottom: 6px; }
            .value { font-size: 16px; font-weight: bold; }
            .active { color: #28a745; }
            .inactive { color: #dc3545; }
            .controls { display: flex; justify-content: space-between; gap: 10px; margin-bottom: 20px; }
            button { flex: 1; padding: 12px; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: 0.2s; }
            .btn-light { background: #007bff; color: #fff; }
            .btn-light:hover { background: #0056b3; }
            .btn-mode { background: #28a745; color: #fff; }
            .btn-mode:hover { background: #1e7e34; }
            button:disabled { opacity: 0.6; cursor: not-allowed; }
            .led-status { background: #fff3cd; border: 1px solid #ffeaa7; border-radius: 8px; padding: 15px; margin-top: 20px; }
            .led-status h3 { margin: 0 0 10px 0; font-size: 16px; color: #856404; }
            .led-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
            .led-item { display: flex; align-items: center; gap: 8px; }
            .led-indicator { width: 12px; height: 12px; border-radius: 50%; border: 2px solid #ddd; }
            .led-on { background: #28a745; box-shadow: 0 0 8px #28a745; }
            .led-off { background: #dc3545; }
        </style>
    </head>
    <body>
        <div class="container">
            <div class="card">
                <h1>Smart Light Control</h1>
                <div class="status-grid">
                    <div class="status-item">
                        <div class="label">Motion</div>
                        <div class="value" id="motion">-</div>
                    </div>
                    <div class="status-item">
                        <div class="label">Light</div>
                        <div class="value" id="light">-</div>
                    </div>
                    <div class="status-item">
                        <div class="label">Relay</div>
                        <div class="value" id="relay">-</div>
                    </div>
                    <div class="status-item">
                        <div class="label">Mode</div>
                        <div class="value" id="mode">-</div>
                    </div>
                </div>
                <div class="controls">
                    <button id="lightBtn" class="btn-light" onclick="toggleLight()">Toggle Light</button>
                    <button id="modeBtn" class="btn-mode" onclick="toggleMode()">Change Mode</button>
                </div>
                <div class="led-status">
                    <h3>LED Indicators</h3>
                    <div class="led-grid">
                        <div class="led-item">
                            <div class="led-indicator" id="motionLed"></div>
                            <span>Motion LED (GP8)</span>
                        </div>
                        <div class="led-item">
                            <div class="led-indicator" id="ldrLed"></div>
                            <span>Dark LED (GP9)</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <script>
            function updateStatus() {
                fetch('/status').then(r => r.json()).then(data => {
                    document.getElementById('motion').textContent = data.motion ? "Detected" : "None";
                    document.getElementById('motion').className = "value " + (data.motion ? "active" : "inactive");

                    document.getElementById('light').textContent = data.dark ? "Dark" : "Bright";
                    document.getElementById('light').className = "value " + (data.dark ? "inactive" : "active");

                    document.getElementById('relay').textContent = data.relay ? "ON" : "OFF";
                    document.getElementById('relay').className = "value " + (data.relay ? "active" : "inactive");

                    document.getElementById('mode').textContent = data.auto ? "Automatic" : "Manual";
                    document.getElementById('mode').className = "value " + (data.auto ? "active" : "inactive");

                    document.getElementById('lightBtn').disabled = data.auto;
                    
                    // Update LED indicators
                    document.getElementById('motionLed').className = "led-indicator " + (data.motion ? "led-on" : "led-off");
                    document.getElementById('ldrLed').className = "led-indicator " + (data.dark ? "led-on" : "led-off");
                });
            }

            function toggleLight() {
                fetch('/light').then(updateStatus);
            }

            function toggleMode() {
                fetch('/mode').then(updateStatus);
            }

            setInterval(updateStatus, 1000);
            updateStatus();
        </script>
    </body>
    </html>
    """

# === WiFi Setup ===
def start_access_point():
    ap = network.WLAN(network.AP_IF)
    ap.config(essid="SmartLight", password="12345678")
    ap.active(True)

    while not ap.active():
        time.sleep(0.1)

    print("=== SMART LIGHT SYSTEM READY ===")
    print("✅ System working AUTOMATICALLY")
    print("✅ No user interaction required")
    print("📱 Optional web control available:")
    print("   Network: SmartLight")
    print("   Password: 12345678") 
    print("   URL: http://192.168.4.1")
    print("=================================")
    return "192.168.4.1"

# === Web Server ===
def start_server(ip):
    addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(1)  # Reduced to 1 since automatic operation is priority
    s.settimeout(0.1)  # Non-blocking socket for continuous operation
    
    print(f"🚀 Automatic Smart Light System Running")
    print(f"📡 Web dashboard: http://{ip} (optional)")

    controller = LightController()
    last_check = time.ticks_ms()

    while True:
        try:
            # Auto mode check
            if time.ticks_diff(time.ticks_ms(), last_check) > 200:
                controller.handle_auto_mode()
                last_check = time.ticks_ms()

            cl, addr = s.accept()
            request = cl.recv(1024).decode()
            if not request:
                cl.close()
                continue

            if request.startswith("GET /status"):
                response = ujson.dumps(controller.get_status())
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n")
                cl.send(response)

            elif request.startswith("GET /light"):
                controller.toggle_relay()
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nOK")

            elif request.startswith("GET /mode"):
                controller.toggle_auto_mode()
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nOK")

            else:
                response = create_webpage()
                cl.send("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n")
                cl.send(response)

            cl.close()
            gc.collect()

        except Exception as e:
            print("Error:", e)
            try:
                cl.close()
            except:
                pass

# === Main Program ===
try:
    ip = start_access_point()
    start_server(ip)
except KeyboardInterrupt:
    print("Stopped by user")
except Exception as e:
    print("System Error:", e)