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

#include <stddef.h>
#include <stdint.h>
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/ioboard.hpp"
#include "ioboard.hpp"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace io {

static uint16_t _digitalIOI2CReg, _digitalIODSBusReg;

/* Light output control API */

void setIOBoardLights(uint32_t bits) {
	bits = ~bits;

	if (isDigitalIOPresent()) {
		SYS573D_FPGA_LIGHTS_AL = (bits >>  0) << 12;
		SYS573D_FPGA_LIGHTS_AH = (bits >>  4) << 12;
		SYS573D_CPLD_LIGHTS_BL = (bits >>  8) << 12;
		SYS573D_FPGA_LIGHTS_BH = (bits >> 12) << 12;
		SYS573D_CPLD_LIGHTS_CL = (bits >> 16) << 12;
		SYS573D_CPLD_LIGHTS_CH = (bits >> 20) << 12;
		SYS573D_FPGA_LIGHTS_D  = (bits >> 24) << 12;
	} else {
		// Due to how traces are routed on the analog I/O PCB, the bit order of
		// each bank is scrambled and must be changed from 7-6-5-4-3-2-1-0 to
		// 0-2-4-6-7-5-3-1.
		uint32_t order     = 0x02467531;
		uint32_t reordered = 0;

		for (int i = 8; i; i--) {
			reordered |= (bits & 0x01010101) << (order & 15);

			bits  >>= 1;
			order >>= 4;
		}

		SYS573A_LIGHTS_A = (reordered >>  0) & 0xff;
		SYS573A_LIGHTS_B = (reordered >>  8) & 0xff;
		SYS573A_LIGHTS_C = (reordered >> 16) & 0xff;
		SYS573A_LIGHTS_D = (reordered >> 24) & 0xff;
	}
}

/* Digital I/O board FPGA initialization */

static constexpr int _FPGA_PROGRAM_DELAY   = 5000;
static constexpr int _FPGA_STARTUP_DELAY   = 50000;
static constexpr int _FPGA_RESET_REG_DELAY = 500;

enum BitstreamTagType : uint8_t {
	_TAG_SOURCE_FILE = 'a',
	_TAG_PART_NAME   = 'b',
	_TAG_BUILD_DATE  = 'c',
	_TAG_BUILD_TIME  = 'd',
	_TAG_DATA        = 'e'
};

static void _writeBitstreamLSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++);

		for (int i = 8; i; i--, bits >>= 1)
			SYS573D_CPLD_BITSTREAM = (bits & 1) << 15;
	}
}

static void _writeBitstreamMSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++) << 8;

		for (int i = 8; i; i--, bits <<= 1)
			SYS573D_CPLD_BITSTREAM = bits & (1 << 15);
	}
}

bool loadDigitalIOBitstream(const uint8_t *data, size_t length) {
	// Konami's bitstreams are always stored LSB-first and with no headers,
	// however Xilinx tools export .bit files which contain MSB-first bitstreams
	// wrapped in a TLV container. In order to upload the bitstream properly,
	// the bit order and presence of a header must be autodetected. See
	// https://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm and
	// the "Data Stream Format" section in the XCS40XL datasheet for details.
	if (data[0] == 0xff)
		return loadDigitalIORawBitstream(data, length);

	auto     dataEnd      = &data[length];
	uint16_t headerLength = util::concat2(data[1], data[0]);

	data += headerLength + 4;

	while (data < dataEnd) {
		size_t tagLength;

		switch (data[0]) {
			case _TAG_DATA:
				tagLength = util::concat4(data[4], data[3], data[2], data[1]);
				data     += 5;

				return loadDigitalIORawBitstream(data, tagLength);

			default:
				tagLength = util::concat2(data[2], data[1]);
				data     += 3;
		}

		data += tagLength;
	}

	LOG_IO("no data tag found");
	return false;
}

