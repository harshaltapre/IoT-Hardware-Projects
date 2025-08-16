import RPi.GPIO as GPIO
import time
from picamera import PiCamera

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

#  Motor Pins (BTS7960) 
# Left BTS7960 (U1)
L_PWM = 12
L_EN = 16
R_PWM = 13
R_EN = 20

# Right BTS7960 (U2)
L2_PWM = 6
L2_EN = 5
R2_PWM = 26
R2_EN = 19

motor_pins = [L_PWM, L_EN, R_PWM, R_EN, L2_PWM, L2_EN, R2_PWM, R2_EN]
for pin in motor_pins:
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

# Servo Motor Pin 
SERVO_PIN = 21
GPIO.setup(SERVO_PIN, GPIO.OUT)
servo = GPIO.PWM(SERVO_PIN, 50)
servo.start(0)

#  Laser Module 
LASER_PIN = 17
GPIO.setup(LASER_PIN, GPIO.OUT)
GPIO.output(LASER_PIN, GPIO.LOW)

#  Camera 
camera = PiCamera()

# Functions 
def motor_forward():
    GPIO.output(L_EN, GPIO.HIGH)
    GPIO.output(R_EN, GPIO.HIGH)
    GPIO.output(L_PWM, GPIO.HIGH)
    GPIO.output(R_PWM, GPIO.LOW)

    GPIO.output(L2_EN, GPIO.HIGH)
    GPIO.output(R2_EN, GPIO.HIGH)
    GPIO.output(L2_PWM, GPIO.HIGH)
    GPIO.output(R2_PWM, GPIO.LOW)

def motor_stop():
    for pin in motor_pins:
        GPIO.output(pin, GPIO.LOW)

def servo_rotate(angle):
    duty = 2 + (angle / 18)
    GPIO.output(SERVO_PIN, True)
    servo.ChangeDutyCycle(duty)
    time.sleep(0.5)
    GPIO.output(SERVO_PIN, False)
    servo.ChangeDutyCycle(0)

def laser_on():
    GPIO.output(LASER_PIN, GPIO.HIGH)

def laser_off():
    GPIO.output(LASER_PIN, GPIO.LOW)

def take_picture():
    camera.start_preview()
    time.sleep(2)
    camera.capture('/home/pi/image.jpg')
    camera.stop_preview()

# Main Loop
try:
    while True:
        motor_forward()
        laser_on()
        servo_rotate(90)
        time.sleep(1)
        motor_stop()
        laser_off()
        take_picture()
        time.sleep(2)

except KeyboardInterrupt:
    print("Exiting...")
    motor_stop()
    laser_off()
    servo.stop()
    GPIO.cleanup()
