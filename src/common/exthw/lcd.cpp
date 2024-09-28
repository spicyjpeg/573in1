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
#include "common/exthw/lcd.hpp"
#include "common/util/string.hpp"
#include "ps1/registers573.h"
#include "ps1/system.h"

/*
 * This is a simple driver for a character LCD module, wired to the EXT-OUT
 * connector on the 573 main board as follows:
 *
 * | EXT-OUT pin | LCD pin      |
 * | ----------: | :----------- |
 * |        1, 2 | `VCC`        |
 * |           5 | `E`          |
 * |           6 | `RS`         |
 * |           7 | `D7`         |
 * |           8 | `D6`         |
 * |           9 | `D5`         |
 * |          10 | `D4`         |
 * |      11, 12 | `GND`, `R/W` |
 *
 * The `V0` (bias voltage) pin shall be connected to ground through an
 * appropriate resistor or potentiometer in order to set the display's contrast.
 */

namespace exthw {

/* Debug LCD driver */

DebugLCD debugLCD;

static constexpr int _WRITE_PULSE_TIME = 1;
static constexpr int _WRITE_DELAY      = 50;
static constexpr int _INIT_DELAY       = 5000;

void DebugLCD::_writeNibble(uint8_t value, bool isCmd) const {
	uint8_t outputs = 0
		| ((value & 15) << LCD_PIN_D0)
		| ((isCmd ^ 1)  << LCD_PIN_RS);

	SYS573_EXT_OUT = outputs;
	delayMicroseconds(_WRITE_PULSE_TIME);
	SYS573_EXT_OUT = outputs | (1 << LCD_PIN_E);
	delayMicroseconds(_WRITE_PULSE_TIME);

	SYS573_EXT_OUT = outputs;
	delayMicroseconds(_WRITE_DELAY);
}

void DebugLCD::_setCursor(int x, int y) const {
	uint8_t offset = x | ((y & 1) << 6);

	if (y & 2)
		offset += width;

	_writeByte(LCD_SET_DDRAM_PTR | offset, true);
}

void DebugLCD::init(int _width, int _height) {
	width  = _width;
	height = _height;
	clear();

	// See http://elm-chan.org/docs/lcd/hd44780_e.html.
	for (int i = 3; i; i--) {
		_writeNibble((LCD_FUNCTION_SET | LCD_FUNCTION_SET_BUS_8BIT) >> 4, true);
		delayMicroseconds(_INIT_DELAY);
	}

	_writeNibble((LCD_FUNCTION_SET | LCD_FUNCTION_SET_BUS_4BIT) >> 4, true);

	_writeByte(LCD_FUNCTION_SET | LCD_FUNCTION_SET_ROWS_2, true);
	_writeByte(LCD_DISPLAY_MODE | LCD_DISPLAY_MODE_ON,     true);
	_writeByte(LCD_ENTRY_MODE   | LCD_ENTRY_MODE_INC,      true);

	_writeByte(LCD_CLEAR, true);
	delayMicroseconds(_INIT_DELAY);
}

void DebugLCD::flush(void) const {
	for (int y = 0; y < height; y++) {
		auto ptr = buffer[y];

		_setCursor(0, y);

		for (int x = width; x; x--)
			_writeByte(*(ptr++));
	}

	auto cmd = LCD_DISPLAY_MODE | LCD_DISPLAY_MODE_ON;

	if ((cursorX >= 0) && (cursorY >= 0)) {
		cmd |= LCD_DISPLAY_MODE_CURSOR | LCD_DISPLAY_MODE_BLINK;
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

}