bool loadDigitalIORawBitstream(const uint8_t *data, size_t length) {
	if (data[0] != 0xff) {
		LOG_IO("invalid sync byte: 0x%02x", data[0]);
		return false;
	}

	uint8_t id1 = data[1], id2 = data[4];
	void    (*writeFunc)(const uint8_t *, size_t);

	if (((id1 & 0xf0) == 0x20) && ((id2 & 0x0f) == 0x0f)) {
		writeFunc = &_writeBitstreamMSB;
	} else if (((id1 & 0x0f) == 0x04) && ((id2 & 0xf0) == 0xf0)) {
		writeFunc = &_writeBitstreamLSB;
	} else {
		LOG_IO("could not detect bit order");
		return false;
	}

	const uint16_t mask = 0
		| SYS573D_CPLD_INIT_STAT_INIT
		| SYS573D_CPLD_INIT_STAT_DONE;

	for (int i = 3; i; i--) {
		SYS573D_CPLD_DAC_RESET = 0;

		SYS573D_CPLD_INIT_CTRL = SYS573D_CPLD_INIT_CTRL_UNKNOWN;
		SYS573D_CPLD_INIT_CTRL = 0
			| SYS573D_CPLD_INIT_CTRL_PROGRAM
			| SYS573D_CPLD_INIT_CTRL_UNKNOWN;
		SYS573D_CPLD_INIT_CTRL = 0
			| SYS573D_CPLD_INIT_CTRL_INIT
			| SYS573D_CPLD_INIT_CTRL_DONE
			| SYS573D_CPLD_INIT_CTRL_PROGRAM
			| SYS573D_CPLD_INIT_CTRL_UNKNOWN;
		delayMicroseconds(_FPGA_PROGRAM_DELAY);

		uint16_t status = SYS573D_CPLD_INIT_STAT;

		if ((status & mask) != SYS573D_CPLD_INIT_STAT_INIT) {
			LOG_IO("reset failed, st=0x%04x", status);
			continue;
		}

		writeFunc(data, length);
		delayMicroseconds(_FPGA_STARTUP_DELAY);

		status = SYS573D_CPLD_INIT_STAT;

		if ((status & mask) != mask) {
			LOG_IO("upload failed, st=0x%04x", status);
			continue;
		}

		return true;
	}

	LOG_IO("too many attempts failed");
	return false;
}

void initDigitalIOFPGA(void) {
	SYS573D_FPGA_RESET = 0xf000;
	SYS573D_FPGA_RESET = 0x0000;
	delayMicroseconds(_FPGA_RESET_REG_DELAY);

	SYS573D_FPGA_RESET = 0xf000;
	delayMicroseconds(_FPGA_RESET_REG_DELAY);

	// Some of the digital I/O board's light outputs are controlled by the FPGA
	// and cannot be turned off until the FPGA is initialized.
	setIOBoardLights(0);

	_digitalIOI2CReg   = 0
		| SYS573D_FPGA_MP3_I2C_SDA
		| SYS573D_FPGA_MP3_I2C_SCL;
	_digitalIODSBusReg = 0
		| SYS573D_FPGA_DS_BUS_DS2401
		| SYS573D_FPGA_DS_BUS_DS2433;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
	SYS573D_FPGA_DS_BUS  = _digitalIODSBusReg;
}

/* Digital I/O board bus APIs */

bool DigitalIOI2CDriver::_getSDA(void) const {
	return (SYS573D_FPGA_MP3_I2C / SYS573D_FPGA_MP3_I2C_SDA) & 1;
}

void DigitalIOI2CDriver::_setSDA(bool value) const {
	if (value)
		_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SDA;
	else
		_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SCL;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
}

void DigitalIOI2CDriver::_setSCL(bool value) const {
	if (value)
		_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SDA;
	else
		_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SCL;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;

	// The MAS3507D makes extensive use of clock stretching as part of its
	// protocol, so waiting until it deasserts SCL is needed here.
	while ((SYS573D_FPGA_MP3_I2C ^ _digitalIOI2CReg) & SYS573D_FPGA_MP3_I2C_SCL)
		__asm__ volatile("");
}

bool DigitalIODS2401Driver::_get(void) const {
	return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2401) & 1;
}

void DigitalIODS2401Driver::_set(bool value) const {
	if (value)
		_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2401;
	else
		_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2401;

	SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
}

