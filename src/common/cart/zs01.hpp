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
#include "common/cart/cart.hpp"
#include "common/bus.hpp"

namespace cart {

/* ZS01 definitions */

enum ZS01Address : uint16_t {
	ZS01_ADDR_UNPRIVILEGED     = 0x0000,
	ZS01_ADDR_UNPRIVILEGED_END = 0x0004,
	ZS01_ADDR_PRIVILEGED       = 0x0004,
	ZS01_ADDR_PRIVILEGED_END   = 0x000e,
	ZS01_ADDR_ZS01_ID          = 0x00fc, // Unprivileged, read-only
	ZS01_ADDR_DS2401_ID        = 0x00fd, // Unprivileged, read-only
	ZS01_ADDR_ERASE            = 0x00fd, // Privileged, write-only
	ZS01_ADDR_CONFIG           = 0x00fe, // Privileged
	ZS01_ADDR_SET_KEY          = 0x00ff  // Privileged, write-only
};

enum ZS01RequestFlag : uint8_t {
	ZS01_REQ_WRITE       = 0 << 0,
	ZS01_REQ_READ        = 1 << 0,
	ZS01_REQ_ADDRESS_MSB = 1 << 1, // Unused
	ZS01_REQ_PRIVILEGED  = 1 << 2
};

enum ZS01ResponseCode : uint8_t {
	// The meaning of these codes is currently unknown. Presumably:
	// - one of the "security errors" is a CRC validation failure, the other
	//   could be data key related, the third one could be DS2401 related;
	// - one of the unknown errors is for invalid commands or addresses;
	// - one of the unknown errors is for actual read/write failures.
	ZS01_RESP_NO_ERROR        = 0x00,
	ZS01_RESP_UNKNOWN_ERROR1  = 0x01,
	ZS01_RESP_SECURITY_ERROR1 = 0x02,
	ZS01_RESP_SECURITY_ERROR2 = 0x03,
	ZS01_RESP_UNKNOWN_ERROR2  = 0x04,
	ZS01_RESP_SECURITY_ERROR3 = 0x05
};

/* ZS01 packet scrambling */

class ZS01Key {
public:
	uint8_t add[KEY_LENGTH];
	uint8_t shift[KEY_LENGTH];

	void unpackFrom(const uint8_t *key);
	void packInto(uint8_t *key) const;

	void scramblePacket(
		uint8_t *data,
		size_t  length,
		uint8_t state = 0xff
	) const;
	void unscramblePacket(
		uint8_t *data,
		size_t  length,
		uint8_t state = 0xff
	) const;
	void scramblePayload(
		uint8_t *data,
		size_t  length,
		uint8_t state = 0xff
	) const;
};

/* ZS01 packet structure */

class ZS01Packet {
public:
	uint8_t  command, address, data[8];
	uint16_t crc;

	void updateChecksum(void);
	bool validateChecksum(void) const;

	void setRead(uint16_t _address);
	void setWrite(uint16_t _address, const uint8_t *_data);
	void encodeRequest(const uint8_t *key = nullptr, uint8_t state = 0xff);
	bool decodeResponse(void);
};

/* ZS01 security cartridge driver */

class ZS01Cart : public Cart {
	friend Cart *_newCartDriver(const bus::I2CDriver &i2c);

private:
	uint8_t _scramblerState;

	inline ZS01Cart(const bus::I2CDriver &i2c)
	: Cart(i2c, ZS01, 112) {}

	CartError _transact(const ZS01Packet &request, ZS01Packet &response);

public:
	CartError read(
		void          *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key = nullptr
	);
	CartError write(
		const void    *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key
	);
	CartError erase(const uint8_t *key);

	CartError readConfig(uint8_t *config, const uint8_t *key);
	CartError writeConfig(const uint8_t *config, const uint8_t *key);
	CartError setKey(const uint8_t *newKey, const uint8_t *oldKey);

	CartError readID(bus::OneWireID &output);
	CartError readInternalID(bus::OneWireID &output);
};

}
