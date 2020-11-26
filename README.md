# AS3935_I2C
CJMCU AS3935 Lightning Detector on I2C with ESP8266 NodeMCU ESP-12E
This code is for ESP8266 and uses the Arduino IDE
It also uses stevemarple's AS3935.h library
The wiring diagram is:
The wiring is:

AS3935                                  ESP-12E
    SCL                                   D1/GPIO5
    MISO                                  D2/GPIO4
    VCC                                    3.3V
    GND                                    GND
    
Slight changes gas been made to stevemarple's library to add a 'calibrate' function   
