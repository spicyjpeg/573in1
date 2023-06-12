
#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"
#include "ps1/system.h"
#include "io.hpp"
#include "util.hpp"

namespace io {

uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

void init(void) {
	_bankSwitchReg = 0;
	_cartOutputReg = 0;
	_miscOutputReg = 0x0107;

	BIU_DEV0_ADDR = DEV0_BASE & 0x1fffffff;
	BIU_DEV0_CTRL = 0
		| (7 << 0) // Write delay
		| (4 << 4) // Read delay
		| BIU_CTRL_RECOVERY
		| BIU_CTRL_HOLD
		| BIU_CTRL_FLOAT
		| BIU_CTRL_PRESTROBE
		| BIU_CTRL_WIDTH_16
		| BIU_CTRL_AUTO_INCR
		| (23 << 16) // Number of address lines
		| ( 4 << 24) // DMA read/write delay
		| BIU_CTRL_DMA_DELAY;

	SYS573_WATCHDOG  = 0;
	SYS573_BANK_CTRL = 0;
	SYS573_CART_OUT  = 0;
	SYS573_MISC_OUT  = 0x0107;

	// Some of the digital I/O board's light outputs are controlled by the FPGA
	// and cannot be turned off until the FPGA is initialized.
	if (isDigitalIOPresent()) {
		//SYS573D_CPLD_LIGHTS_B0 = 0xf000;
		SYS573D_CPLD_LIGHTS_C0 = 0xf000;
		SYS573D_CPLD_LIGHTS_C1 = 0xf000;
	} else {
		SYS573A_LIGHTS_A = 0x00ff;
		SYS573A_LIGHTS_B = 0x00ff;
		SYS573A_LIGHTS_C = 0x00ff;
		SYS573A_LIGHTS_D = 0x00ff;
	}
}

uint32_t getJAMMAInputs(void) {
	uint32_t inputs;

	inputs  =  SYS573_JAMMA_MAIN;
	inputs |= (SYS573_JAMMA_EXT1 & 0x0f00) << 8;
	inputs |= (SYS573_JAMMA_EXT2 & 0x0f00) << 12;
	inputs |= (SYS573_MISC_IN    & 0x1f00) << 16;

	return inputs ^ 0x1fffffff;
}

uint32_t getRTCTime(void) {
	SYS573_RTC_CTRL |= SYS573_RTC_CTRL_READ;

	int year = SYS573_RTC_YEAR, month = SYS573_RTC_MONTH,  day = SYS573_RTC_DAY;
	int hour = SYS573_RTC_HOUR, min   = SYS573_RTC_MINUTE, sec = SYS573_RTC_SECOND;

	year  = (year  & 15) + 10 * ((year  >> 4) & 15); // 0-99
	month = (month & 15) + 10 * ((month >> 4) &  1); // 1-12
	day   = (day   & 15) + 10 * ((day   >> 4) &  3); // 1-31
	hour  = (hour  & 15) + 10 * ((hour  >> 4) &  3); // 0-23
	min   = (min   & 15) + 10 * ((min   >> 4) &  7); // 0-59
	sec   = (sec   & 15) + 10 * ((sec   >> 4) &  7); // 0-59

	// Return all values packed into a FAT/MS-DOS-style bitfield. Assume the
	// year is always in 1995-2094 range.
	int _year = (year >= 95) ? (year + 1900 - 1980) : (year + 2000 - 1980);

	return 0
		| (_year << 25)
		| (month << 21)
		| (day   << 16)
		| (hour  << 11)
		| (min   <<  5)
		| (sec   >>  1);
}

/* Digital I/O board driver */

static void _writeBitstreamLSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++);

		for (int i = 8; i; i--, bits >>= 1)
			SYS573D_CPLD_BITSTREAM = (bits & 1) << 15;
	}
}

static void _writeBitstreamMSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++) << 8;

		for (int i = 8; i; i--, bits <<= 1)
			SYS573D_CPLD_BITSTREAM = bits & (1 << 15);
	}
}

