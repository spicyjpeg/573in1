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
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "main/cart/zs01.hpp"

namespace cart {

/* Fixed keys */

// This key is identical across all ZS01 cartridges and seems to be hardcoded.
static const ZS01Key _COMMAND_KEY{
	.add   = { 237, 8, 16, 11, 6, 4, 8, 30 },
	.shift = {   0, 3,  2,  2, 6, 2, 2,  1 }
};

// This key is provided by the 573 to the ZS01 and is used to encode responses.
// Konami's driver generates a pseudorandom key for each transaction, but it can
// be a fixed key as well.
static const ZS01Key _RESPONSE_KEY{
	.add   = { 0, 0, 0, 0, 0, 0, 0, 0 },
	.shift = { 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Packet encoding/decoding */

void ZS01Key::unpackFrom(const uint8_t *key) {
	add[0]   = key[0];
	shift[0] = 0;

	for (int i = 1; i < 8; i++) {
		add[i]   = key[i] & 0x1f;
		shift[i] = key[i] >> 5;
	}
}

void ZS01Key::packInto(uint8_t *key) const {
	key[0] = add[0];

	for (int i = 1; i < 8; i++)
		key[i] = (add[i] & 0x1f) | (shift[i] << 5);
}

void ZS01Key::encodePacket(uint8_t *data, size_t length, uint8_t state) const {
	for (data += length; length; length--) {
		uint8_t value = *(--data) ^ state;
		value = (value + add[0]) & 0xff;

		for (int i = 1; i < 8; i++) {
			int newValue;
			newValue  = static_cast<int>(value) << shift[i];
			newValue |= static_cast<int>(value) >> (8 - shift[i]);
			newValue &= 0xff;

			value = (newValue + add[i]) & 0xff;
		}

		state = value;
		*data = value;
	}
}

void ZS01Key::decodePacket(uint8_t *data, size_t length, uint8_t state) const {
	for (data += length; length; length--) {
		uint8_t value = *(--data), prevState = state;
		state = value;

		for (int i = 7; i; i--) {
			int newValue = (value - add[i]) & 0xff;
			value  = static_cast<int>(newValue) >> shift[i];
			value |= static_cast<int>(newValue) << (8 - shift[i]);
			value &= 0xff;
		}

		value = (value - add[0]) & 0xff;
		*data = value ^ prevState;
	}
}

void ZS01Key::encodePayload(uint8_t *data, size_t length, uint8_t state) const {
	for (; length; length--) {
		uint8_t value = *data ^ state;
		value = (value + add[0]) & 0xff;

		for (int i = 1; i < 8; i++) {
			int newValue;
			newValue  = static_cast<int>(value) << shift[i];
			newValue |= static_cast<int>(value) >> (8 - shift[i]);
			newValue &= 0xff;

			value = (newValue + add[i]) & 0xff;
		}

		state     = value;
		*(data++) = value;
	}
}

void ZS01Packet::updateCRC(void) {
	uint16_t value = util::zsCRC16(&command, sizeof(ZS01Packet) - sizeof(crc));
	crc            = __builtin_bswap16(value);
}

bool ZS01Packet::validateCRC(void) const {
	uint16_t _crc  = __builtin_bswap16(crc);
	uint16_t value = util::zsCRC16(&command, sizeof(ZS01Packet) - sizeof(crc));

	if (value != _crc) {
		LOG_CART("mismatch, exp=0x%04x, got=0x%04x", value, _crc);
		return false;
	}

	return true;
}

void ZS01Packet::encodeReadRequest(void) {
	LOG_CART("addr=0x%02x", address);

	command = ZS01_REQ_READ;
	_RESPONSE_KEY.packInto(data);
	updateCRC();

	_COMMAND_KEY.encodePacket(&command, sizeof(ZS01Packet));
}

void ZS01Packet::encodeReadRequest(ZS01Key &dataKey, uint8_t state) {
	LOG_CART("addr=0x%02x, privileged", address);

	command = ZS01_REQ_READ | ZS01_REQ_PRIVILEGED;
	_RESPONSE_KEY.packInto(data);
	updateCRC();

	dataKey.encodePayload(data, sizeof(data), state);
	_COMMAND_KEY.encodePacket(&command, sizeof(ZS01Packet));
}

void ZS01Packet::encodeWriteRequest(ZS01Key &dataKey, uint8_t state) {
	LOG_CART("addr=0x%02x", address);

	command = ZS01_REQ_READ | ZS01_REQ_PRIVILEGED;
	updateCRC();

	dataKey.encodePayload(data, sizeof(data), state);
	_COMMAND_KEY.encodePacket(&command, sizeof(ZS01Packet));
}

bool ZS01Packet::decodeResponse(void) {
	// NOTE: if a non-fixed response key is used, the ZS01 may encode the
	// response to a read request with either the key provided in the request
	// *or* the last key used (Konami's driver attempts decoding the response
	// with both keys before giving up). When replying to a write request, the
	// ZS01 always encodes the response with the same key it used when replying
	// to the last read request. Confused yet?
	_RESPONSE_KEY.decodePacket(&command, sizeof(ZS01Packet));

	return validateCRC();
}

}
