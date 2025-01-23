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
#include "common/sys573/mp3.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/bus.hpp"

namespace sys573 {

/* MAS3507D MP3 decoder driver */

enum MAS3507DPacketType : uint8_t {
	_MAS_PACKET_COMMAND = 0x68, // Called "write" in the datasheet
	_MAS_PACKET_READ    = 0x69,
	_MAS_PACKET_RESET   = 0x6a  // Called "control" in the datasheet
};

enum MAS3507DCommand : uint8_t {
	_MAS_CMD_RUN         = 0x0 << 4,
	_MAS_CMD_READ_STATUS = 0x3 << 4,
	_MAS_CMD_WRITE_REG   = 0x9 << 4,
	_MAS_CMD_WRITE_D0    = 0xa << 4,
	_MAS_CMD_WRITE_D1    = 0xb << 4,
	_MAS_CMD_READ_REG    = 0xd << 4,
	_MAS_CMD_READ_D0     = 0xe << 4,
	_MAS_CMD_READ_D1     = 0xf << 4
};

static constexpr uint8_t _MAS_I2C_ADDR = 0x1d;

bool MAS3507DDriver::_issueCommand(const uint8_t *data, size_t length) const {
	if (!_i2c.startDeviceWrite(_MAS_I2C_ADDR)) {
		_i2c.stop();
		LOG_IO("chip not responding");
		return false;
	}

	_i2c.writeByte(_MAS_PACKET_COMMAND);
	if (!_i2c.getACK()) {
		_i2c.stop();
		LOG_IO("NACK while sending type");
		return false;
	}

	if (!_i2c.writeBytes(data, length)) {
		_i2c.stop();
		LOG_IO("NACK while sending data");
		return false;
	}

	_i2c.stop();
	return true;
}

bool MAS3507DDriver::_issueRead(uint8_t *data, size_t length) const {
	// Due to the MAS3507D's weird I2C protocol layering, reads are performed by
	// first wrapping a read request into a "write" packet, then starting a new
	// read packet and actually reading the data.
	if (!_i2c.startDeviceWrite(_MAS_I2C_ADDR)) {
		_i2c.stop();
		LOG_IO("chip not responding");
		return false;
	}

	_i2c.writeByte(_MAS_PACKET_READ);
	if (!_i2c.getACK()) {
		_i2c.stop();
		LOG_IO("NACK while sending type");
		return false;
	}

	if (!_i2c.startDeviceRead(_MAS_I2C_ADDR)) {
		_i2c.stop();
		LOG_IO("chip not responding");
		return false;
	}

	_i2c.readBytes(data, length);
	_i2c.sendACK(false);
	_i2c.stop();
	return true;
}

int MAS3507DDriver::readFrameCount(void) const {
	uint8_t response[2];

	if (!_issueRead(response, sizeof(response)))
		return -1;

	return util::concat2(response[1], response[0]);
}

int MAS3507DDriver::readMemory(int bank, uint16_t offset) const {
	uint8_t packet[6]{
		bank ? _MAS_CMD_READ_D1 : _MAS_CMD_READ_D0,
		0,
		0,
		1,
		uint8_t((offset >>  8) & 0xff),
		uint8_t((offset >>  0) & 0xff)
	};
	uint8_t response[4];

	if (!_issueCommand(packet, sizeof(packet)))
		return -1;
	if (!_issueRead(response, sizeof(response)))
		return -1;

	return util::concat4(
		response[1],
		response[0],
		response[3] & 0x0f,
		0
	);
}

bool MAS3507DDriver::writeMemory(int bank, uint16_t offset, int value) const {
	uint8_t packet[10]{
		bank ? _MAS_CMD_WRITE_D1 : _MAS_CMD_WRITE_D0,
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

	return _issueCommand(packet, sizeof(packet));
}

int MAS3507DDriver::readReg(uint8_t offset) const {
	uint8_t packet[2]{
		uint8_t(((offset >> 4) & 0x0f) | _MAS_CMD_READ_REG),
		uint8_t( (offset << 4) & 0xf0)
	};
	uint8_t response[4];

	if (!_issueCommand(packet, sizeof(packet)))
		return -1;
	if (!_issueRead(response, sizeof(response)))
		return -1;

	return util::concat4(
		response[1],
		response[0],
		response[3] & 0x0f,
		0
	);
}

bool MAS3507DDriver::writeReg(uint8_t offset, int value) const {
	uint8_t packet[4]{
		uint8_t(((offset >>  4) & 0x0f) | _MAS_CMD_WRITE_REG),
		uint8_t(((value  >>  0) & 0x0f) | ((offset << 4) & 0xf0)),
		uint8_t( (value  >> 12) & 0xff),
		uint8_t( (value  >>  4) & 0xff)
	};

	return _issueCommand(packet, sizeof(packet));
}

bool MAS3507DDriver::runFunction(uint16_t func) const {
	if (func > 0x1fff)
		return false;

	uint8_t packet[2]{
		uint8_t((func >> 8) & 0xff),
		uint8_t((func >> 0) & 0xff)
	};

	return _issueCommand(packet, sizeof(packet));
}

}
