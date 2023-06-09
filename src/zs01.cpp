
#include <stddef.h>
#include <stdint.h>
#include "util.hpp"
#include "zs01.hpp"

namespace zs01 {

/* Fixed keys */

// This key is identical across all ZS01 cartridges and seems to be hardcoded.
static const Key _COMMAND_KEY{
	.add   = { 237, 8, 16, 11, 6, 4, 8, 30 },
	.shift = {   0, 3,  2,  2, 6, 2, 2,  1 }
};

// This key is provided by the 573 to the ZS01 and is used to encode responses.
// Konami's driver generates a pseudorandom key for each transaction, but it can
// be a fixed key as well.
static const Key _RESPONSE_KEY{
	.add   = { 0, 0, 0, 0, 0, 0, 0, 0 },
	.shift = { 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Packet encoding/decoding */

void Key::unpackFrom(const uint8_t *key) {
	add[0]   = key[0];
	shift[0] = 0;

	for (int i = 1; i < 8; i++) {
		add[i]   = key[i] & 0x1f;
		shift[i] = key[i] >> 5;
	}
}

void Key::packInto(uint8_t *key) const {
	key[0] = add[0];

	for (int i = 1; i < 8; i++)
		key[i] = (add[i] & 0x1f) | (shift[i] << 5);
}

void Key::encodePacket(uint8_t *data, size_t length, uint8_t state) const {
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

void Key::decodePacket(uint8_t *data, size_t length, uint8_t state) const {
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

void Key::encodePayload(uint8_t *data, size_t length, uint8_t state) const {
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

void Packet::updateCRC(void) {
	uint16_t value = util::zsCRC16(&command, sizeof(Packet) - sizeof(crc));

	crc[0] = value >> 8;
	crc[1] = value & 0xff;
}

bool Packet::validateCRC(void) const {
	uint16_t _crc  = (crc[0] << 8) | crc[1];
	uint16_t value = util::zsCRC16(&command, sizeof(Packet) - sizeof(crc));

	if (value != _crc) {
		LOG("mismatch, exp=0x%04x, got=0x%04x", value, _crc);
		return false;
	}

	return true;
}

void Packet::encodeReadRequest(void) {
	command = REQ_READ;
	_RESPONSE_KEY.packInto(data);
	updateCRC();

	_COMMAND_KEY.encodePacket(&command, sizeof(Packet));
}

void Packet::encodeReadRequest(Key &dataKey, uint8_t state) {
	command = REQ_READ | REQ_USE_DATA_KEY;
	_RESPONSE_KEY.packInto(data);
	updateCRC();

	dataKey.encodePayload(data, sizeof(data), state);
	_COMMAND_KEY.encodePacket(&command, sizeof(Packet));
}

void Packet::encodeWriteRequest(Key &dataKey, uint8_t state) {
	command = REQ_WRITE | REQ_USE_DATA_KEY;
	updateCRC();

	dataKey.encodePayload(data, sizeof(data), state);
	_COMMAND_KEY.encodePacket(&command, sizeof(Packet));
}

bool Packet::decodeResponse(void) {
	// NOTE: if a non-fixed response key is used, the ZS01 may encode the
	// response to a read request with either the key provided in the request
	// *or* the last key used (Konami's driver attempts decoding the response
	// with both keys before giving up). When replying to a write request, the
	// ZS01 always encodes the response with the same key it used when replying
	// to the last read request. Confused yet?
	_RESPONSE_KEY.decodePacket(&command, sizeof(Packet));

	return validateCRC();
}

}
