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
#include "common/bus.hpp"

namespace sys573 {

/* Base I/O board class */

enum IOBoardType {
	IO_NONE         = 0,
	IO_ANALOG       = 1,
	IO_KICK         = 2,
	IO_FISHING_REEL = 3,
	IO_DIGITAL      = 4,
	IO_DDR_KARAOKE  = 5,
	IO_GUNMANIA     = 6
};

class IOBoardDriver {
public:
	IOBoardType type;
	size_t      extMemoryLength;

	const bus::OneWireDriver *ds2401, *ds2433;
	const bus::UARTDriver    *serial[2];

	inline IOBoardDriver(void)
	:
		type(IO_NONE),
		extMemoryLength(0),
		ds2401(nullptr),
		ds2433(nullptr)
	{
		serial[0] = nullptr;
		serial[1] = nullptr;
	}

	virtual bool isReady(void) const { return true; }
	virtual bool loadBitstream(const uint8_t *data, size_t length) {
		return false;
	}

	virtual void setLightOutputs(uint32_t bits) const {}
	virtual void readExtMemory(void *data, uint32_t offset, size_t length) {}
	virtual void writeExtMemory(
		const void *data, uint32_t offset, size_t length
	) {}
};

/* I/O board classes */

class AnalogIOBoardDriver : public IOBoardDriver {
public:
	AnalogIOBoardDriver(void);

	void setLightOutputs(uint32_t bits) const;
};

class KickIOBoardDriver : public IOBoardDriver {
public:
	KickIOBoardDriver(void);
};

class FishingReelIOBoardDriver : public IOBoardDriver {
public:
	FishingReelIOBoardDriver(void);
};

class KaraokeIOBoardDriver : public IOBoardDriver {
public:
	KaraokeIOBoardDriver(void);
};

class GunManiaIOBoardDriver : public IOBoardDriver {
public:
	GunManiaIOBoardDriver(void);
};

IOBoardDriver *newIOBoardDriver(void);

}
