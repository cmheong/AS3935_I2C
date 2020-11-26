#include <AS3935.h>

AS3935::AS3935(void) : _busError(false),
					   _address(0x03),
					   _tunCap(0),
					   _indoor(true),
					   _interruptFlags(0),
					   _distance(0xFF),
					   _i2c(255,255),
					   _state(stateOff),
					   _triggered(false),
					   _calibrateCounter(0),
					   _timestampCB(NULL)
{
	;
}

bool AS3935::initialise(uint8_t sda, uint8_t scl, uint8_t address,
						uint8_t tunCap, bool indoor,
						void (*timestampCB)(void))
{
	_address = address;
	_tunCap = tunCap;
	_indoor = indoor;
	_triggered = false;
	_timestampCB = (void (*)())timestampCB;
	_i2c.setSda(sda);
	_i2c.setScl(scl);
	_i2c.setTimeout_ms(40);
	_i2c.setDelay_us(2);
	_i2c.begin();  // Sets up pin mode for SDA and SCL

	// Check presence of AS3935
	uint8_t s = _i2c.startWrite(_address);
	_i2c.stop();
	if (s == SoftWire::timedOut)
		return false;

	// Change _state first. Otherwise if it was set to stateListening
	// the interrupt handler could change delay to somewhing different.
	_state = statePoweringUp;
	_delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);
	return true;
}


bool AS3935::readRegister(uint8_t reg, uint8_t &val) const
{
	bool r = !(_i2c.startWrite(_address) ||
			   _i2c.llWrite(reg) ||
			   _i2c.repeatedStartRead(_address) ||
			   _i2c.readThenNack(val));
	_i2c.stop();
	return r;
}


bool AS3935::writeRegister(uint8_t reg, uint8_t val) const
{
	bool r = !(_i2c.startWrite(_address) ||
			   _i2c.llWrite(reg) ||
			   _i2c.llWrite(val));
	_i2c.stop();
	return r;
}

bool AS3935::setRegisterBit(uint8_t reg, uint8_t bit, bool bitValue) const
{
	uint8_t val;

	if (!readRegister(reg, val))
		return false;

	uint8_t bitMask = (1 << bit);
	if (bitValue)
		val |= bitMask; // Set bit
	else
		val &= ~bitMask; // Clear bit

	return writeRegister(reg, val);
}


bool AS3935::setIndoor(bool indoor) const
{
	uint8_t val;
	if (!readRegister(regAfeGain, val))
		return false;

	uint8_t mask = 0x3E;
	val &= ~mask;
	if (indoor)
		val |= 0x24;
	else
		val |= 0x1C;

	return writeRegister(regAfeGain, val);
}


bool AS3935::setNoiseFloor(uint8_t level) const
{
	uint8_t val;
	if (!readRegister(regNoiseFloor, val))
		return false;

	uint8_t mask = 0x70;
	val &= ~mask;
	val |= (level & 0x07) << 4;

	return writeRegister(regNoiseFloor, val);
}


bool AS3935::setThreshold(uint8_t level) const
{
	uint8_t val;
	if (!readRegister(regNoiseFloor, val))
		return false;

	uint8_t mask = 0x0f;
	val &= ~mask;
	val |= level & 0x0f;

	return writeRegister(regNoiseFloor, val);
}

bool AS3935::setSpikeRejection(uint8_t spikes) const
{
	uint8_t val;
	if (!readRegister(regSpikeRej, val))
		return false;

	uint8_t mask = 0x0f;
	val &= ~mask;
	val |= (spikes & mask);

	return writeRegister(regSpikeRej, val);
}


bool AS3935::setLCOFreqDiv16(void) const
{
	uint8_t val;
	if (!readRegister(regInt, val))
		return false;

	uint8_t mask = 0xC0;
	val &= ~mask;
	return writeRegister(regInt, val);
}


void AS3935::start(void)
{
	if (_state == stateOff) {
		// TO DO Set to listening mode?
		;
	}

	// Change _state first. Otherwise if it was set to stateListening
	// the interrupt handler could change delay to somewhing different.
	_state = statePoweringUp;
	_delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);
}


bool AS3935::process(void)
{
	bool r = false;
	switch (_state) {
	case stateOff:
		// Stay powered off until told to turn on
		break;

	case statePoweringUp:
		if (_delay.isExpired()) {
			if (presetDefault() && writeRegister(regTunCap, _tunCap & 0x0F)) {
				_delay.start(initialisation1Delay_ms, AsyncDelay::MILLIS);
				_state = stateInitialising1;
			}
			else
				_busError = true;
		}
		break;

	case stateInitialising1:
		if (_delay.isExpired()) {
			// Calibrate RCO
			if (calibrateRCO() && setRegisterBit(regTunCap, 5, true)) {
				_delay.start(initialisation2Delay_ms, AsyncDelay::MILLIS);
				_state = stateInitialising2;
			}
			else
				_busError = true;
		}
		break;

	case stateInitialising2:
		if (_delay.isExpired()) {
			// Set register 0x08, bit 5 to 0
			if (setRegisterBit(regTunCap, 5, false)) {
				// Clear any existing interrupt status by reading the interrupts
				uint8_t tmp;
				readRegister(regInt, tmp);
				_state = stateListening;
			}
			else
				_busError = true;
		}
		break;

	case stateListening:
		break;

	case stateWaitingForResult:
		if (_delay.isExpired()) {
			// Read distance and cause of interrupt
			if (readRegister(regInt, _interruptFlags)
				&& readRegister(regDistance, _distance)) {
				_interruptFlags &= 0x0D;
				_distance &= 0x3F;
				// From the datasheet:
				// "If the AS3935 issues an interrupt and the
				// Interrupt register is REG0x03[3:0] = 000 the
				// distance estimation has changed due to purging of
				// old events in the statistics, based on the
				// lightning distance estimation algorithm."
				r = (_interruptFlags != 0);
			}
			else {
				_busError = true;
				_interruptFlags = 0;
				_distance = 0xFF;
			}
			_triggered = false;
			_state = stateListening;
		}
		break;

	case stateCalibrate:
		break;
	}

	return r;
}


bool AS3935::calibrate(void)
{
	uint8_t bestTuning = 0;
	int16_t bestError = 0x7FFF;
	state_t lastState = _state;
	_state = stateCalibrate;

	setLCOFreqDiv16();


	for (uint8_t tuning = 0; tuning < 16; ++tuning) {

		// Set tuning and enable LCO output on INT pin
		writeRegister(regTunCap, 0x80 | tuning);

		delay(10);

		noInterrupts();
		_calibrateCounter = 0;
		interrupts();

		delay(40);

		noInterrupts();
		int16_t errorVal = _calibrateCounter - 1250;
		interrupts();

		Serial.print("Tuning value: ");
		Serial.print(tuning, DEC);
		Serial.print(", error: ");
		Serial.println(errorVal, DEC);

		if (errorVal < 0)
			errorVal = -errorVal;
		if (errorVal < bestError) {
			bestError = errorVal;
			bestTuning = tuning;
		}
	}

	// Set to best tuning and disable LCO output on INT pin
	writeRegister(regTunCap, bestTuning);

	//_tunCap = bestTuning;
	// TODO: do the right thing to return to the correct state
	initialise(_i2c.getSda(), _i2c.getScl(), _address, bestTuning,
			   _indoor, (void (*)())_timestampCB);

	//lastState = _state;
	return true;
}
