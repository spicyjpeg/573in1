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
#include "common/util/string.hpp"

namespace exthw {

/* Pin and command definitions */

enum DebugLCDPin {
	LCD_PIN_D0 = 0,
	LCD_PIN_RS = 4,
	LCD_PIN_E  = 5
};

enum DebugLCDCommand : uint8_t {
	LCD_CLEAR = 1 << 0,
	LCD_HOME  = 1 << 1,

	LCD_ENTRY_MODE_SHIFT = 1 << 0,
	LCD_ENTRY_MODE_DEC   = 0 << 1,
	LCD_ENTRY_MODE_INC   = 1 << 1,
	LCD_ENTRY_MODE       = 1 << 2,

	LCD_DISPLAY_MODE_BLINK  = 1 << 0,
	LCD_DISPLAY_MODE_CURSOR = 1 << 1,
	LCD_DISPLAY_MODE_ON     = 1 << 2,
	LCD_DISPLAY_MODE        = 1 << 3,

	LCD_MOVE_LEFT    = 0 << 2,
	LCD_MOVE_RIGHT   = 1 << 2,
	LCD_MOVE_CURSOR  = 0 << 3,
	LCD_MOVE_DISPLAY = 1 << 3,
	LCD_MOVE         = 1 << 4,

	LCD_FUNCTION_SET_HEIGHT_8  = 0 << 2,
	LCD_FUNCTION_SET_HEIGHT_11 = 1 << 2,
	LCD_FUNCTION_SET_ROWS_1    = 0 << 3,
	LCD_FUNCTION_SET_ROWS_2    = 1 << 3,
	LCD_FUNCTION_SET_BUS_4BIT  = 0 << 4,
	LCD_FUNCTION_SET_BUS_8BIT  = 1 << 4,
	LCD_FUNCTION_SET           = 1 << 5,

	LCD_SET_CGRAM_PTR = 1 << 6,
	LCD_SET_DDRAM_PTR = 1 << 7
};

/* Debug LCD driver */

static constexpr size_t NUM_LCD_ROWS    = 4;
static constexpr size_t NUM_LCD_COLUMNS = 20;

class DebugLCD {
private:
	inline void _writeByte(uint8_t value, bool isCmd = false) const {
		_writeNibble(value >> 4, isCmd);
		_writeNibble(value & 15, isCmd);
	}

	void _writeNibble(uint8_t value, bool isCmd = false) const;
	void _setCursor(int x, int y) const;

public:
	uint8_t width, height;
	int8_t  cursorX, cursorY;
	uint8_t buffer[NUM_LCD_ROWS][NUM_LCD_COLUMNS];

	inline void clear(uint8_t fillCh = ' ') {
		cursorX = -1;
		cursorY = -1;

		__builtin_memset(buffer, fillCh, sizeof(buffer));
	}

	void init(int _width, int _height);
	void flush(void) const;

	void put(int x, int y, util::UTF8CodePoint codePoint);
	size_t print(int x, int y, const char *format, ...);
};

extern DebugLCD debugLCD;

}
