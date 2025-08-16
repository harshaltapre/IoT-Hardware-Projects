from machine import Pin, I2C
import time
from pico_i2c_lcd import I2cLcd  


i2c = I2C(0, scl=Pin(0), sda=Pin(1), freq=400000)
lcd = I2cLcd(i2c, 0x27, 2, 16)  


trig = Pin(2, Pin.OUT)
echo = Pin(3, Pin.IN)

def get_distance():
    trig.low()
        time.sleep_us(2)
            trig.high()
                time.sleep_us(10)
                    trig.low()
                        
                            while echo.value() == 0:
                                    pulse_start = time.ticks_us()
                                        
                                            while echo.value() == 1:
                                                    pulse_end = time.ticks_us()

                                                        pulse_duration = time.ticks_diff(pulse_end, pulse_start)
                                                            distance = (pulse_duration * 0.0343) / 2 
                                                                return round(distance, 2)

                                                                # Main loop
                                                                while True:
                                                                    distance = get_distance()
                                                                        lcd.clear()
                                                                            lcd.putstr("Distance: {:.2f}cm".format(distance))
                                                                                time.sleep(1)
                                                                    
