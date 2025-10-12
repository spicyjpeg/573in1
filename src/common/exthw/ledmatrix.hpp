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
#include "common/util/templates.hpp"

namespace exthw {

/* Pin and color definitions */

enum LEDMatrixPin {
	LED_PIN_AE  = 0,
	LED_PIN_WE  = 1,
	LED_PIN_CLK = 2,
	LED_PIN_RD  = 3,
	LED_PIN_A0  = 3,
	LED_PIN_ABB = 7
};

enum LEDMatrixColor : uint8_t {
	LED_BLACK  = 0,
	LED_RED    = 1,
	LED_GREEN  = 2,
	LED_YELLOW = 3
};

/* LED dot matrix driver */

static constexpr size_t NUM_LED_ROWS    = 16;
static constexpr size_t NUM_LED_COLUMNS = 64; // Must be a multiple of 4

class LEDMatrix {
private:
	int _currentBuffer;

public:
	uint16_t width, height;
	uint8_t  buffer[NUM_LED_ROWS][NUM_LED_COLUMNS];

	inline void clear(LEDMatrixColor color = LED_BLACK) {
		util::clear(buffer, color);
	}

	LEDMatrix(void);
	void init(int w = NUM_LED_COLUMNS, int h = NUM_LED_ROWS);
	void flush(void);

	void drawRect(int x, int y, int w, int h, LEDMatrixColor color);
};

extern LEDMatrix ledMatrix;

/* Image class */

class LEDImageHeader {
public:
	uint32_t magic[2];
	uint16_t width, height;

	inline bool validateMagic(void) const {
		return (magic[0] == "573l"_c) && (magic[1] == "edim"_c);
	}

	inline const void *getData(void) const {
		return this + 1;
	}
};

}
