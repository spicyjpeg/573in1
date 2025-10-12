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

#include <assert.h>
#include <stdint.h>
#include "common/exthw/ledmatrix.hpp"
#include "ps1/registers573.h"

/*
 * Driver and basic software renderer for the 64x16x2bpp (red/green) LED dot
 * matrix marquee used by Punch Mania/Fighting Mania. The marquee consists of
 * two daisy chained Rohm LUM-512HML350 32x16 panels, connected to the EXT-OUT
 * (CN4) header on the 573 main board as follows:
 *
 * | EXT-OUT pin | EXT-OUT signal | Panel pin | Panel signal    |
 * | ----------: | :------------- | --------: | :-------------- |
 * |        1, 2 | `5V`           |     1, 13 | `SEin`, `ENBin` |
 * |           3 | `OUT7`         |         2 | `A/BBin`        |
 * |           4 | `OUT6`         |         3 | `A3in`          |
 * |           5 | `OUT5`         |         4 | `A2in`          |
 * |           6 | `OUT4`         |      5, 8 | `A1in`, `GRin`  |
 * |           7 | `OUT3`         |     6, 11 | `A0in`, `RDin`  |
 * |           8 | `OUT2`         |         9 | `CLKin`         |
 * |           9 | `OUT1`         |        10 | `WEin`          |
 * |          10 | `OUT0`         |        12 | `AEin`          |
 * |      11, 12 | `GND`          |         7 | `GND`           |
 *
 * Each panel has two built-in framebuffers and will self refresh while new data
 * is being written into the inactive buffer. In manual switching mode (SE tied
 * high) the A/BB pin is used to flip buffers. A0-A3 are used once 64 pixels
 * have been shifted in to select which row to latch them into.
 *
 * Punch Mania generates the following waveforms to send each row (RD-GR and
 * A0-A3 are tied together but shown separately here for clarity) before
 * toggling A/BB:
 *
 *       | Row 1                                                | Row 2   ...
 *        _____ _____ _       _ _____ __________________________ _____ _
 * RD-GR <__0__X__1__X_  ...  _X_63__X__________________________X__0__X_  ...
 *           __    __         _    _____       ________             __
 * CLK   ___|  |__|  |_  ...   |__|     |_____|        |___________|  |_  ...
 *       ______________       _______ __________________________ _______
 * A0-A3 ______________  ...  _______X________Row index_________X_______  ...
 *                                          ______________
 * AE    ______________  ...  _____________|              |_____________  ...
 *                                                __
 * WE    ______________  ...  ___________________|  |___________________  ...
 *
 * More information on pinouts and signal timings can be found here:
 * https://bake-san.com/16x16led.htm
 */

namespace exthw {

/* LED dot matrix driver */

LEDMatrix ledMatrix;

LEDMatrix::LEDMatrix(void) :
	_currentBuffer(0),
	width(0),
	height(0)
{}

void LEDMatrix::init(int w, int h) {
	assert(!(w % 4));

	width  = w;
	height = h;
	clear();

	_currentBuffer = 0;
	flush();
}

void LEDMatrix::flush(void) {
	for (int y = 0; y < height; y++) {
		// Read the buffer 4 pixels at a time.
		auto ptr = reinterpret_cast<const uint32_t *>(buffer[y]);

		for (int x = width; x > 0; x -= 4) {
			auto values = *(ptr++);

			for (int i = 4; i > 0; i--, values >>= 8) {
				uint16_t outputs = 0
					| ((values & 15)  << LED_PIN_RD)
					| (_currentBuffer << LED_PIN_ABB);

				SYS573_EXT_OUT = outputs;
				SYS573_EXT_OUT = outputs | (1 << LED_PIN_CLK);
			}
		}

		// FIXME: this may have to be slowed down (needs testing)
		uint16_t outputs = 0
			| ((y & 15)       << LED_PIN_RD)
			| (_currentBuffer << LED_PIN_ABB);

		SYS573_EXT_OUT = outputs;
		SYS573_EXT_OUT = outputs | (1 << LED_PIN_AE);
		SYS573_EXT_OUT = outputs | (1 << LED_PIN_AE) | (1 << LED_PIN_WE);
		SYS573_EXT_OUT = outputs | (1 << LED_PIN_AE);
		SYS573_EXT_OUT = outputs;
	}

	_currentBuffer ^= 1;
	SYS573_EXT_OUT  = _currentBuffer << LED_PIN_ABB;
}

void LEDMatrix::drawRect(int x, int y, int w, int h, LEDMatrixColor color) {
	for (; h > 0; h--, y++)
		__builtin_memset(&buffer[y][x], color, w);
}

}
