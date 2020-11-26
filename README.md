# AS3935_I2C
CJMCU AS3935 Lightning Detector on I2C with ESP8266 NodeMCU ESP-12E
This code is for ESP8266 and uses the Arduino IDE
It also uses stevemarple's AS3935.h library at
https://github.com/stevemarple/AS3935

The wiring diagram is at:
https://cmheong.blogspot.com/2020/11/as3935-lightning-detector-with-i2c-and.html

SCL to D1/GPIO5, MISO to D2/GPIO4, and VCC is 3V3.  
SI, A0 and A1 on the AS3935 are tied to 3.3V
    
Slight changes has been made to stevemarple's library to add a 'calibrate' function   
