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

#include <stdint.h>
#include "common/sys573/digitalio.hpp"
#include "common/sys573/ioboard.hpp"
#include "ps1/registers573.h"

namespace sys573 {

/* Analog I/O board class */

static constexpr uint32_t _ANALOG_IO_LIGHT_ORDER1 = 0x02467531;
static constexpr uint32_t _ANALOG_IO_LIGHT_ORDER2 = 0x0123;

AnalogIOBoardDriver::AnalogIOBoardDriver(void) {
	type = IO_ANALOG;
}

void AnalogIOBoardDriver::setLightOutputs(uint32_t bits) const {
	bits = ~bits;

	// Due to how traces are routed on the analog I/O PCB, the first 3 banks'
	// bit order is scrambled and must be changed from 7-6-5-4-3-2-1-0 to
	// 0-2-4-6-7-5-3-1.
	uint32_t reordered1 = 0;
	uint32_t order      = _ANALOG_IO_LIGHT_ORDER1;

	for (int i = 8; i > 0; i--) {
		reordered1 |= (bits & 0x010101) << (order & 15);

		bits  >>= 1;
		order >>= 4;
	}

	bits >>= 16;

	// The last bank's bit order is reversed from 3-2-1-0 to 0-1-2-3.
	uint32_t reordered2 = 0;
	order               = _ANALOG_IO_LIGHT_ORDER2;

	for (int i = 4; i > 0; i--) {
		reordered2 |= (bits & 1) << (order & 15);

		bits  >>= 1;
		order >>= 4;
	}

	SYS573A_LIGHTS_A = (reordered1 >>  0) & 0xff;
	SYS573A_LIGHTS_B = (reordered1 >>  8) & 0xff;
	SYS573A_LIGHTS_C = (reordered1 >> 16) & 0xff;
	SYS573A_LIGHTS_D = (reordered2 >>  0);
}

/* Kick & Kick I/O board class */

class KickIODS2401Driver : public bus::OneWireDriver {
private:
	bool _get(void) const {
		return (SYS573KK_MISC_IN / SYS573KK_MISC_IN_DS2401) & 1;
	}
	void _set(bool value) const {
		SYS573KK_DS2401_OUT = ((value ^ 1) & 1) << 15;
	}
};

static const KickIODS2401Driver _kickIODS2401;

KickIOBoardDriver::KickIOBoardDriver(void) {
	type   = IO_KICK;
	ds2401 = &_kickIODS2401;
}

/* Fishing reel I/O board class */

FishingReelIOBoardDriver::FishingReelIOBoardDriver(void) {
	type = IO_FISHING_REEL;
}

/* DDR Karaoke Mix I/O board class */

class KaraokeIODS2401Driver : public bus::OneWireDriver {
private:
	bool _get(void) const {
		return SYS573DK_DS2401 & 1;
	}
	void _set(bool value) const {
		SYS573DK_DS2401 = value & 1;
	}
};

static const KaraokeIODS2401Driver _karaokeIODS2401;

KaraokeIOBoardDriver::KaraokeIOBoardDriver(void) {
	type   = IO_DDR_KARAOKE;
	ds2401 = &_karaokeIODS2401;
}

/* GunMania I/O board class */

class GunManiaIODS2401Driver : public bus::OneWireDriver {
private:
	bool _get(void) const {
		return (SYS573G_MATRIX_X >> 7) & 1;
	}
	void _set(bool value) const {
		SYS573G_DS2401_OUT = (value & 1) << 5;
	}
};

static const GunManiaIODS2401Driver _gunManiaIODS2401;

GunManiaIOBoardDriver::GunManiaIOBoardDriver(void) {
	type   = IO_GUNMANIA;
	ds2401 = &_gunManiaIODS2401;
}

/* I/O board detection and constructor */

IOBoardDriver *newIOBoardDriver(void) {
	// The digital I/O board can be detected by checking the CPLD status
	// register. This will work even if no bitstream is loaded in the FPGA.
	uint16_t mask  = 0
		| SYS573D_CPLD_INIT_STAT_ID1
		| SYS573D_CPLD_INIT_STAT_ID2;
	uint16_t value = SYS573D_CPLD_INIT_STAT_ID2;

	if ((SYS573D_CPLD_INIT_STAT & mask) == value)
		return new DigitalIOBoardDriver();

	// The fishing reel board may be detected by resetting and probing its
	// rotary encoder interface chip (NEC uPD4701). The chip has three "button"
	// inputs, two of which are hardwired to ground.
	value = 0
		| SYS573F_ENCODER_H_UNUSED1
		| SYS573F_ENCODER_H_UNUSED2
		| SYS573F_ENCODER_H_SWITCH_FLAG;
	mask  = value
		| (15 << 0);

	SYS573F_ENCODER_RESET = 0;

	if (
		((SYS573F_ENCODER_XH & mask) == value) &&
		((SYS573F_ENCODER_YH & mask) == value)
	)
		return new FishingReelIOBoardDriver();

	// Other boards can be detected by attempting to initialize their DS2401s.
	if (_kickIODS2401.reset())
		return new KickIOBoardDriver();
	if (_karaokeIODS2401.reset())
		return new KaraokeIOBoardDriver();
	if (_gunManiaIODS2401.reset())
		return new GunManiaIOBoardDriver();

	// There is no way to detect the presence of an analog I/O board as its
	// registers are write-only. However it is safe to assume one is present (if
	// not, writes to the light outputs will simply go nowhere).
	return new AnalogIOBoardDriver();
}

}
