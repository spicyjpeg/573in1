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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/util/misc.hpp"
#include "common/util/string.hpp"
#include "common/util/templates.hpp"
#include "common/io.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace io {

uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

/* System initialization */

static constexpr int _IDE_RESET_ASSERT_DELAY = 5000;
static constexpr int _IDE_RESET_CLEAR_DELAY  = 50000;

void init(void) {
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

#if 0
	// Revision D of the main board has footprints for either eight 8-bit RAM
	// chips wired as two 32-bit banks, or two 16-bit chips wired as a single
	// bank.
	DRAM_CTRL = isDualBankRAM() ? 0x0c80 : 0x4788;
#endif

	_bankSwitchReg = 0;
	_cartOutputReg = 0;
	_miscOutputReg = 0
		| SYS573_MISC_OUT_ADC_DI
		| SYS573_MISC_OUT_ADC_CS
		| SYS573_MISC_OUT_ADC_CLK
		| SYS573_MISC_OUT_JVS_RESET;

	SYS573_BANK_CTRL = _bankSwitchReg;
	SYS573_CART_OUT  = _cartOutputReg;
	SYS573_MISC_OUT  = _miscOutputReg;

	clearWatchdog();
}

void resetIDEDevices(void) {
	SYS573_IDE_RESET = 0;
	delayMicroseconds(_IDE_RESET_ASSERT_DELAY);

	SYS573_IDE_RESET = 1;
	delayMicroseconds(_IDE_RESET_CLEAR_DELAY);
}

/* System bus DMA */

static constexpr int _DMA_TIMEOUT = 100000;

size_t doDMARead(volatile void *source, void *data, size_t length, bool wait) {
	length = (length + 3) / 4;

	util::assertAligned<uint32_t>(data);

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT))
		return 0;

	// The BIU will output the base address set through this register over the
	// address lines during a DMA transfer. This does not affect non-DMA access
	// as the BIU will realign the address by masking off the bottommost N bits
	// (where N is the number of address lines used) and replace them with the
	// respective CPU address bits.
	BIU_DEV0_ADDR = reinterpret_cast<uint32_t>(source) & 0x1fffffff;

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (wait)
		waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);

	return length * 4;
}

size_t doDMAWrite(
	volatile void *dest, const void *data, size_t length, bool wait
) {
	length = (length + 3) / 4;

	util::assertAligned<uint32_t>(data);

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT))
		return 0;

	BIU_DEV0_ADDR = reinterpret_cast<uint32_t>(dest) & 0x1fffffff;

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (wait)
		waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);

	return length * 4;
}

/* JAMMA and RTC functions */

uint32_t getJAMMAInputs(void) {
	uint32_t inputs;

	inputs  =  SYS573_JAMMA_MAIN;
	inputs |= (SYS573_JAMMA_EXT1 & 0x0f00) << 8;
	inputs |= (SYS573_JAMMA_EXT2 & 0x0f00) << 12;
	inputs |= (SYS573_MISC_IN    & 0x1f00) << 16;

	return inputs ^ 0x1fffffff;
}

void getRTCTime(util::Date &output) {
	SYS573_RTC_CTRL |= SYS573_RTC_CTRL_READ;

	auto second = SYS573_RTC_SECOND, minute = SYS573_RTC_MINUTE;
	auto hour   = SYS573_RTC_HOUR,   day    = SYS573_RTC_DAY;
	auto month  = SYS573_RTC_MONTH,  year   = SYS573_RTC_YEAR;

	SYS573_RTC_CTRL &= ~SYS573_RTC_CTRL_READ;

	output.year   = (year   & 15) + 10 * ((year   >> 4) & 15); // 0-99
	output.month  = (month  & 15) + 10 * ((month  >> 4) &  1); // 1-12
	output.day    = (day    & 15) + 10 * ((day    >> 4) &  3); // 1-31
	output.hour   = (hour   & 15) + 10 * ((hour   >> 4) &  3); // 0-23
	output.minute = (minute & 15) + 10 * ((minute >> 4) &  7); // 0-59
	output.second = (second & 15) + 10 * ((second >> 4) &  7); // 0-59

	output.year += (output.year < 70) ? 2000 : 1900;
}

