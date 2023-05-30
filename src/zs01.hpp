
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace zs01 {

/* Command definitions */

enum Address : uint8_t {
	ADDR_PUBLIC      = 0x00,
	ADDR_PUBLIC_END  = 0x04,
	ADDR_PRIVATE     = 0x04,
	ADDR_PRIVATE_END = 0x0e,
	ADDR_ZS01_ID     = 0xfc, // Read-only (?)
	ADDR_DS2401_ID   = 0xfd, // Read-only
	ADDR_ERASE       = 0xfd, // Write-only
	ADDR_CONFIG      = 0xfe,
	ADDR_DATA_KEY    = 0xff  // Write-only
};

enum RequestFlag : uint8_t {
	REQ_WRITE        = 0 << 0,
	REQ_READ         = 1 << 0,
	REQ_BANK_SWITCH  = 1 << 1, // Unused (should be bit 8 of block address)
	REQ_USE_DATA_KEY = 1 << 2
};

enum ResponseCode : uint8_t {
	// The meaning of these codes is currently unknown. Presumably:
	// - one of the "security errors" is a CRC validation failure, the other
	//   could be data key related
	// - one of the unknown errors is for invalid commands or addresses
	// - one or two of the unknown errors are for actual read/write failures
	RESP_NO_ERROR        = 0x00,
	RESP_SECURITY_ERROR1 = 0x01,
	RESP_UNKNOWN_ERROR1  = 0x02,
	RESP_UNKNOWN_ERROR2  = 0x03,
	RESP_SECURITY_ERROR2 = 0x04,
	RESP_UNKNOWN_ERROR3  = 0x05
};

/* Packet encoding/decoding */

class Key {
public:
	uint8_t add[8], shift[8];

	void unpackFrom(const uint8_t *key);
	void packInto(uint8_t *key) const;
	void encodePacket(uint8_t *data, size_t length, uint8_t state = 0xff) const;
	void decodePacket(uint8_t *data, size_t length, uint8_t state = 0xff) const;
	void encodePayload(uint8_t *data, size_t length, uint8_t state = 0xff) const;
};

class Packet {
public:
	uint8_t command, address, data[8], crc[2];

	inline void copyDataFrom(const uint8_t *source) {
		memcpy(data, source, sizeof(data));
	}
	inline void copyDataTo(uint8_t *dest) const {
		memcpy(dest, data, sizeof(data));
	}
	inline void clearData(void) {
		memset(data, 0, sizeof(data));
	}

	void updateCRC(void);
	bool validateCRC(void) const;

	void encodeReadRequest(void);
	void encodeReadRequest(Key &dataKey, uint8_t state = 0xff);
	void encodeWriteRequest(Key &dataKey, uint8_t state = 0xff);
	bool decodeResponse(void);
};

}