bool DigitalIODS2433Driver::_get(void) const {
	return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2433) & 1;
}

void DigitalIODS2433Driver::_set(bool value) const {
	if (value)
		_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2433;
	else
		_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2433;

	SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
}

const DigitalIOI2CDriver    digitalIOI2C;
const DigitalIODS2401Driver digitalIODS2401;
const DigitalIODS2433Driver digitalIODS2433;

/* Digital I/O MP3 decoder driver */

enum MAS3507DPacketType : uint8_t {
	_MAS3507D_COMMAND = 0x68, // Called "write" in the datasheet
	_MAS3507D_READ    = 0x69,
	_MAS3507D_RESET   = 0x6a  // Called "control" in the datasheet
};

enum MAS3507DCommand : uint8_t {
	_MAS3507D_CMD_RUN         = 0x0 << 4,
	_MAS3507D_CMD_READ_STATUS = 0x3 << 4,
	_MAS3507D_CMD_WRITE_REG   = 0x9 << 4,
	_MAS3507D_CMD_WRITE_D0    = 0xa << 4,
	_MAS3507D_CMD_WRITE_D1    = 0xb << 4,
	_MAS3507D_CMD_READ_REG    = 0xd << 4,
	_MAS3507D_CMD_READ_D0     = 0xe << 4,
	_MAS3507D_CMD_READ_D1     = 0xf << 4
};

static constexpr uint8_t _MAS3507D_I2C_ADDR = 0x1d;

static constexpr int _MAS3507D_RESET_ASSERT_DELAY = 500;
static constexpr int _MAS3507D_RESET_CLEAR_DELAY  = 5000;

static bool _mas3507dCommand(const uint8_t *data, size_t length) {
	if (!digitalIOI2C.startDeviceWrite(_MAS3507D_I2C_ADDR)) {
		digitalIOI2C.stop();
		LOG_IO("chip not responding");
		return false;
	}

	digitalIOI2C.writeByte(_MAS3507D_COMMAND);
	if (!digitalIOI2C.getACK()) {
		digitalIOI2C.stop();
		LOG_IO("NACK while sending type");
		return false;
	}

	if (!digitalIOI2C.writeBytes(data, length)) {
		digitalIOI2C.stop();
		LOG_IO("NACK while sending data");
		return false;
	}

	digitalIOI2C.stop();
	return true;
}

static bool _mas3507dRead(uint8_t *data, size_t length) {
	// Due to the MAS3507D's weird I2C protocol layering, reads are performed by
	// first wrapping a read request into a "write" packet, then starting a new
	// read packet and actually reading the data.
	if (!digitalIOI2C.startDeviceWrite(_MAS3507D_I2C_ADDR)) {
		digitalIOI2C.stop();
		LOG_IO("chip not responding");
		return false;
	}

	digitalIOI2C.writeByte(_MAS3507D_READ);
	if (!digitalIOI2C.getACK()) {
		digitalIOI2C.stop();
		LOG_IO("NACK while sending type");
		return false;
	}

	if (!digitalIOI2C.startDeviceRead(_MAS3507D_I2C_ADDR)) {
		digitalIOI2C.stop();
		LOG_IO("chip not responding");
		return false;
	}

	digitalIOI2C.readBytes(data, length);
	digitalIOI2C.sendACK(false);
	digitalIOI2C.stop();
	return true;
}

