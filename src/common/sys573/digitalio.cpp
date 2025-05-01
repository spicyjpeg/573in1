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

#include <stddef.h>
#include <stdint.h>
#include "common/sys573/digitalio.hpp"
#include "common/sys573/mp3.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace sys573 {

/* Digital I/O board bus drivers */

static constexpr int _FPGA_UART_RESET_DELAY = 500;

static constexpr int _MIN_BAUD_RATE  = 4800;
static constexpr int _NUM_BAUD_RATES = 8;

static uint16_t _digitalIOI2CReg, _digitalIODSBusReg;

class DigitalIODS2401Driver : public bus::OneWireDriver {
private:
	bool _get(void) const {
		return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2401) & 1;
	}

	void _set(bool value) const {
		if (value)
			_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2401;
		else
			_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2401;

		SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
	}
};

class DigitalIODS2433Driver : public bus::OneWireDriver {
private:
	bool _get(void) const {
		return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2433) & 1;
	}

	void _set(bool value) const {
		if (value)
			_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2433;
		else
			_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2433;

		SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
	}
};

class DigitalIOUARTDriver : public bus::UARTDriver {
public:
	int init(int baud) const {
		// Find the closest available baud rate to the one requested.
		int baudIndex = -1;

		for (int i = 0; i < _NUM_BAUD_RATES; i++) {
			int lowerBound = _MIN_BAUD_RATE << i;
			int upperBound = lowerBound     << 1;

			if ((baud >= lowerBound) && (baud < upperBound)) {
				baudIndex = i;
				break;
			}
		}

		if (baudIndex < 0)
			return 0;
		if (SYS573D_FPGA_MAGIC != SYS573D_FPGA_MAGIC_573IN1)
			return 0;

		const uint16_t mask = 0
			| SYS573D_FPGA_UART_CTRL_TX_IDLE
			| SYS573D_FPGA_UART_CTRL_RX_IDLE;

		// In order to prevent glitches, wait for the UART to go idle and
		// disable it before changing the baud rate.
		while ((SYS573D_FPGA_UART_CTRL & mask) != mask)
			__asm__ volatile("");

		SYS573D_FPGA_UART_CTRL = 0
			| (baudIndex << 1)
			| SYS573D_FPGA_UART_CTRL_RTS;
		delayMicroseconds(_FPGA_UART_RESET_DELAY);

		SYS573D_FPGA_UART_CTRL = 0
			| SYS573D_FPGA_UART_CTRL_ENABLE
			| (baudIndex << 1)
			| SYS573D_FPGA_UART_CTRL_RTS;
		return _MIN_BAUD_RATE << baudIndex;
	}

	uint8_t readByte(void) const {
		while (!(SYS573D_FPGA_UART_CTRL & SYS573D_FPGA_UART_CTRL_RX_FULL))
			__asm__ volatile("");

		return SYS573D_FPGA_UART_DATA & 0xff;
	}

	void writeByte(uint8_t value) const {
		while (SYS573D_FPGA_UART_CTRL & SYS573D_FPGA_UART_CTRL_TX_FULL)
			__asm__ volatile("");

		SYS573D_FPGA_UART_DATA = value;
	}

	bool isRXAvailable(void) const {
		return (SYS573D_FPGA_UART_CTRL / SYS573D_FPGA_UART_CTRL_RX_FULL) & 1;
	}
	bool isTXFull(void) const {
		return (SYS573D_FPGA_UART_CTRL / SYS573D_FPGA_UART_CTRL_TX_FULL) & 1;
	}
};

class DigitalIOI2CDriver : public bus::I2CDriver {
private:
	bool _getSDA(void) const {
		return (SYS573D_FPGA_MP3_I2C / SYS573D_FPGA_MP3_I2C_SDA) & 1;
	}

	void _setSDA(bool value) const {
		if (value)
			_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SDA;
		else
			_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SDA;

		SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
	}

	void _setSCL(bool value) const {
		if (value)
			_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SCL;
		else
			_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SCL;

		SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;

		// The MAS3507D makes extensive use of clock stretching as part of its
		// protocol, so waiting until it deasserts SCL is needed here.
		while (
			(SYS573D_FPGA_MP3_I2C ^ _digitalIOI2CReg) & SYS573D_FPGA_MP3_I2C_SCL
		)
			__asm__ volatile("");
	}
};

static const DigitalIODS2401Driver _ds2401;
static const DigitalIODS2433Driver _ds2433;
static const DigitalIOUARTDriver   _serial;
static const DigitalIOI2CDriver    _i2c;

/* Digital I/O board FPGA bitstream loading */

