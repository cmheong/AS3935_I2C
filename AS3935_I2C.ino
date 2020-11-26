#include <AsyncDelay.h>
#include <SoftWire.h>
#include <AS3935.h>

#ifdef JTD
#include <DisableJTAG.h>
#endif

AS3935 as3935;
bool ledState = true;
AsyncDelay d;

ICACHE_RAM_ATTR void int2Handler(void) // 2020-11-22
{
  as3935.interruptHandler();
}


void readRegs(uint8_t start, uint8_t end)
{
  for (uint8_t reg = start; reg < end; ++reg) {
    delay(50);
    uint8_t val;
    as3935.readRegister(reg, val);

    Serial.print("Reg: 0x");
    Serial.print(reg, HEX);
    Serial.print(": 0x");
    Serial.println(val, HEX);
    Serial.flush();
  }
  Serial.print("State: ");
  Serial.println(as3935.getState(), DEC);

  Serial.println("-------------");
}

bool NoiseHigh = false;
bool Disturbed = false;

void printInterruptReason(Stream &s, uint8_t value, const char *prefix = nullptr)
{
  if (value & AS3935::intNoiseLevelTooHigh) {
    if (NoiseHigh == false) {
      if (prefix)
        s.print(prefix);
      s.println(F("Noise level too high"));
      // NoiseHigh = true;
    }  
  }
  if (value & AS3935::intDisturberDetected) {
    if (Disturbed == false) {
      if (prefix)
        s.print(prefix);
      s.println(F("Disturber detected"));
      // Disturbed = true;
    }
  }  
  if (value & AS3935::intLightningDetected) {
    if (prefix)
      s.print(prefix);
    s.println(F("Lightning detected"));
  }
}


void setup(void)
{
#ifdef JTD
  disableJTAG();
#endif

  Serial.begin(115200);
  // as3935.initialise(14, 17, 0x03, 3, true, NULL);
  as3935.initialise(4, 5, 0x03, 3, true, NULL); // 2020-11-22

  // as3935.calibrate();
  as3935.start();
  d.start(1000, AsyncDelay::MILLIS);

  while (!d.isExpired())
    as3935.process();

  Serial.println("Before setup:");
  readRegs(0, 0x09);

  // attachInterrupt(2, int2Handler, RISING); // 2 is believed to be D4, GPIO2
  attachInterrupt(14, int2Handler, RISING); // 2020-11-22 Chose D5 GPI14
  // attachInterrupt(digitalPinToInterrupt(5), int2Handler, RISING); // 2020-11-22 Chose D3 GPIO0
  d.start(1000, AsyncDelay::MILLIS);
  Serial.println("setup() done");
  readRegs(0, 0x09);

  as3935.setIndoor(true);
  as3935.setNoiseFloor(4);
  as3935.setThreshold(4);
  as3935.setSpikeRejection(0); // From AS3935_timestamp_demo.ino
  
  //as3935.calibrate();
  //Serial.println("Calibration done");
  //readRegs(0, 0x09);

  Serial.println("Masking Disturber interrupts ...");
  as3935.setRegisterBit(as3935.regInt, 5, true);
  readRegs(0, 0x09);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledState);

}

uint8_t count = 0;
void loop(void)
{
  if (as3935.process()) {
    uint8_t flags = as3935.getInterruptFlags();
    uint8_t dist = as3935.getDistance();

    /*
    Serial.println("-------------------");
    Serial.println("Interrupt!");
    Serial.println("Reason(s):");
    */

    printInterruptReason(Serial, flags, "    ");
    
    if (AS3935::intLightningDetected & flags) {
      Serial.print("Distance: ");
      Serial.println(dist, DEC);
    }
  }

  if (as3935.getBusError()) {
    Serial.println("Bus error!");
    as3935.clearBusError();
  }

  // Flash the LED to show activity
  if (d.isExpired()) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);

    // // Periodically output the AS3935 registers
    // if (++count > 5) {
    //  count = 0;
    //  readRegs(0, 0x09);
    // }
    d.start(1000, AsyncDelay::MILLIS);
  }

}