bool digitalIOMP3Init(void) {
	// Turn off the DAC during initialization to prevent any audible popping.
	SYS573D_CPLD_DAC_RESET = 0;

	SYS573D_FPGA_MP3_CHIP_CTRL = SYS573D_FPGA_MP3_CHIP_CTRL_STATUS_CS;
	delayMicroseconds(_MAS3507D_RESET_ASSERT_DELAY);
	SYS573D_FPGA_MP3_CHIP_CTRL = SYS573D_FPGA_MP3_CHIP_CTRL_RESET;
	delayMicroseconds(_MAS3507D_RESET_CLEAR_DELAY);

	const auto startupCfg = 0
		| MP3_STARTUP_CFG_MODE_DATA_REQ
		| MP3_STARTUP_CFG_SAMPLE_FMT_16
		| MP3_STARTUP_CFG_LAYER2
		| MP3_STARTUP_CFG_LAYER3
		| MP3_STARTUP_CFG_INPUT_SDI
		| MP3_STARTUP_CFG_MCLK_DIVIDE;

	if (!digitalIOMP3WriteReg(MP3_REG_STARTUP_CFG, startupCfg))
		return false;
	if (!digitalIOMP3Run(MP3_FUNC_UPDATE_STARTUP_CFG))
		return false;

	// The AK4309 DAC does not use the standard I2S protocol and instead
	// requires a 16- or 32-bit LSB justified stream, so the default output mode
	// has to be adjusted accordingly.
	const auto outputCfg = 0
		| MP3_OUTPUT_CFG_SAMPLE_FMT_16
		| MP3_OUTPUT_CFG_INVERT_LRCK;

	if (!digitalIOMP3WriteMem(0, MP3_D0_OUTPUT_CFG, outputCfg))
		return false;
	if (!digitalIOMP3Run(MP3_FUNC_UPDATE_OUTPUT_CFG))
		return false;

	SYS573D_CPLD_DAC_RESET = 1 << 15;
	return true;
}

int digitalIOMP3ReadFrameCount(void) {
	uint8_t response[2];

	if (!_mas3507dRead(response, sizeof(response)))
		return -1;

	return util::concat2(response[1], response[0]);
}

int digitalIOMP3ReadMem(int bank, uint16_t offset) {
	uint8_t packet[6]{
		bank ? _MAS3507D_CMD_READ_D1 : _MAS3507D_CMD_READ_D0,
		0,
		0,
		1,
		uint8_t((offset >>  8) & 0xff),
		uint8_t((offset >>  0) & 0xff)
	};
	uint8_t response[4];

	if (!_mas3507dCommand(packet, sizeof(packet)))
		return -1;
	if (!_mas3507dRead(response, sizeof(response)))
		return -1;

	return util::concat4(response[1], response[0], response[3] & 0x0f, 0);
}

bool digitalIOMP3WriteMem(int bank, uint16_t offset, int value) {
	uint8_t packet[10]{
		bank ? _MAS3507D_CMD_WRITE_D1 : _MAS3507D_CMD_WRITE_D0,
		0,
		0,
		1,
		uint8_t((offset >>  8) & 0xff),
		uint8_t((offset >>  0) & 0xff),
		uint8_t((value  >>  8) & 0xff),
		uint8_t((value  >>  0) & 0xff),
		0,
		uint8_t((value  >> 16) & 0x0f)
	};

	return _mas3507dCommand(packet, sizeof(packet));
}

int digitalIOMP3ReadReg(uint8_t offset) {
	uint8_t packet[2]{
		uint8_t(((offset >> 4) & 0x0f) | _MAS3507D_CMD_READ_REG),
		uint8_t( (offset << 4) & 0xf0)
	};
	uint8_t response[4];

	if (!_mas3507dCommand(packet, sizeof(packet)))
		return -1;
	if (!_mas3507dRead(response, sizeof(response)))
		return -1;

	return util::concat4(response[1], response[0], response[3] & 0x0f, 0);
}

bool digitalIOMP3WriteReg(uint8_t offset, int value) {
	uint8_t packet[4]{
		uint8_t(((offset >>  4) & 0x0f) | _MAS3507D_CMD_WRITE_REG),
		uint8_t(((value  >>  0) & 0x0f) | ((offset << 4) & 0xf0)),
		uint8_t( (value  >> 12) & 0xff),
		uint8_t( (value  >>  4) & 0xff)
	};

	return _mas3507dCommand(packet, sizeof(packet));
}

bool digitalIOMP3Run(uint16_t func) {
	if (func > 0x1fff)
		return false;

	uint8_t packet[2]{
		uint8_t((func >> 8) & 0xff),
		uint8_t((func >> 0) & 0xff)
	};

	return _mas3507dCommand(packet, sizeof(packet));
}

}
