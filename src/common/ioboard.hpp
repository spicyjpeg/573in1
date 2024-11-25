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
#include "common/io.hpp"
#include "ps1/registers573.h"

namespace io {

/* Light output control API */

void setIOBoardLights(uint32_t bits);

/* Digital I/O board initialization */

static inline bool isDigitalIOPresent(void) {
	const uint16_t mask = 0
		| SYS573D_CPLD_INIT_STAT_ID1
		| SYS573D_CPLD_INIT_STAT_ID2;

	return ((SYS573D_CPLD_INIT_STAT & mask) == SYS573D_CPLD_INIT_STAT_ID2);
}

static inline bool isDigitalIOReady(void) {
	uint16_t magic = SYS573D_FPGA_MAGIC;

	return 0
		|| (magic == SYS573D_FPGA_MAGIC_KONAMI)
		|| (magic == SYS573D_FPGA_MAGIC_573IN1);
}

bool digitalIOLoadBitstream(const uint8_t *data, size_t length);
bool digitalIOLoadRawBitstream(const uint8_t *data, size_t length);
void digitalIOFPGAInit(void);

/* Digital I/O board bus APIs */

class DigitalIOI2CDriver : public I2CDriver {
private:
	bool _getSDA(void) const;
	void _setSDA(bool value) const;
	void _setSCL(bool value) const;
};

class DigitalIODS2401Driver : public OneWireDriver {
private:
	bool _get(void) const;
	void _set(bool value) const;
};

class DigitalIODS2433Driver : public OneWireDriver {
private:
	bool _get(void) const;
	void _set(bool value) const;
};

extern const DigitalIOI2CDriver    digitalIOI2C;
extern const DigitalIODS2401Driver digitalIODS2401;
extern const DigitalIODS2433Driver digitalIODS2433;

/* Digital I/O MP3 decoder driver */

enum DigitalIOMP3MemoryOffset : uint16_t {
	// Is it 0x036f or 0x032f? The datasheet lists the former as the output
	// configuration register, however Konami's code uses the latter and so does
	// one of the command examples in the datasheet.
	MP3_D0_PLL_OFFSET_48 = 0x036d,
	MP3_D0_PLL_OFFSET_44 = 0x036e,
	MP3_D0_OUTPUT_CFG    = 0x036f,

	MP3_D1_VOLUME_LL = 0x07f8,
	MP3_D1_VOLUME_LR = 0x07f9,
	MP3_D1_VOLUME_RL = 0x07fa,
	MP3_D1_VOLUME_RR = 0x07fb,
	MP3_D1_MAGIC     = 0x0ff6,
	MP3_D1_VERSION   = 0x0ff7
};

enum DigitalIOMP3Register : uint8_t {
	MP3_REG_SDI_INIT    = 0x3b,
	MP3_REG_SDI_UNKNOWN = 0x4b,
	MP3_REG_SI1M0       = 0x4f,
	MP3_REG_KBASS       = 0x6b,
	MP3_REG_KTREBLE     = 0x6f,
	MP3_REG_DCCF        = 0x8e,
	MP3_REG_MUTE        = 0xaa,
	MP3_REG_SDO_LSB_L   = 0xc5,
	MP3_REG_SDO_LSB_R   = 0xc6,
	MP3_REG_PI19        = 0xc8,
	MP3_REG_STARTUP_CFG = 0xe6,
	MP3_REG_KPRESCALE   = 0xe7,
	MP3_REG_PIO_DATA    = 0xed
};

enum DigitalIOMP3Function : uint16_t {
	// Konami's driver uses 0x0fcb instead of 0x0475. It is currently unknown
	// whether this is a mistake in the code (or in the MAS3507D datasheet, see
	// above) or an actual, separate entry point.
	MP3_FUNC_INIT               = 0x0001,
	MP3_FUNC_UPDATE_OUTPUT_CFG  = 0x0475,
	MP3_FUNC_UPDATE_STARTUP_CFG = 0x0fcd
};

enum DigitalIOMP3OutputConfigFlag : uint32_t {
	MP3_OUTPUT_CFG_SAMPLE_FMT_32   = 0 <<  4,
	MP3_OUTPUT_CFG_SAMPLE_FMT_16   = 1 <<  4,
	MP3_OUTPUT_CFG_INVERT_LRCK     = 1 <<  5,
	MP3_OUTPUT_CFG_LRCK_BEFORE_LSB = 1 << 11,
	MP3_OUTPUT_CFG_INVERT_BCLK     = 1 << 14
};

enum DigitalIOMP3StartupConfigFlag : uint32_t {
	MP3_STARTUP_CFG_MODE_DATA_REQ  = 0 << 0,
	MP3_STARTUP_CFG_MODE_BROADCAST = 1 << 0,
	MP3_STARTUP_CFG_SAMPLE_FMT_32  = 0 << 1,
	MP3_STARTUP_CFG_SAMPLE_FMT_16  = 1 << 1,
	MP3_STARTUP_CFG_LAYER2         = 1 << 2,
	MP3_STARTUP_CFG_LAYER3         = 1 << 3,
	MP3_STARTUP_CFG_INPUT_SDI      = 0 << 4,
	MP3_STARTUP_CFG_INPUT_PIO      = 1 << 4,
	MP3_STARTUP_CFG_MCLK_DIVIDE    = 0 << 8,
	MP3_STARTUP_CFG_MCLK_FIXED     = 1 << 8
};

bool digitalIOMP3Init(void);
int digitalIOMP3ReadFrameCount(void);
int digitalIOMP3ReadMem(int bank, uint16_t offset);
bool digitalIOMP3WriteMem(int bank, uint16_t offset, int value);
int digitalIOMP3ReadReg(uint8_t offset);
bool digitalIOMP3WriteReg(uint8_t offset, int value);
bool digitalIOMP3Run(uint16_t func);

}
