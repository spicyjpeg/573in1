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

namespace cart {

/* Command definitions */

enum ZS01Address : uint8_t {
	ZS01_ADDR_PUBLIC      = 0x00,
	ZS01_ADDR_PUBLIC_END  = 0x04,
	ZS01_ADDR_PRIVATE     = 0x04,
	ZS01_ADDR_PRIVATE_END = 0x0e,
	ZS01_ADDR_ZS01_ID     = 0xfc, // Read-only (?)
	ZS01_ADDR_DS2401_ID   = 0xfd, // Read-only
	ZS01_ADDR_ERASE       = 0xfd, // Write-only
	ZS01_ADDR_CONFIG      = 0xfe,
	ZS01_ADDR_DATA_KEY    = 0xff  // Write-only
};

enum ZS01RequestFlag : uint8_t {
	ZS01_REQ_WRITE      = 0 << 0,
	ZS01_REQ_READ       = 1 << 0,
	ZS01_REQ_ADDR_BIT8  = 1 << 1, // Unused (should be bit 8 of block address)
	ZS01_REQ_PRIVILEGED = 1 << 2
};

enum ZS01ResponseCode : uint8_t {
	// The meaning of these codes is currently unknown. Presumably:
	// - one of the "security errors" is a CRC validation failure, the other
	//   could be data key related, the third one could be DS2401 related
	// - one of the unknown errors is for invalid commands or addresses
	// - one of the unknown errors is for actual read/write failures
	ZS01_RESP_NO_ERROR        = 0x00,
	ZS01_RESP_UNKNOWN_ERROR1  = 0x01,
	ZS01_RESP_SECURITY_ERROR1 = 0x02,
	ZS01_RESP_SECURITY_ERROR2 = 0x03,
	ZS01_RESP_UNKNOWN_ERROR2  = 0x04,
	ZS01_RESP_SECURITY_ERROR3 = 0x05
};

/* Packet encoding/decoding */

class ZS01Key {
public:
	uint8_t add[8], shift[8];

	void unpackFrom(const uint8_t *key);
	void packInto(uint8_t *key) const;
	void encodePacket(uint8_t *data, size_t length, uint8_t state = 0xff) const;
	void decodePacket(uint8_t *data, size_t length, uint8_t state = 0xff) const;
	void encodePayload(uint8_t *data, size_t length, uint8_t state = 0xff) const;
};

class ZS01Packet {
public:
	uint8_t  command, address, data[8];
	uint16_t crc;

	inline void copyFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, sizeof(data));
	}
	inline void copyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, sizeof(data));
	}

	void updateCRC(void);
	bool validateCRC(void) const;

	void encodeReadRequest(void);
	void encodeReadRequest(ZS01Key &dataKey, uint8_t state = 0xff);
	void encodeWriteRequest(ZS01Key &dataKey, uint8_t state = 0xff);
	bool decodeResponse(void);
};

}
