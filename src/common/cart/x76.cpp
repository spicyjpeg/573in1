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
#include "common/cart/x76.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/bus.hpp"

namespace cart {

static constexpr int _X76_MAX_ACK_POLLS = 5;
static constexpr int _X76_WRITE_DELAY   = 12000;
static constexpr int _X76_PACKET_DELAY  = 12000;

/* Utilities */

CartError _issueX76Command(
	const bus::I2CDriver &i2c,
	const uint8_t        *data,
	size_t               length,
	const uint8_t        *key,
	uint8_t              pollByte
) {
	delayMicroseconds(_X76_PACKET_DELAY);
	i2c.startWithCS();

	if (!i2c.writeBytes(data, length)) {
		i2c.stopWithCS();

		LOG_CART("NACK while sending command");
		return CHIP_ERROR;
	}

	if (key) {
		if (!i2c.writeBytes(key, KEY_LENGTH)) {
			i2c.stopWithCS();

			LOG_CART("NACK while sending key");
			return CHIP_ERROR;
		}
	}

	for (int i = _X76_MAX_ACK_POLLS; i; i--) {
		delayMicroseconds(_X76_WRITE_DELAY);
		i2c.start();
		i2c.writeByte(pollByte);

		if (i2c.getACK())
			return NO_ERROR;
	}

	i2c.stopWithCS();

	LOG_CART("ACK poll timeout (wrong key?)");
	return CHIP_TIMEOUT;
}

/* X76F041 security cartridge driver */

enum X76F041Command : uint8_t {
	_X76F041_WRITE    = 0x40,
	_X76F041_READ     = 0x60,
	_X76F041_CONFIG   = 0x80,
	_X76F041_ACK_POLL = 0xc0
};

enum X76F041ConfigCommand : uint8_t {
	_X76F041_CFG_SET_WRITE_KEY   = 0x00,
	_X76F041_CFG_SET_READ_KEY    = 0x10,
	_X76F041_CFG_SET_CONFIG_KEY  = 0x20,
	_X76F041_CFG_CLEAR_WRITE_KEY = 0x30,
	_X76F041_CFG_CLEAR_READ_KEY  = 0x40,
	_X76F041_CFG_WRITE_CONFIG    = 0x50,
	_X76F041_CFG_READ_CONFIG     = 0x60,
	_X76F041_CFG_MASS_PROGRAM    = 0x70,
	_X76F041_CFG_MASS_ERASE      = 0x80
};

static constexpr size_t _X76F041_SECTORS_PER_BLOCK = 16;
static constexpr size_t _X76F041_CONFIG_LENGTH     = 5;

CartError X76F041Cart::read(
	void          *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	// Even though the X76F041 supports unprivileged reads, attempting to
	// perform one on a privileged block will trigger the failed attempt counter
	// (as if the wrong key was provided). Since different games protect
	// different blocks and there is no other way to tell which blocks are
	// privileged, this renders unprivileged reads virtually useless.
	if (!key)
		return UNSUPPORTED_OP;

	auto ptr = reinterpret_cast<uint8_t *>(data);

	while (count > 0) {
		// A single read operation may span multiple sectors but can't cross
		// 128-byte block boundaries.
		auto blockOffset = lba % _X76F041_SECTORS_PER_BLOCK;
		auto readCount   =
			util::min(count, _X76F041_SECTORS_PER_BLOCK - blockOffset);
		auto readLength  = SECTOR_LENGTH * readCount;

		const uint8_t packet[2]{
			uint8_t((lba >> 8) | _X76F041_READ),
			uint8_t((lba >> 0) & 0xff)
		};

		auto error = _issueX76Command(
			_i2c,
			packet,
			sizeof(packet),
			key,
			_X76F041_ACK_POLL
		);

		if (error)
			return error;

		_i2c.readByte(); // Ignore "secure read setup" byte
		_i2c.start();
		_i2c.writeByte(packet[1]);

		if (!_i2c.getACK()) {
			_i2c.stopWithCS();

			LOG_CART("NACK after resending address");
			return CHIP_ERROR;
		}

		_i2c.readBytes(ptr, readLength);
		_i2c.stopWithCS();

		ptr   += readLength;
		lba   += readCount;
		count -= readCount;
	}

	return NO_ERROR;
}

CartError X76F041Cart::write(
	const void    *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	auto ptr = reinterpret_cast<const uint8_t *>(data);

	while (count > 0) {
		const uint8_t packet[2]{
			uint8_t((lba >> 8) | _X76F041_WRITE),
			uint8_t((lba >> 0) & 0xff)
		};

		auto error = _issueX76Command(
			_i2c,
			packet,
			sizeof(packet),
			key,
			_X76F041_ACK_POLL
		);

		if (error)
			return error;

		auto ok = _i2c.writeBytes(ptr, SECTOR_LENGTH);
		_i2c.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART("NACK while sending data");
			return CHIP_ERROR;
		}

		ptr += SECTOR_LENGTH;
		lba++;
		count--;
	}

	return NO_ERROR;
}

CartError X76F041Cart::erase(const uint8_t *key) {
	const uint8_t packet[2]{
		_X76F041_CONFIG,
		_X76F041_CFG_MASS_PROGRAM
	};

	auto error = _issueX76Command(
		_i2c,
		packet,
		sizeof(packet),
		key,
		_X76F041_ACK_POLL
	);

	if (error)
		return error;

	_i2c.stopWithCS(_X76_WRITE_DELAY);
	return NO_ERROR;
}