static constexpr int _FPGA_PROGRAM_DELAY   = 5000;
static constexpr int _FPGA_STARTUP_DELAY   = 50000;
static constexpr int _MAX_PROGRAM_ATTEMPTS = 3;

enum BitstreamTagType : uint8_t {
	_TAG_SOURCE_FILE = 'a',
	_TAG_PART_NAME   = 'b',
	_TAG_BUILD_DATE  = 'c',
	_TAG_BUILD_TIME  = 'd',
	_TAG_DATA        = 'e'
};

DigitalIOBoardDriver::DigitalIOBoardDriver(void) {
	type            = IO_DIGITAL;
	extMemoryLength = 0x1800000;

	ds2401    = &_ds2401;
	ds2433    = &_ds2433;
	serial[0] = &_serial;
}

bool DigitalIOBoardDriver::_loadRawBitstream(
	const uint8_t *data, size_t length
) const {
	if (data[0] != 0xff) {
		LOG_IO("invalid sync byte: 0x%02x", data[0]);
		return false;
	}

	bool msbFirst;

	if (((data[1] & 0xf0) == 0x20) && ((data[4] & 0x0f) == 0x0f)) {
		msbFirst = true;
	} else if (((data[1] & 0x0f) == 0x04) && ((data[4] & 0xf0) == 0xf0)) {
		msbFirst = false;
	} else {
		LOG_IO("could not detect bit order");
		return false;
	}

	const uint16_t mask = 0
		| SYS573D_CPLD_INIT_STAT_INIT
		| SYS573D_CPLD_INIT_STAT_DONE;

	for (int i = _MAX_PROGRAM_ATTEMPTS; i; i--) {
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

		for (size_t j = 0; j < length; j++) {
			uint16_t bits = data[i] << 8;

			if (msbFirst) {
				bits <<= 8;

				for (int k = 8; k > 0; k--, bits <<= 1)
					SYS573D_CPLD_BITSTREAM = bits & (1 << 15);
			} else {
				for (int k = 8; k > 0; k--, bits >>= 1)
					SYS573D_CPLD_BITSTREAM = (bits & 1) << 15;
			}
		}

		delayMicroseconds(_FPGA_STARTUP_DELAY);

		status = SYS573D_CPLD_INIT_STAT;

		if ((status & mask) != mask) {
			LOG_IO("upload failed, st=0x%04x", status);
			continue;
		}

		_initFPGA();
		return true;
	}

	LOG_IO("too many attempts failed");
	return false;
}

bool DigitalIOBoardDriver::isReady(void) const {
	uint16_t magic = SYS573D_FPGA_MAGIC;

	return false
		|| (magic == SYS573D_FPGA_MAGIC_KONAMI)
		|| (magic == SYS573D_FPGA_MAGIC_573IN1);
}

bool DigitalIOBoardDriver::loadBitstream(const uint8_t *data, size_t length) {
	// Konami's bitstreams are always stored LSB-first and with no headers,
	// however Xilinx tools export .bit files which contain MSB-first bitstreams
	// wrapped in a TLV container. In order to upload the bitstream properly,
	// the bit order and presence of a header must be autodetected. See
	// https://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm and
	// the "Data Stream Format" section in the XCS40XL datasheet for details.
	if (data[0] == 0xff)
		return _loadRawBitstream(data, length);

	data += util::concat2(data[1], data[0]) + 4;

	while (length > 0) {
		size_t tagLength;

		switch (data[0]) {
			case _TAG_DATA:
				tagLength = util::concat4(data[4], data[3], data[2], data[1]);
				data     += 5;
				length   -= 5;

				return _loadRawBitstream(data, tagLength);

			default:
				tagLength = util::concat2(data[2], data[1]);
				data     += 3;
				length   -= 3;
		}

		data   += tagLength;
		length -= tagLength;
	}

	LOG_IO("no data tag found");
	return false;
}

/* Digital I/O board initialization */

static constexpr int _FPGA_RESET_REG_DELAY   = 500;
static constexpr int _MAS_RESET_ASSERT_DELAY = 500;
static constexpr int _MAS_RESET_CLEAR_DELAY  = 5000;

static const MAS3507DDriver _mas3507d(_i2c);

