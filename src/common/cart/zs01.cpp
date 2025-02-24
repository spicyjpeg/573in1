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
#include "common/cart/zs01.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"

namespace cart {

/* ZS01 packet scrambling */

// This key is fixed across all ZS01 cartridges and used to scramble command
// packets.
static const ZS01Key _COMMAND_KEY{
	.add   = { 237, 8, 16, 11, 6, 4, 8, 30 },
	.shift = {   0, 3,  2,  2, 6, 2, 2,  1 }
};

// This key is provided by the 573 to the ZS01 and used to scramble response
// packets. Konami's driver generates random response keys for each transaction,
// however the ZS01 does not impose any requirements on it.
static const ZS01Key _RESPONSE_KEY{
	.add   = { 0, 0, 0, 0, 0, 0, 0, 0 },
	.shift = { 0, 0, 0, 0, 0, 0, 0, 0 }
};

void ZS01Key::unpackFrom(const uint8_t *key) {
	add[0]   = key[0];
	shift[0] = 0;

	for (size_t i = 1; i < KEY_LENGTH; i++) {
		add[i]   = key[i] & 0x1f;
		shift[i] = key[i] >> 5;
	}
}

void ZS01Key::packInto(uint8_t *key) const {
	key[0] = add[0];

	for (size_t i = 1; i < KEY_LENGTH; i++)
		key[i] = (add[i] & 0x1f) | (shift[i] << 5);
}

void ZS01Key::scramblePacket(
	uint8_t *data,
	size_t  length,
	uint8_t state
) const {
	for (data += length; length; length--) {
		uint8_t value = *(--data) ^ state;
		value = (value + add[0]) & 0xff;

		for (size_t i = 1; i < KEY_LENGTH; i++) {
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

void ZS01Key::unscramblePacket(
	uint8_t *data,
	size_t  length,
	uint8_t state
) const {
	for (data += length; length; length--) {
		uint8_t value = *(--data), prevState = state;
		state = value;

		for (int i = KEY_LENGTH - 1; i; i--) {
			int newValue = (value - add[i]) & 0xff;
			value  = static_cast<int>(newValue) >> shift[i];
			value |= static_cast<int>(newValue) << (8 - shift[i]);
			value &= 0xff;
		}

		value = (value - add[0]) & 0xff;
		*data = value ^ prevState;
	}
}

void ZS01Key::scramblePayload(
	uint8_t *data,
	size_t  length,
	uint8_t state
) const {
	for (; length; length--) {
		uint8_t value = *data ^ state;
		value = (value + add[0]) & 0xff;

		for (size_t i = 1; i < KEY_LENGTH; i++) {
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

/* ZS01 packet structure */

void ZS01Packet::updateChecksum(void) {
	uint16_t value = util::zsCRC16(&command, sizeof(ZS01Packet) - sizeof(crc));
	crc            = __builtin_bswap16(value);
}

bool ZS01Packet::validateChecksum(void) const {
	uint16_t _crc  = __builtin_bswap16(crc);
	uint16_t value = util::zsCRC16(&command, sizeof(ZS01Packet) - sizeof(crc));

	if (value != _crc) {
		LOG_CART("mismatch, exp=0x%04x, got=0x%04x", value, _crc);
		return false;
	}

	return true;
}

void ZS01Packet::setRead(uint16_t _address) {
	command = ZS01_REQ_READ;
	address = uint8_t(_address & 0xff);

#if 0
	if (_address & (1 << 8))
		command |= ZS01_REQ_ADDRESS_MSB;
#endif

	_RESPONSE_KEY.packInto(data);
}

void ZS01Packet::setWrite(uint16_t _address, const uint8_t *_data) {
	command = ZS01_REQ_WRITE;
	address = uint8_t(_address & 0xff);

#if 0
	if (_address & (1 << 8))
		command |= ZS01_REQ_ADDRESS_MSB;
#endif

	__builtin_memcpy(data, _data, sizeof(data));
}

void ZS01Packet::encodeRequest(const uint8_t *key, uint8_t state) {
	if (key)
		command |= ZS01_REQ_PRIVILEGED;
	else
		command &= ~ZS01_REQ_PRIVILEGED;

	updateChecksum();

	if (key) {
		ZS01Key payloadKey;

		payloadKey.unpackFrom(key);
		payloadKey.scramblePayload(data, sizeof(data), state);
	}

	_COMMAND_KEY.scramblePacket(&command, sizeof(ZS01Packet));
}

bool ZS01Packet::decodeResponse(void) {
	// NOTE: the ZS01 may scramble the response to a read request with either
	// the key provided in the request payload *or* the last response key
	// provided beforehand (Konami's driver attempts unscrambling the response
	// using either key before giving up). Responses to write requests are
	// always scrambled using the last read request's response key, as write
	// packets contain data to be written in place of the key. Confused yet?
	_RESPONSE_KEY.unscramblePacket(&command, sizeof(ZS01Packet));

	return validateChecksum();
}

/* ZS01 security cartridge driver */

// _ZS01_SEND_DELAY and _ZS01_PACKET_DELAY are set to rather conservative values
// here. While it is likely possible to use lower delays, setting either to
// ~30000 is known to result in key corruption (rendering the cartridge
// inaccessible and thus soft-bricking it).
static constexpr int _ZS01_SEND_DELAY   = 100000;
static constexpr int _ZS01_PACKET_DELAY = 300000;

CartError ZS01Cart::_transact(
	const ZS01Packet &request,
	ZS01Packet       &response
) {
	delayMicroseconds(_ZS01_PACKET_DELAY);
	_i2c.start();

	if (!_i2c.writeBytes(
		&request.command,
		sizeof(ZS01Packet),
		_ZS01_SEND_DELAY
	)) {
		_i2c.stop();

		LOG_CART("NACK while sending request");
		return CHIP_ERROR;
	}

	_i2c.readBytes(&response.command, sizeof(ZS01Packet));
	_i2c.stop();

	if (!response.decodeResponse())
		return CHECKSUM_MISMATCH;

	_scramblerState = response.address;

	if (response.command != ZS01_RESP_NO_ERROR) {
		LOG_CART("ZS01 error, code=0x%02x", response.command);
		return CHIP_ERROR;
	}

	return NO_ERROR;
}

CartError ZS01Cart::read(
	void          *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	auto ptr = reinterpret_cast<uint8_t *>(data);

	while (count > 0) {
		ZS01Packet packet;

		packet.setRead(lba);
		packet.encodeRequest(key, _scramblerState);

		auto error = _transact(packet, packet);

		if (error)
			return error;

		__builtin_memcpy(ptr, packet.data, SECTOR_LENGTH);
		ptr += SECTOR_LENGTH;
		lba++;
		count--;
	}

	return NO_ERROR;
}

CartError ZS01Cart::write(
	const void    *data,
	uint16_t      lba,
	size_t        count,
	const uint8_t *key
) {
	auto ptr = reinterpret_cast<const uint8_t *>(data);

	while (count > 0) {
		ZS01Packet packet;

		packet.setWrite(lba, ptr);
		packet.encodeRequest(key, _scramblerState);

		auto error = _transact(packet, packet);

		if (error)
			return error;

		ptr += SECTOR_LENGTH;
		lba++;
		count--;
	}

	return NO_ERROR;
}

CartError ZS01Cart::erase(const uint8_t *key) {
	const uint8_t dummy[SECTOR_LENGTH]{ 0 };
	ZS01Packet    packet;

	packet.setWrite(ZS01_ADDR_ERASE, dummy);
	packet.encodeRequest(key, _scramblerState);

	return _transact(packet, packet);
}

CartError ZS01Cart::readConfig(uint8_t *config, const uint8_t *key) {
	ZS01Packet packet;

	packet.setRead(ZS01_ADDR_CONFIG);
	packet.encodeRequest(key, _scramblerState);

	auto error = _transact(packet, packet);

	if (!error)
		__builtin_memcpy(config, packet.data, CONFIG_LENGTH);

	return error;
}

CartError ZS01Cart::writeConfig(const uint8_t *config, const uint8_t *key) {
	ZS01Packet packet;

	packet.setWrite(ZS01_ADDR_CONFIG, config);
	packet.encodeRequest(key, _scramblerState);

	return _transact(packet, packet);
}

CartError ZS01Cart::setKey(const uint8_t *newKey, const uint8_t *oldKey) {
	ZS01Packet packet;

	packet.setWrite(ZS01_ADDR_SET_KEY, newKey);
	packet.encodeRequest(oldKey, _scramblerState);

	return _transact(packet, packet);
}

CartError ZS01Cart::readID(bus::OneWireID *output) {
	ZS01Packet packet;

	packet.setRead(ZS01_ADDR_DS2401_ID);
	packet.encodeRequest();

	auto error = _transact(packet, packet);

	if (error)
		return error;

	__builtin_memcpy(output, packet.data, sizeof(bus::OneWireID));
	return output->validateChecksum() ? NO_ERROR : INVALID_ID;
}

CartError ZS01Cart::readInternalID(bus::OneWireID *output) {
	ZS01Packet packet;

	packet.setRead(ZS01_ADDR_ZS01_ID);
	packet.encodeRequest();

	auto error = _transact(packet, packet);

	if (error)
		return error;

	__builtin_memcpy(output, packet.data, sizeof(bus::OneWireID));
	return output->validateChecksum() ? NO_ERROR : INVALID_ID;
}

}
