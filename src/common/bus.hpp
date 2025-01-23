/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ps1/system.h"

namespace bus {

/* Hardware serial port driver */

class UARTDriver {
public:
	virtual int init(int baud) const { return 0; }
	virtual bool isConnected(void) const { return true; }

	virtual uint8_t readByte(void) const { return 0; }
	virtual void writeByte(uint8_t value) const {}
	virtual bool isRXAvailable(void) const { return false; }
	virtual bool isTXFull(void) const { return false; }

	size_t readBytes(uint8_t *data, size_t length, int timeout = 0) const;
	void writeBytes(const uint8_t *data, size_t length) const;
};

class SIO1Driver : public UARTDriver {
public:
	int init(int baud) const;
	bool isConnected(void) const;

	uint8_t readByte(void) const;
	void writeByte(uint8_t value) const;
	bool isRXAvailable(void) const;
	bool isTXFull(void) const;
};

/* Bitbanged I2C driver */

class I2CDriver {
private:
	inline void _setSDA(bool value, int delay) const {
		_setSDA(value);
		delayMicroseconds(delay);
	}
	inline void _setSCL(bool value, int delay) const {
		_setSCL(value);
		delayMicroseconds(delay);
	}
	inline void _setCS(bool value, int delay) const {
		_setCS(value);
		delayMicroseconds(delay);
	}
	inline void _setReset(bool value, int delay) const {
		_setReset(value);
		delayMicroseconds(delay);
	}

	virtual bool _getSDA(void) const { return true; }
	virtual void _setSDA(bool value) const {}
	virtual void _setSCL(bool value) const {}
	virtual void _setCS(bool value) const {}
	virtual void _setReset(bool value) const {}

public:
	inline bool startDeviceRead(uint8_t address) const {
		start();
		writeByte((1 << 0) | (address << 1));
		return getACK();
	}
	inline bool startDeviceWrite(uint8_t address) const {
		start();
		writeByte((0 << 0) | (address << 1));
		return getACK();
	}

	void start(void) const;
	void startWithCS(int csDelay = 0) const;
	void stop(void) const;
	void stopWithCS(int csDelay = 0) const;

	bool getACK(void) const;
	void sendACK(bool value) const;
	uint8_t readByte(void) const;
	void writeByte(uint8_t value) const;

	void readBytes(uint8_t *data, size_t length) const;
	bool writeBytes(
		const uint8_t *data, size_t length, int lastACKDelay = 0
	) const;

	uint32_t resetX76(void) const;
	uint32_t resetZS01(void) const;
};

class I2CLock {
private:
	const I2CDriver &_driver;

public:
	inline I2CLock(const I2CDriver &driver)
	: _driver(driver) {
		_driver.start();
	}
	inline ~I2CLock(void) {
		_driver.stop();
	}
};

class I2CLockWithCS {
private:
	const I2CDriver &_driver;
	int             _csDelay;

public:
	inline I2CLockWithCS(const I2CDriver &driver, int csDelay = 0)
	: _driver(driver), _csDelay(csDelay) {
		_driver.startWithCS(_csDelay);
	}
	inline ~I2CLockWithCS(void) {
		_driver.stopWithCS(_csDelay);
	}
};

/* Bitbanged 1-wire driver */

class OneWireDriver {
private:
	inline void _set(bool value, int delay) const {
		_set(value);
		delayMicroseconds(delay);
	}

	virtual bool _get(void) const { return true; }
	virtual void _set(bool value) const {}

public:
	bool reset(void) const;

	uint8_t readByte(void) const;
	void writeByte(uint8_t value) const;

	bool readID(uint8_t *output) const;
};

}
