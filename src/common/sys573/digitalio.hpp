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
#include "common/sys573/ioboard.hpp"

namespace sys573 {

/* Digital I/O board class */

class DigitalIOBoardDriver : public IOBoardDriver {
private:
	bool _loadRawBitstream(const uint8_t *data, size_t length) const;
	void _initFPGA(void) const;
	bool _initMP3(void) const;

public:
	DigitalIOBoardDriver(void);

	bool isReady(void) const;
	bool loadBitstream(const uint8_t *data, size_t length);

	void setLightOutputs(uint32_t bits) const;
	void readExtMemory(void *data, uint32_t offset, size_t length);
	void writeExtMemory(const void *data, uint32_t offset, size_t length);
};

}