bool loadBitstream(const uint8_t *data, size_t length) {
	if (data[0] != 0xff)
		return false;

	// Konami's bitstreams are always stored LSB first, however Xilinx tools
	// seem to export bitstreams MSB first by default. The only way out of this
	// mess is to autodetect the bit order by checking for preamble and frame
	// start sequences, as specified in the XCS40XL datasheet.
	uint8_t id1 = data[1], id2 = data[4];
	void    (*writeFunc)(const uint8_t *, size_t);

	if (((id1 & 0x0f) == 0x04) && ((id2 & 0xf0) == 0xf0))
		writeFunc = &_writeBitstreamLSB;
	else if (((id1 & 0xf0) == 0x20) && ((id2 & 0x0f) == 0x0f))
		writeFunc = &_writeBitstreamMSB;
	else
		return false;

	for (int i = 3; i; i--) {
		SYS573D_CPLD_UNK_RESET = 0;

		SYS573D_CPLD_CTRL = SYS573D_CPLD_CTRL_UNK4;
		SYS573D_CPLD_CTRL = SYS573D_CPLD_CTRL_UNK4 | SYS573D_CPLD_CTRL_UNK3;
		SYS573D_CPLD_CTRL = SYS573D_CPLD_CTRL_UNK4 | SYS573D_CPLD_CTRL_UNK3 |
			SYS573D_CPLD_CTRL_UNK2 | SYS573D_CPLD_CTRL_UNK1;
		delayMicroseconds(5000);

		if (!(SYS573D_CPLD_STAT & SYS573D_CPLD_STAT_INIT))
			continue;

		writeFunc(data, length);

		for (int j = 15; j; j--) {
			if (
				(SYS573D_CPLD_STAT & (SYS573D_CPLD_STAT_INIT | SYS573D_CPLD_STAT_DONE))
				== (SYS573D_CPLD_STAT_INIT | SYS573D_CPLD_STAT_DONE)
			)
				return true;

			delayMicroseconds(1000);
		}
	}

	return false;
}

void initKonamiBitstream(void) {
	SYS573D_FPGA_INIT = 0xf000;
	SYS573D_FPGA_INIT = 0x0000;
	delayMicroseconds(1000);

	SYS573D_FPGA_INIT = 0xf000;
	delayMicroseconds(1000);

	// Turn off all lights including the ones that were left on by init().
	SYS573D_FPGA_LIGHTS_A0 = 0xf000;
	SYS573D_FPGA_LIGHTS_A1 = 0xf000;
	SYS573D_CPLD_LIGHTS_B0 = 0xf000;
	SYS573D_FPGA_LIGHTS_B1 = 0xf000;
	SYS573D_CPLD_LIGHTS_C0 = 0xf000;
	SYS573D_CPLD_LIGHTS_C1 = 0xf000;
	SYS573D_FPGA_LIGHTS_D0 = 0xf000;
}

/* I2C driver */

// SDA is open-drain so it is toggled by changing pin direction.
#define _SDA(value)   setCartSDADir(!(value))
#define SDA(value)    _SDA(value), delayMicroseconds(20)
#define _SCL(value)   setCartOutput(OUT_SCL, value)
#define SCL(value)    _SCL(value), delayMicroseconds(20)
#define _CS(value)    setCartOutput(OUT_CS, value)
#define CS(value)     _CS(value), delayMicroseconds(20)
#define _RESET(value) setCartOutput(OUT_RESET, value)
#define RESET(value)  _RESET(value), delayMicroseconds(20)

void i2cStart(void) {
	_SDA(true);
	SCL(true);

	SDA(false); // START: SDA falling, SCL high
	SCL(false);
}

void i2cStartWithCS(int csDelay) {
	_SDA(true);
	_SCL(true);
	CS(true);

	CS(false);
	delayMicroseconds(csDelay);
	SDA(false); // START: SDA falling, SCL high
	SCL(false);
}

void i2cStop(void) {
	_SDA(false);
	SCL(true);

	SDA(true); // STOP: SDA rising, SCL high
}

void i2cStopWithCS(int csDelay) {
	_SDA(false);
	SCL(true);

	SDA(true); // STOP: SDA rising, SCL high
	delayMicroseconds(csDelay);
	CS(true);
}

uint8_t i2cReadByte(void) {
	uint8_t value = 0;

	for (int bit = 7; bit >= 0; bit--) { // MSB first
		SCL(true);
		if (getCartSDA())
			value |= (1 << bit);
		SCL(false);
	}

	delayMicroseconds(20);
	return value;
}

void i2cWriteByte(uint8_t value) {
	for (int bit = 7; bit >= 0; bit--) { // MSB first
		_SDA(value & (1 << bit));
		SCL(true);
		SCL(false);
	}

	SDA(true);
}

void i2cSendACK(bool ack) {
	_SDA(!ack);
	SCL(true);
	SCL(false);
	SDA(true);
}

