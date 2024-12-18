/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
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
#include <stdio.h>
#include "common/util/misc.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace io {

/* Register and bit definitions */

enum JAMMAInput : uint32_t {
	// SYS573_JAMMA_MAIN
	JAMMA_P2_LEFT    = 1 <<  0,
	JAMMA_P2_RIGHT   = 1 <<  1,
	JAMMA_P2_UP      = 1 <<  2,
	JAMMA_P2_DOWN    = 1 <<  3,
	JAMMA_P2_BUTTON1 = 1 <<  4,
	JAMMA_P2_BUTTON2 = 1 <<  5,
	JAMMA_P2_BUTTON3 = 1 <<  6,
	JAMMA_P2_START   = 1 <<  7,
	JAMMA_P1_LEFT    = 1 <<  8,
	JAMMA_P1_RIGHT   = 1 <<  9,
	JAMMA_P1_UP      = 1 << 10,
	JAMMA_P1_DOWN    = 1 << 11,
	JAMMA_P1_BUTTON1 = 1 << 12,
	JAMMA_P1_BUTTON2 = 1 << 13,
	JAMMA_P1_BUTTON3 = 1 << 14,
	JAMMA_P1_START   = 1 << 15,

	// SYS573_JAMMA_EXT1
	JAMMA_P1_BUTTON4 = 1 << 16,
	JAMMA_P1_BUTTON5 = 1 << 17,
	JAMMA_TEST       = 1 << 18,
	JAMMA_P1_BUTTON6 = 1 << 19,

	// SYS573_JAMMA_EXT2
	JAMMA_P2_BUTTON4 = 1 << 20,
	JAMMA_P2_BUTTON5 = 1 << 21,
	JAMMA_RAM_LAYOUT = 1 << 22,
	JAMMA_P2_BUTTON6 = 1 << 23,

	// SYS573_MISC_IN
	JAMMA_COIN1      = 1 << 24,
	JAMMA_COIN2      = 1 << 25,
	JAMMA_PCMCIA_CD1 = 1 << 26,
	JAMMA_PCMCIA_CD2 = 1 << 27,
	JAMMA_SERVICE    = 1 << 28
};

enum CartInputPin {
	CART_INPUT_DS2401 = 6
};

enum CartOutputPin {
	CART_OUTPUT_SDA    = 0,
	CART_OUTPUT_SCL    = 1,
	CART_OUTPUT_CS     = 2,
	CART_OUTPUT_RESET  = 3,
	CART_OUTPUT_DS2401 = 4
};

enum MiscOutputPin {
	MISC_OUT_ADC_DI      = 0,
	MISC_OUT_ADC_CS      = 1,
	MISC_OUT_ADC_CLK     = 2,
	MISC_OUT_COIN_COUNT1 = 3,
	MISC_OUT_COIN_COUNT2 = 4,
	MISC_OUT_AMP_ENABLE  = 5,
	MISC_OUT_CDDA_ENABLE = 6,
	MISC_OUT_SPU_ENABLE  = 7,
	MISC_OUT_JVS_RESET   = 8
};

/* Inputs */

static inline void clearWatchdog(void) {
	SYS573_WATCHDOG = 0;
}

static inline bool isDualBankRAM(void) {
	return (SYS573_JAMMA_EXT2 >> 10) & 1;
}

static inline bool getDIPSwitch(int bit) {
	return !((SYS573_DIP_CART >> bit) & 1);
}

static inline bool getCartInsertionStatus(void) {
	return (SIO_STAT(1) / SIO_STAT_DSR) & 1;
}

static inline bool getCartSerialStatus(void) {
	SIO_CTRL(1) |= SIO_CTRL_RTS;
	return (SIO_STAT(1) / SIO_STAT_CTS) & 1;
}

/* Bitbanged I/O */

extern uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

static inline bool getCartInput(CartInputPin pin) {
	return (SYS573_DIP_CART >> (8 + pin)) & 1;
}

static inline void setCartOutput(CartOutputPin pin, bool value) {
	if (value)
		_cartOutputReg |= 1 << pin;
	else
		_cartOutputReg &= ~(1 << pin);

	SYS573_CART_OUT = _cartOutputReg;
}

static inline void setFlashBank(int bank) {
	_bankSwitchReg = (_bankSwitchReg & (3 << 6)) | bank;

	SYS573_BANK_CTRL = _bankSwitchReg;
}

static inline void setCartSDADirection(bool dir) {
	if (dir)
		_bankSwitchReg |= 1 << 6;
	else
		_bankSwitchReg &= ~(1 << 6);

	SYS573_BANK_CTRL = _bankSwitchReg;
}

static inline void setMiscOutput(MiscOutputPin pin, bool value) {
	if (value)
		_miscOutputReg |= 1 << pin;
	else
		_miscOutputReg &= ~(1 << pin);

	SYS573_MISC_OUT = _miscOutputReg;
}

/* System initialization */

void init(void);
void resetIDEDevices(void);

/* System bus DMA */

size_t doDMARead(
	volatile void *source, void *data, size_t length, bool wait = false
);
size_t doDMAWrite(
	volatile void *dest, const void *data, size_t length, bool wait = false
);

/* JAMMA and RTC functions */

uint32_t getJAMMAInputs(void);
void getRTCTime(util::Date &output);
void setRTCTime(const util::Date &value, bool stop = false);
bool isRTCBatteryLow(void);

/* I2C driver */

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

/* 1-wire driver */

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
};

/* Security cartridge bus APIs */

class CartI2CDriver : public I2CDriver {
private:
	bool _getSDA(void) const;
	void _setSDA(bool value) const;
	void _setSCL(bool value) const;
	void _setCS(bool value) const;
	void _setReset(bool value) const;
};

class CartDS2401Driver : public OneWireDriver {
private:
	bool _get(void) const;
	void _set(bool value) const;
};

extern const CartI2CDriver    cartI2C;
extern const CartDS2401Driver cartDS2401;

}
