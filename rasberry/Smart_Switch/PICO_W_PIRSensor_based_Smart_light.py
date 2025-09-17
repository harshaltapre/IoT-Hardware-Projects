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

# Initialize Hardware
pir_sensor = machine.Pin(PIR_PIN, machine.Pin.IN, machine.Pin.PULL_DOWN)
relay = machine.Pin(RELAY_PIN, machine.Pin.OUT)
ldr = machine.ADC(LDR_PIN)

# System Configuration
LDR_THRESHOLD = 30000   # Adjust this based on your room lighting
RELAY_ACTIVE_HIGH = True  # Set to True if relay turns ON with HIGH signal

# === Controller Class ===
class LightController:
    def __init__(self):
        self.auto_mode = True
        self.motion_detected = False
        self.is_dark = False
        self.relay_on = False
        self.last_motion_check = 0
        self.motion_timeout = 30000  # Motion timeout in ms (30 seconds)

    def read_sensors(self):
        """Read PIR and LDR"""
        self.motion_detected = pir_sensor.value() == 1
        self.is_dark = ldr.read_u16() < LDR_THRESHOLD

    def set_relay(self, state):
        """Turn relay ON/OFF"""
        relay.value(state if RELAY_ACTIVE_HIGH else not state)
        self.relay_on = state

    def handle_auto_mode(self):
        """Automatic mode logic"""
        current_time = time.ticks_ms()
        self.read_sensors()

        if self.auto_mode:
            if self.motion_detected:
                self.last_motion_check = current_time
                if self.is_dark:
                    self.set_relay(True)
            elif time.ticks_diff(current_time, self.last_motion_check) > self.motion_timeout:
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
            .controls { display: flex; justify-content: space-between; gap: 10px; }
            button { flex: 1; padding: 12px; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: 0.2s; }
            .btn-light { background: #007bff; color: #fff; }
            .btn-light:hover { background: #0056b3; }
            .btn-mode { background: #28a745; color: #fff; }
            .btn-mode:hover { background: #1e7e34; }
            button:disabled { opacity: 0.6; cursor: not-allowed; }
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

    print("Network Ready")
    print("Name: SmartLight")
    print("Password: 12345678")
    print("IP: 192.168.4.1")
    return "192.168.4.1"

# === Web Server ===
def start_server(ip):
    addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(5)
    print(f"Server running at http://{ip}")

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