CartError X76F041Cart::readConfig(uint8_t *config, const uint8_t *key) {
	const uint8_t packet[2]{
		_X76F041_CONFIG,
		_X76F041_CFG_READ_CONFIG
	};

	auto error = _issueX76Command(
		_i2c,
		packet,
		sizeof(packet),
		key,
		_X76F041_ACK_POLL
	);

	if (error)
		return error;

	__builtin_memset(config, 0, CONFIG_LENGTH);
	_i2c.readBytes(config, _X76F041_CONFIG_LENGTH);
	_i2c.stopWithCS();
	return NO_ERROR;
}

CartError X76F041Cart::writeConfig(const uint8_t *config, const uint8_t *key) {
	const uint8_t packet[2]{
		_X76F041_CONFIG,
		_X76F041_CFG_WRITE_CONFIG
	};

	auto error = _issueX76Command(
		_i2c,
		packet,
		sizeof(packet),
		key,
		_X76F041_ACK_POLL
	);

	if (error)
		return error;

	auto ok = _i2c.writeBytes(config, _X76F041_CONFIG_LENGTH);
	_i2c.stopWithCS(_X76_WRITE_DELAY);

	if (!ok) {
		LOG_CART("NACK while sending new config");
		return CHIP_ERROR;
	}

	return NO_ERROR;
}

CartError X76F041Cart::setKey(const uint8_t *newKey, const uint8_t *oldKey) {
	// All known games use the configuration key for all commands and leave the
	// read and write keys unused.
	const uint8_t packet[2]{
		_X76F041_CONFIG,
		_X76F041_CFG_SET_CONFIG_KEY
	};

	auto error = _issueX76Command(
		_i2c,
		packet,
		sizeof(packet),
		oldKey,
		_X76F041_ACK_POLL
	);

	if (error)
		return error;

	// The chip requires the new key to be sent twice as a way of ensuring it
	// gets received correctly.
	for (int i = 0; i < 2; i++) {
		if (!_i2c.writeBytes(newKey, KEY_LENGTH)) {
			_i2c.stopWithCS(_X76_WRITE_DELAY);

			LOG_CART("NACK while sending new key, i=%d", i);
			return CHIP_ERROR;
		}
	}

	_i2c.stopWithCS(_X76_WRITE_DELAY);
	return NO_ERROR;
}

/* X76F100 security cartridge driver */

enum X76F100Command : uint8_t {
	_X76F100_ACK_POLL      = 0x55,
	_X76F100_WRITE         = 0x80,
	_X76F100_READ          = 0x81,
	_X76F100_SET_WRITE_KEY = 0xfc,
	_X76F100_SET_READ_KEY  = 0xfe
};

CartError X76F100Cart::read(
	void          *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	// The X76F100 does not support unprivileged reads.
	if (!key)
		return UNSUPPORTED_OP;

	const uint8_t cmd = _X76F100_READ | (lba << 1);

	auto error = _issueX76Command(
		_i2c,
		&cmd,
		sizeof(cmd),
		key,
		_X76F100_ACK_POLL
	);

	if (error)
		return error;

#if 0
	_i2c.start();
#endif
	_i2c.readBytes(reinterpret_cast<uint8_t *>(data), SECTOR_LENGTH * count);
	_i2c.stopWithCS();
	return NO_ERROR;
}

CartError X76F100Cart::write(
	const void    *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	auto ptr = reinterpret_cast<const uint8_t *>(data);

	while (count > 0) {
		const uint8_t cmd = _X76F100_WRITE | (lba << 1);

		auto error = _issueX76Command(
			_i2c,
			&cmd,
			sizeof(cmd),
			key,
			_X76F100_ACK_POLL
		);

		if (error)
			return error;

		auto ok = _i2c.writeBytes(ptr, SECTOR_LENGTH);
		_i2c.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART("NACK while sending data");
			return CHIP_ERROR;
		}

		ptr += SECTOR_LENGTH;
		lba++;
		count--;
	}

	return NO_ERROR;
}

CartError X76F100Cart::erase(const uint8_t *key) {
	// The chip does not have an erase command, so erasing must be performed
	// manually one block at a time. The keys must also be explicitly cleared.
	const uint8_t dummy[SECTOR_LENGTH]{ 0 };

	for (uint16_t i = 0; i < capacity; i++) {
		auto error = write(dummy, i, sizeof(dummy), key);

		if (error)
			return error;
	}

	return setKey(dummy, key);
}

CartError X76F100Cart::setKey(const uint8_t *newKey, const uint8_t *oldKey) {
	// All known games use the same key for both reading and writing.
	const uint8_t packets[2]{
		_X76F100_SET_WRITE_KEY,
		_X76F100_SET_READ_KEY
	};

	for (auto &cmd : packets) {
		auto error = _issueX76Command(
			_i2c,
			&cmd,
			sizeof(cmd),
			oldKey,
			_X76F100_ACK_POLL
		);

		if (error)
			return error;

		auto ok = _i2c.writeBytes(newKey, KEY_LENGTH);
		_i2c.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART("NACK while sending new key, cmd=0x%02x", cmd);
			return CHIP_ERROR;
		}
	}

	return NO_ERROR;
}

}
