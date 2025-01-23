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

/* MAS3507D definitions */

enum MAS3507DMemoryOffset : uint16_t {
	// Is it 0x036f or 0x032f? The datasheet lists the former as the output
	// configuration register, however Konami's code uses the latter and so does
	// one of the command examples in the datasheet.
	MAS_D0_PLL_OFFSET_48 = 0x036d,
	MAS_D0_PLL_OFFSET_44 = 0x036e,
	MAS_D0_OUTPUT_CFG    = 0x036f,

	MAS_D1_VOLUME_LL = 0x07f8,
	MAS_D1_VOLUME_LR = 0x07f9,
	MAS_D1_VOLUME_RL = 0x07fa,
	MAS_D1_VOLUME_RR = 0x07fb,
	MAS_D1_MAGIC     = 0x0ff6,
	MAS_D1_VERSION   = 0x0ff7
};

enum MAS3507DRegister : uint8_t {
	MAS_REG_SDI_INIT    = 0x3b,
	MAS_REG_SDI_UNKNOWN = 0x4b,
	MAS_REG_SI1M0       = 0x4f,
	MAS_REG_KBASS       = 0x6b,
	MAS_REG_KTREBLE     = 0x6f,
	MAS_REG_DCCF        = 0x8e,
	MAS_REG_MUTE        = 0xaa,
	MAS_REG_SDO_LSB_L   = 0xc5,
	MAS_REG_SDO_LSB_R   = 0xc6,
	MAS_REG_PI19        = 0xc8,
	MAS_REG_STARTUP_CFG = 0xe6,
	MAS_REG_KPRESCALE   = 0xe7,
	MAS_REG_PIO_DATA    = 0xed
};

enum MAS3507DFunction : uint16_t {
	// Konami's driver uses 0x0fcb instead of 0x0475. It is currently unknown
	// whether this is a mistake in the code (or in the MAS3507D datasheet, see
	// above) or an actual, separate entry point.
	MAS_FUNC_INIT               = 0x0001,
	MAS_FUNC_UPDATE_OUTPUT_CFG  = 0x0475,
	MAS_FUNC_UPDATE_STARTUP_CFG = 0x0fcd
};

enum MAS3507DOutputConfigFlag : uint32_t {
	MAS_OUTPUT_CFG_SAMPLE_FMT_32   = 0 <<  4,
	MAS_OUTPUT_CFG_SAMPLE_FMT_16   = 1 <<  4,
	MAS_OUTPUT_CFG_INVERT_LRCK     = 1 <<  5,
	MAS_OUTPUT_CFG_LRCK_BEFORE_LSB = 1 << 11,
	MAS_OUTPUT_CFG_INVERT_BCLK     = 1 << 14
};

enum MAS3507DStartupConfigFlag : uint32_t {
	MAS_STARTUP_CFG_MODE_DATA_REQ  = 0 << 0,
	MAS_STARTUP_CFG_MODE_BROADCAST = 1 << 0,
	MAS_STARTUP_CFG_SAMPLE_FMT_32  = 0 << 1,
	MAS_STARTUP_CFG_SAMPLE_FMT_16  = 1 << 1,
	MAS_STARTUP_CFG_LAYER2         = 1 << 2,
	MAS_STARTUP_CFG_LAYER3         = 1 << 3,
	MAS_STARTUP_CFG_INPUT_SDI      = 0 << 4,
	MAS_STARTUP_CFG_INPUT_PIO      = 1 << 4,
	MAS_STARTUP_CFG_MCLK_DIVIDE    = 0 << 8,
	MAS_STARTUP_CFG_MCLK_FIXED     = 1 << 8
};

/* MAS3507D MP3 decoder driver */

class MAS3507DDriver {
private:
	const bus::I2CDriver &_i2c;

	bool _issueCommand(const uint8_t *data, size_t length) const;
	bool _issueRead(uint8_t *data, size_t length) const;

public:
	inline MAS3507DDriver(const bus::I2CDriver &i2c)
	: _i2c(i2c) {}

	int readFrameCount(void) const;
	int readMemory(int bank, uint16_t offset) const;
	bool writeMemory(int bank, uint16_t offset, int value) const;
	int readReg(uint8_t offset) const;
	bool writeReg(uint8_t offset, int value) const;
	bool runFunction(uint16_t func) const;
};

}
