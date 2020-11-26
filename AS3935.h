#ifndef AS3935_H
#define AS3935_H

#define AS3935_VERSION "1.0.5"

#include <AsyncDelay.h>
#include <SoftWire.h>

class AS3935 {
public:
	enum state_t {
		stateOff = 0,
		statePoweringUp = 1, // first 2ms wait after power applied
		stateInitialising1 = 2,
		stateInitialising2 = 3,
		stateListening = 4,
		stateWaitingForResult = 5,
		stateCalibrate = 6,
	};

	static const uint8_t regAfeGain = 0x00;
	static const uint8_t regNoiseFloor = 0x01;
	static const uint8_t regSpikeRej = 0x02;
	static const uint8_t regInt = 0x03;
	static const uint8_t regDistance = 0x07;
	static const uint8_t regTunCap = 0x08;
	static const uint8_t regPresetDefault = 0x3c;
	static const uint8_t regCalibRCO = 0x3d;

	static const uint8_t intNoiseLevelTooHigh = 1;
	static const uint8_t intDisturberDetected = 4;
	static const uint8_t intLightningDetected = 8;

  
	// Various delays for switching between different states
	// etc. Because timer accuracy is limited to nearest ms add 1ms to
	// datasheet values.
	static const uint8_t powerUpDelay_ms = 5;
	static const uint8_t initialisation1Delay_ms = 3;
	static const uint8_t initialisation2Delay_ms = 3;
	static const uint8_t interruptDelay_ms = 3;
  
	AS3935(void);
  
	bool initialise(uint8_t sda, uint8_t scl, uint8_t address, uint8_t tunCap,
					bool indoor, void (*timestampCB)(void) = NULL);
  
	void start(void);

	// Call often to process state machine. If returns true then an
	// interrupt was generated and a result is available.
	bool process(void);

	void finish(void); // Force completion and power-down

	// Set up the interrupt handler to call this routine
	inline void interruptHandler(void);

	bool readRegister(uint8_t reg, uint8_t &val) const;
	bool writeRegister(uint8_t reg, uint8_t val) const;
	bool setRegisterBit(uint8_t reg, uint8_t bit, bool bitValue = true) const;

	inline bool presetDefault(void) const;
	inline bool calibrateRCO(void) const;
	bool setIndoor(bool indoor) const;
	bool setNoiseFloor(uint8_t level) const;
	bool setThreshold(uint8_t level) const;
	bool setSpikeRejection(uint8_t spikes) const;
	bool setLCOFreqDiv16(void) const;
	inline bool setMaskDisturber(bool maskDist) const;
  
	inline state_t getState(void) const;
	inline uint8_t getInterruptFlags(void) const;
	inline uint8_t getDistance(void) const;
	inline bool getBusError(void) const;
	inline void clearBusError(void);
	inline bool getTriggered(void) const;

	bool calibrate(void);
  
private:
	bool _busError;
	uint8_t _address;
	uint8_t _tunCap;
	bool _indoor;
	uint8_t _interruptFlags;
	uint8_t _distance;
	SoftWire _i2c;
	volatile state_t _state; // Read but not modified by interrupt handler
	volatile bool _triggered;
	volatile uint16_t _calibrateCounter;
	void (*_timestampCB)(void);

	// Timer used when changing state. Used by the interrupt handler
	// when in listening state. 
	volatile AsyncDelay _delay;
};


void AS3935::interruptHandler(void)
{
	if (_state == stateListening) {
		if (_timestampCB)
			(*_timestampCB)();
    
		_delay.start(interruptDelay_ms, AsyncDelay::MILLIS);
		_triggered = true;
		_state = stateWaitingForResult;
	}
	else
		if (_state == stateCalibrate) {
			++_calibrateCounter;
		}
}

bool AS3935::presetDefault(void) const
{
	return writeRegister(regPresetDefault, 0x96);
}


bool AS3935::calibrateRCO(void) const
{
	return writeRegister(regCalibRCO, 0x96);
}


bool AS3935::setMaskDisturber(bool maskDist) const
{
	return setRegisterBit(regInt, 5, maskDist);
}


AS3935::state_t AS3935::getState(void) const
{
	return _state;
}


uint8_t AS3935::getInterruptFlags(void) const
{
	return _interruptFlags;
}


uint8_t AS3935::getDistance(void) const
{
	return _distance;
}

  
bool AS3935::getBusError(void) const
{
	return _busError;
}

void AS3935::clearBusError(void)
{
	_busError = false;
}

bool AS3935::getTriggered(void) const
{
	return _triggered;
}

#endif