void setRTCTime(const util::Date &value, bool stop) {
	assert((value.year >= 1970) && (value.year <= 2069));

	int _year   = value.year % 100;
	int weekday = value.getDayOfWeek() + 1;

	int year    = (_year        % 10) | (((_year        / 10) & 15) << 4);
	int month   = (value.month  % 10) | (((value.month  / 10) &  1) << 4);
	int day     = (value.day    % 10) | (((value.day    / 10) &  3) << 4);
	int hour    = (value.hour   % 10) | (((value.hour   / 10) &  3) << 4);
	int minute  = (value.minute % 10) | (((value.minute / 10) &  7) << 4);
	int second  = (value.second % 10) | (((value.second / 10) &  7) << 4);

	SYS573_RTC_CTRL |= SYS573_RTC_CTRL_WRITE;

	SYS573_RTC_SECOND  = second
		| (stop ? SYS573_RTC_SECOND_STOP : 0);
	SYS573_RTC_MINUTE  = minute;
	SYS573_RTC_HOUR    = hour;
	SYS573_RTC_WEEKDAY = weekday
		| SYS573_RTC_WEEKDAY_CENTURY
		| SYS573_RTC_WEEKDAY_CENTURY_ENABLE;
	SYS573_RTC_DAY     = day
		| SYS573_RTC_DAY_BATTERY_MONITOR;
	SYS573_RTC_MONTH   = month;
	SYS573_RTC_YEAR    = year;

	SYS573_RTC_CTRL &= ~SYS573_RTC_CTRL_WRITE;
}

bool isRTCBatteryLow(void) {
	SYS573_RTC_DAY |= SYS573_RTC_DAY_BATTERY_MONITOR;

	return (SYS573_RTC_DAY / SYS573_RTC_DAY_LOW_BATTERY) & 1;
}

/* I2C driver */

static constexpr int _I2C_BUS_DELAY   = 50;
static constexpr int _I2C_RESET_DELAY = 500;