void DigitalIOBoardDriver::_initFPGA(void) const {
	SYS573D_FPGA_RESET = 0xf000;
	SYS573D_FPGA_RESET = 0x0000;
	delayMicroseconds(_FPGA_RESET_REG_DELAY);

	SYS573D_FPGA_RESET = 0xf000;
	delayMicroseconds(_FPGA_RESET_REG_DELAY);

	// Some of the digital I/O board's light outputs are controlled by the FPGA
	// and cannot be turned off until the FPGA is initialized.
	setLightOutputs(0);

	_digitalIOI2CReg   = 0
		| SYS573D_FPGA_MP3_I2C_SDA
		| SYS573D_FPGA_MP3_I2C_SCL;
	_digitalIODSBusReg = 0
		| SYS573D_FPGA_DS_BUS_DS2401
		| SYS573D_FPGA_DS_BUS_DS2433;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
	SYS573D_FPGA_DS_BUS  = _digitalIODSBusReg;
}

bool DigitalIOBoardDriver::_initMP3(void) const {
	// Turn off the DAC during initialization to prevent any audible popping.
	SYS573D_CPLD_DAC_RESET = 0;

	SYS573D_FPGA_MP3_CHIP_CTRL = SYS573D_FPGA_MP3_CHIP_CTRL_STATUS_CS;
	delayMicroseconds(_MAS_RESET_ASSERT_DELAY);
	SYS573D_FPGA_MP3_CHIP_CTRL = SYS573D_FPGA_MP3_CHIP_CTRL_RESET;
	delayMicroseconds(_MAS_RESET_CLEAR_DELAY);

	const auto startupCfg = 0
		| MAS_STARTUP_CFG_MODE_DATA_REQ
		| MAS_STARTUP_CFG_SAMPLE_FMT_16
		| MAS_STARTUP_CFG_LAYER2
		| MAS_STARTUP_CFG_LAYER3
		| MAS_STARTUP_CFG_INPUT_SDI
		| MAS_STARTUP_CFG_MCLK_DIVIDE;

	if (!_mas3507d.writeReg(MAS_REG_STARTUP_CFG, startupCfg))
		return false;
	if (!_mas3507d.runFunction(MAS_FUNC_UPDATE_STARTUP_CFG))
		return false;

	// The AK4309 DAC does not use the standard I2S protocol and instead
	// requires a 16- or 32-bit LSB justified stream, so the default output mode
	// has to be adjusted accordingly.
	const auto outputCfg = 0
		| MAS_OUTPUT_CFG_SAMPLE_FMT_16
		| MAS_OUTPUT_CFG_INVERT_LRCK;

	if (!_mas3507d.writeMemory(0, MAS_D0_OUTPUT_CFG, outputCfg))
		return false;
	if (!_mas3507d.runFunction(MAS_FUNC_UPDATE_OUTPUT_CFG))
		return false;

	SYS573D_CPLD_DAC_RESET = 1 << 15;
	return true;
}

/* Digital I/O board API */

static constexpr int _DRAM_READ_DELAY  = 1;
static constexpr int _DRAM_WRITE_DELAY = 1;

void DigitalIOBoardDriver::setLightOutputs(uint32_t bits) const {
	bits = ~bits;

	SYS573D_FPGA_LIGHTS_AL = (bits >>  0) << 12;
	SYS573D_FPGA_LIGHTS_AH = (bits >>  4) << 12;
	SYS573D_CPLD_LIGHTS_BL = (bits >>  8) << 12;
	SYS573D_FPGA_LIGHTS_BH = (bits >> 12) << 12;
	SYS573D_CPLD_LIGHTS_CL = (bits >> 16) << 12;
	SYS573D_CPLD_LIGHTS_CH = (bits >> 20) << 12;
	SYS573D_FPGA_LIGHTS_D  = (bits >> 24) << 12;
}

void DigitalIOBoardDriver::readExtMemory(
	void *data, uint32_t offset, size_t length
) {
	auto ptr = reinterpret_cast<uint16_t *>(data);

	SYS573D_FPGA_DRAM_RD_PTR_H = (offset >> 16) & 0xffff;
	SYS573D_FPGA_DRAM_RD_PTR_L = (offset >>  0) & 0xffff;

	for (; length > 0; length--) {
		// Give the DRAM arbiter enough time to fetch the next word before
		// attempting to read it.
		delayMicroseconds(_DRAM_READ_DELAY);
		*(ptr++) = SYS573D_FPGA_DRAM_DATA;
	}
}

void DigitalIOBoardDriver::writeExtMemory(
	const void *data, uint32_t offset, size_t length
) {
	auto ptr = reinterpret_cast<const uint16_t *>(data);

	SYS573D_FPGA_DRAM_WR_PTR_H = (offset >> 16) & 0xffff;
	SYS573D_FPGA_DRAM_WR_PTR_L = (offset >>  0) & 0xffff;

	for (; length > 0; length--) {
		SYS573D_FPGA_DRAM_DATA = *(ptr++);
		delayMicroseconds(_DRAM_WRITE_DELAY);
	}
}

}