bool i2cGetACK(void) {
	delayMicroseconds(20); // Required for ZS01
	SCL(true);
	bool ack = !getCartSDA();
	SCL(false);

	delayMicroseconds(20);
	return ack;
}

void i2cReadBytes(uint8_t *data, size_t length) {
	for (; length; length--) {
		*(data++) = i2cReadByte();

		if (length > 1)
			i2cSendACK(true);
	}
}

bool i2cWriteBytes(const uint8_t *data, size_t length, int lastACKDelay) {
	for (; length; length--) {
		i2cWriteByte(*(data++));

		if (length == 1)
			delayMicroseconds(lastACKDelay);
		if (!i2cGetACK())
			return false;
	}

	return true;
}

uint32_t i2cResetX76(void) {
	uint32_t value = 0;

	_SDA(true);
	_SCL(false);
	_CS(false);
	_RESET(false);

	RESET(true);
	SCL(true);
	SCL(false);
	RESET(false);

	for (int bit = 0; bit < 32; bit++) { // LSB first
		SCL(true);
		if (getCartSDA())
			value |= (1 << bit);
		SCL(false);
	}

	SCL(true);
	CS(true);
	return value;
}

// For whatever reason the ZS01 does not implement the exact same response to
// reset protocol as the X76 chips. The reset pin is also active-low rather
// than active-high, and CS is ignored.
uint32_t i2cResetZS01(void) {
	uint32_t value = 0;

	_SDA(true);
	_SCL(false);
	_CS(false);
	_RESET(true);

	RESET(false);
	RESET(true);
	delayMicroseconds(100);

	SCL(true);
	SCL(false);

	for (int bit = 31; bit >= 0; bit--) { // MSB first
		if (getCartSDA())
			value |= (1 << bit);
		SCL(true);
		SCL(false);
	}

	SCL(true);
	return value;
}

/* 1-wire driver */

#define _CART1WIRE(value) setCartOutput(OUT_1WIRE, !(value))
#define _DIO1WIRE(value)  setDIO1Wire(value)

bool dsCartReset(void) {
	_CART1WIRE(false);
	delayMicroseconds(480);
	_CART1WIRE(true);

	delayMicroseconds(60);
	bool present = !getCartInput(IN_1WIRE);
	delayMicroseconds(60);

	delayMicroseconds(1000);
	return present;
}

bool dsDIOReset(void) {
	_DIO1WIRE(false);
	delayMicroseconds(480);
	_DIO1WIRE(true);

	delayMicroseconds(60);
	bool present = !getDIO1Wire();
	delayMicroseconds(60);

	delayMicroseconds(1000);
	return present;
}

uint8_t dsCartReadByte(void) {
	uint8_t value = 0;

	for (int bit = 0; bit < 8; bit++) { // LSB first
		_CART1WIRE(false);
		delayMicroseconds(2);
		_CART1WIRE(true);
		delayMicroseconds(10);
		if (getCartInput(IN_1WIRE))
			value |= (1 << bit);
		delayMicroseconds(50);
	}

	return value;
}

uint8_t dsDIOReadByte(void) {
	uint8_t value = 0;

	for (int bit = 0; bit < 8; bit++) { // LSB first
		_DIO1WIRE(false);
		delayMicroseconds(2);
		_DIO1WIRE(true);
		delayMicroseconds(10);
		if (getDIO1Wire())
			value |= (1 << bit);
		delayMicroseconds(50);
	}

	return value;
}

void dsCartWriteByte(uint8_t value) {
	for (int bit = 0; bit < 8; bit++) { // LSB first
		if (value & (1 << bit)) {
			_CART1WIRE(false);
			delayMicroseconds(2);
			_CART1WIRE(true);
			delayMicroseconds(60);
		} else {
			_CART1WIRE(false);
			delayMicroseconds(60);
			_CART1WIRE(true);
			delayMicroseconds(2);
		}
	}
}

void dsDIOWriteByte(uint8_t value) {
	for (int bit = 0; bit < 8; bit++) { // LSB first
		if (value & (1 << bit)) {
			_DIO1WIRE(false);
			delayMicroseconds(2);
			_DIO1WIRE(true);
			delayMicroseconds(60);
		} else {
			_DIO1WIRE(false);
			delayMicroseconds(60);
			_DIO1WIRE(true);
			delayMicroseconds(2);
		}
	}
}

}