void I2CDriver::start(void) const {
	_setSDA(true);
	_setSCL(true,  _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::startWithCS(int csDelay) const {
	_setSDA(true);
	_setSCL(false);
	_setCS (true, _I2C_BUS_DELAY);

	_setCS (false, _I2C_BUS_DELAY + csDelay);
	_setSCL(true,  _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::stop(void) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high
}

void I2CDriver::stopWithCS(int csDelay) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high

	_setSCL(false, _I2C_BUS_DELAY + csDelay);
	_setCS (true,  _I2C_BUS_DELAY);
}

bool I2CDriver::getACK(void) const {
	delayMicroseconds(_I2C_BUS_DELAY); // Required for ZS01

	_setSCL(true,  _I2C_BUS_DELAY);
	bool ack = _getSDA();
	_setSCL(false, _I2C_BUS_DELAY * 2);

	return ack ^ 1;
}

void I2CDriver::sendACK(bool ack) const {
	_setSDA(ack ^ 1);
	_setSCL(true,  _I2C_BUS_DELAY);
	_setSCL(false, _I2C_BUS_DELAY);
	_setSDA(true,  _I2C_BUS_DELAY);
}

uint8_t I2CDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 7; i >= 0; i--) { // MSB first
		_setSCL(true,  _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	delayMicroseconds(_I2C_BUS_DELAY);
	return value;
}

void I2CDriver::writeByte(uint8_t value) const {
	for (int i = 7; i >= 0; i--) { // MSB first
		_setSDA((value >> i) & 1);
		_setSCL(true,  _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSDA(true, _I2C_BUS_DELAY);
}

void I2CDriver::readBytes(uint8_t *data, size_t length) const {
	for (; length; length--) {
		*(data++) = readByte();

		if (length > 1)
			sendACK(true);
	}
}

bool I2CDriver::writeBytes(
	const uint8_t *data, size_t length, int lastACKDelay
) const {
	for (; length; length--) {
		writeByte(*(data++));

		if (length == 1)
			delayMicroseconds(lastACKDelay);
		if (!getACK())
			return false;
	}

	return true;
}

uint32_t I2CDriver::resetX76(void) const {
	uint32_t value = 0;

	_setSDA  (true);
	_setSCL  (false);
	_setCS   (false);
	_setReset(false);

	_setReset(true,  _I2C_RESET_DELAY);
	_setSCL  (true,  _I2C_BUS_DELAY);
	_setSCL  (false, _I2C_BUS_DELAY);
	_setReset(false, _I2C_RESET_DELAY);

	for (int i = 0; i < 32; i++) { // LSB first
		_setSCL(true,  _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setCS (true, _I2C_BUS_DELAY);
	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

// For whatever reason the ZS01 does not implement the exact same response to
// reset protocol as the X76 chips. The reset pin is also active-low rather
// than active-high, and CS is ignored.
uint32_t I2CDriver::resetZS01(void) const {
	uint32_t value = 0;

	_setSDA  (true);
	_setSCL  (false);
	_setCS   (false);
	_setReset(true);

	_setReset(false, _I2C_RESET_DELAY);
	_setReset(true,  _I2C_RESET_DELAY);
	_setSCL  (true,  _I2C_BUS_DELAY);
	_setSCL  (false, _I2C_BUS_DELAY);

	for (int i = 31; i >= 0; i--) { // MSB first
		value |= _getSDA() << i;
		_setSCL(true,  _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

/* 1-wire driver */

static constexpr int _DS_RESET_LOW_TIME     = 480;
static constexpr int _DS_RESET_SAMPLE_DELAY = 70;
static constexpr int _DS_RESET_DELAY        = 410;

static constexpr int _DS_READ_LOW_TIME     = 3;
static constexpr int _DS_READ_SAMPLE_DELAY = 10;
static constexpr int _DS_READ_DELAY        = 53;

static constexpr int _DS_ZERO_LOW_TIME  = 65;
static constexpr int _DS_ZERO_HIGH_TIME = 5;
static constexpr int _DS_ONE_LOW_TIME   = 10;
static constexpr int _DS_ONE_HIGH_TIME  = 55;

bool OneWireDriver::reset(void) const {
	_set(false, _DS_RESET_LOW_TIME);
	_set(true,  _DS_RESET_SAMPLE_DELAY);
	bool present = _get();

	delayMicroseconds(_DS_RESET_DELAY);
	return present ^ 1;
}

uint8_t OneWireDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 0; i < 8; i++) { // LSB first
		_set(false, _DS_READ_LOW_TIME);
		_set(true,  _DS_READ_SAMPLE_DELAY);
		value |= _get() << i;
		delayMicroseconds(_DS_READ_DELAY);
	}

	return value;
}

void OneWireDriver::writeByte(uint8_t value) const {
	for (int i = 8; i; i--, value >>= 1) { // LSB first
		if (value & 1) {
			_set(false, _DS_ONE_LOW_TIME);
			_set(true,  _DS_ONE_HIGH_TIME);
		} else {
			_set(false, _DS_ZERO_LOW_TIME);
			_set(true,  _DS_ZERO_HIGH_TIME);
		}
	}
}

/* Security cartridge bus APIs */

bool CartI2CDriver::_getSDA(void) const {
	return (SYS573_MISC_IN / SYS573_MISC_IN_CART_SDA) & 1;
}

void CartI2CDriver::_setSDA(bool value) const {
	// SDA is open-drain so it is toggled by tristating the pin.
	setCartOutput(CART_OUTPUT_SDA, false);
	setCartSDADirection(value ^ 1);
}

void CartI2CDriver::_setSCL(bool value) const {
	setCartOutput(CART_OUTPUT_SCL, value);
}

void CartI2CDriver::_setCS(bool value) const {
	setCartOutput(CART_OUTPUT_CS, value);
}

void CartI2CDriver::_setReset(bool value) const {
	setCartOutput(CART_OUTPUT_RESET, value);
}

bool CartDS2401Driver::_get(void) const {
	return getCartInput(CART_INPUT_DS2401);
}

void CartDS2401Driver::_set(bool value) const {
	setCartOutput(CART_OUTPUT_DS2401, value ^ 1);
}

const CartI2CDriver    cartI2C;
const CartDS2401Driver cartDS2401;

/* Debug LCD driver */

/*
 * This is a simple driver for a character LCD module, wired to the EXT-OUT
 * connector on the 573 main board as follows:
 *
 * | EXT-OUT pin | LCD pin |
 * | ----------: | :------ |
 * |        1, 2 | `VCC`   |
 * |           5 | `E`     |
 * |           6 | `RS`    |
 * |           7 | `D7`    |
 * |           8 | `D6`    |
 * |           9 | `D5`    |
 * |          10 | `D4`    |
 * |      11, 12 | `GND`   |
 *
 * The `R/W` pin must be shorted to ground, while `V0` (bias voltage) shall be
 * connected to ground through an appropriate resistor or potentiometer to set
 * the display's contrast.
 */

enum DebugLCDPin {
	_LCD_PIN_D0 = 0,
	_LCD_PIN_RS = 4,
	_LCD_PIN_E  = 5
};

enum DebugLCDCommand : uint8_t {
	_LCD_CMD_CLEAR = 1 << 0,
	_LCD_CMD_HOME  = 1 << 1,

	_LCD_CMD_ENTRY_MODE_SHIFT = 1 << 0,
	_LCD_CMD_ENTRY_MODE_DEC   = 0 << 1,
	_LCD_CMD_ENTRY_MODE_INC   = 1 << 1,
	_LCD_CMD_ENTRY_MODE       = 1 << 2,

	_LCD_CMD_DISPLAY_MODE_BLINK  = 1 << 0,
	_LCD_CMD_DISPLAY_MODE_CURSOR = 1 << 1,
	_LCD_CMD_DISPLAY_MODE_ON     = 1 << 2,
	_LCD_CMD_DISPLAY_MODE        = 1 << 3,

	_LCD_CMD_MOVE_LEFT    = 0 << 2,
	_LCD_CMD_MOVE_RIGHT   = 1 << 2,
	_LCD_CMD_MOVE_CURSOR  = 0 << 3,
	_LCD_CMD_MOVE_DISPLAY = 1 << 3,
	_LCD_CMD_MOVE         = 1 << 4,

	_LCD_CMD_FUNCTION_SET_HEIGHT_8  = 0 << 2,
	_LCD_CMD_FUNCTION_SET_HEIGHT_11 = 1 << 2,
	_LCD_CMD_FUNCTION_SET_ROWS_1    = 0 << 3,
	_LCD_CMD_FUNCTION_SET_ROWS_2    = 1 << 3,
	_LCD_CMD_FUNCTION_SET_BUS_4BIT  = 0 << 4,
	_LCD_CMD_FUNCTION_SET_BUS_8BIT  = 1 << 4,
	_LCD_CMD_FUNCTION_SET           = 1 << 5,

	_LCD_CMD_SET_CGRAM_PTR = 1 << 6,
	_LCD_CMD_SET_DDRAM_PTR = 1 << 7
};

static constexpr int _LCD_WRITE_PULSE_TIME = 1;
static constexpr int _LCD_WRITE_DELAY      = 50;
static constexpr int _LCD_INIT_DELAY       = 5000;

void DebugLCD::_writeNibble(uint8_t value, bool isCmd) const {
	uint8_t outputs = 0
		| ((value & 15) << _LCD_PIN_D0)
		| ((isCmd ^ 1)  << _LCD_PIN_RS);

	SYS573_EXT_OUT = outputs;
	delayMicroseconds(_LCD_WRITE_PULSE_TIME);

	SYS573_EXT_OUT = outputs | (1 << _LCD_PIN_E);
	delayMicroseconds(_LCD_WRITE_PULSE_TIME);

	SYS573_EXT_OUT = outputs;
	delayMicroseconds(_LCD_WRITE_DELAY);
}

void DebugLCD::_setCursor(int x, int y) const {
	uint8_t offset = x | ((y & 1) << 6);

	if (y & 2)
		offset += width;

	_writeByte(_LCD_CMD_SET_DDRAM_PTR | offset, true);
}

void DebugLCD::init(int _width, int _height) {
	width  = _width;
	height = _height;
	clear();

	// See http://elm-chan.org/docs/lcd/hd44780_e.html.
	for (int i = 3; i; i--) {
		_writeNibble(
			(_LCD_CMD_FUNCTION_SET | _LCD_CMD_FUNCTION_SET_BUS_8BIT) >> 4, true
		);
		delayMicroseconds(_LCD_INIT_DELAY);
	}

	_writeNibble(
		(_LCD_CMD_FUNCTION_SET | _LCD_CMD_FUNCTION_SET_BUS_4BIT) >> 4, true
	);
	_writeByte(_LCD_CMD_FUNCTION_SET | _LCD_CMD_FUNCTION_SET_ROWS_2, true);
	_writeByte(_LCD_CMD_DISPLAY_MODE | _LCD_CMD_DISPLAY_MODE_ON,     true);
	_writeByte(_LCD_CMD_ENTRY_MODE   | _LCD_CMD_ENTRY_MODE_INC,      true);

	_writeByte(_LCD_CMD_CLEAR, true);
	delayMicroseconds(_LCD_INIT_DELAY);
}

void DebugLCD::flush(void) const {
	for (int y = 0; y < height; y++) {
		auto ptr = buffer[y];

		_setCursor(0, y);

		for (int x = width; x; x--)
			_writeByte(*(ptr++));
	}

	auto cmd = _LCD_CMD_DISPLAY_MODE | _LCD_CMD_DISPLAY_MODE_ON;

	if ((cursorX >= 0) && (cursorY >= 0)) {
		cmd |= _LCD_CMD_DISPLAY_MODE_CURSOR | _LCD_CMD_DISPLAY_MODE_BLINK;
		_setCursor(cursorX, cursorY);
	}

	_writeByte(cmd, true);
}

void DebugLCD::put(int x, int y, util::UTF8CodePoint codePoint) {
	// The LCD's CGROM has some non-ASCII (JIS X 0201) characters, which must be
	// remapped manually.
	switch (codePoint) {
		case 0xa5: // Yen sign
			buffer[y][x] = 0x5c;
			break;

		case 0xb0: // Degree sign
			buffer[y][x] = 0xdf;
			break;

		case 0x2190: // Left arrow
			buffer[y][x] = 0x7f;
			break;

		case 0x2192: // Right arrow
			buffer[y][x] = 0x7e;
			break;

		case 0x2588: // Filled block
			buffer[y][x] = 0xff;
			break;

		default:
			if (codePoint < 0x80)
				buffer[y][x] = codePoint;
	}
}

size_t DebugLCD::print(int x, int y, const char *format, ...) {
	va_list ap;

	char   printBuffer[NUM_LCD_COLUMNS + 1];
	size_t length = 0;

	va_start(ap, format);
	vsnprintf(printBuffer, width - x + 1, format, ap);
	va_end(ap);

	for (auto ptr = printBuffer;; x++, length++) {
		auto ch = util::parseUTF8Character(ptr);
		ptr    += ch.length;

		if (!ch.codePoint)
			break;

		put(x, y, ch.codePoint);
	}

	return length;
}

DebugLCD debugLCD;

}
