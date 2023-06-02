
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "util.hpp"
#include "zs01.hpp"

namespace cart {

/* Definitions */

enum CartError {
	NO_ERROR          = 0,
	UNSUPPORTED_OP    = 1,
	DS2401_NO_RESP    = 2,
	DS2401_ID_ERROR   = 3,
	X76_NACK          = 4,
	X76_POLL_FAIL     = 5,
	X76_VERIFY_FAIL   = 6,
	ZS01_NACK         = 7,
	ZS01_ERROR        = 8,
	ZS01_CRC_MISMATCH = 9
};

enum ChipType : uint8_t {
	NONE    = 0,
	X76F041 = 1,
	X76F100 = 2,
	ZS01    = 3
};

enum DumpFlag : uint8_t {
	DUMP_HAS_SYSTEM_ID   = 1 << 0,
	DUMP_HAS_CART_ID     = 1 << 1,
	DUMP_CONFIG_OK       = 1 << 2,
	DUMP_SYSTEM_ID_OK    = 1 << 3,
	DUMP_CART_ID_OK      = 1 << 4,
	DUMP_ZS_ID_OK        = 1 << 5,
	DUMP_PUBLIC_DATA_OK  = 1 << 6,
	DUMP_PRIVATE_DATA_OK = 1 << 7
};

static constexpr int    NUM_CHIP_TYPES       = 4;
static constexpr size_t MAX_QR_STRING_LENGTH = 0x600;

/* Common data structures */

class [[gnu::packed]] Identifier {
public:
	uint8_t data[8];

	inline void copyFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, sizeof(data));
	}
	inline void copyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, sizeof(data));
	}
	inline void clear(void) {
		__builtin_memset(data, 0, sizeof(data));
	}
	inline bool isEmpty(void) const {
		return (util::sum(data, sizeof(data)) == 0);
	}

	inline size_t toString(char *output) const {
		return util::hexToString(output, data, sizeof(data), '-');
	}
	inline size_t toSerialNumber(char *output) const {
		return util::serialNumberToString(output, &data[1]);
	}

	void updateChecksum(void);
	bool validateChecksum(void) const;
	void updateDSCRC(void);
	bool validateDSCRC(void) const;
};

/* Dump structure and utilities */

struct ChipSize {
public:
	size_t dataLength, publicDataOffset, publicDataLength;
};

extern const ChipSize CHIP_SIZES[NUM_CHIP_TYPES];

class [[gnu::packed]] Dump {
public:
	ChipType chipType;
	uint8_t  flags;

	Identifier systemID, cartID, zsID;

	uint8_t dataKey[8], config[8];
	uint8_t data[512];

	inline const ChipSize &getChipSize(void) const {
		return CHIP_SIZES[chipType];
	}
	inline size_t getDumpLength(void) const {
		return (sizeof(Dump) - sizeof(data)) + getChipSize().dataLength;
	}
	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(Dump));
	}
	inline void clearData(void) {
		__builtin_memset(data, 0, getChipSize().dataLength);
	}
	inline bool isDataEmpty(void) const {
		size_t length = getChipSize().dataLength;
		auto   sum    = util::sum(data, length);

		return (!sum || (sum == (0xff * length)));
	}

	size_t toQRString(char *output) const;
};

/* Cartridge driver classes */

class Cart {
protected:
	Dump &_dump;

public:
	inline Cart(Dump &dump, ChipType chipType = NONE, uint8_t flags = 0)
	: _dump(dump) {
		dump.clear();

		dump.chipType = chipType;
		dump.flags    = flags;
	}

	virtual CartError readSystemID(void);
	virtual CartError readCartID(void) { return UNSUPPORTED_OP; }
	virtual CartError readPublicData(void) { return UNSUPPORTED_OP; }
	virtual CartError readPrivateData(void) { return UNSUPPORTED_OP; }
	virtual CartError writeData(void) { return UNSUPPORTED_OP; }
	virtual CartError erase(void) { return UNSUPPORTED_OP; }
	virtual CartError setDataKey(const uint8_t *key) { return UNSUPPORTED_OP; }
};

class [[gnu::packed]] X76Cart : public Cart {
protected:
	CartError _readDS2401(void);
	CartError _x76Command(uint8_t cmd, uint8_t param, uint8_t pollByte) const;

public:
	inline X76Cart(Dump &dump, ChipType chipType)
	: Cart(dump, chipType) {}

	CartError readCartID(void);
};

class [[gnu::packed]] X76F041Cart : public X76Cart {
public:
	inline X76F041Cart(Dump &dump)
	: X76Cart(dump, X76F041) {}

	CartError readPrivateData(void);
	CartError writeData(void);
	CartError erase(void);
	CartError setDataKey(const uint8_t *key);
};

class [[gnu::packed]] X76F100Cart : public X76Cart {
public:
	inline X76F100Cart(Dump &dump)
	: X76Cart(dump, X76F100) {}

	CartError readPrivateData(void);
	CartError writeData(void);
	CartError erase(void);
	CartError setDataKey(const uint8_t *key);
};

class [[gnu::packed]] ZS01Cart : public Cart {
private:
	uint8_t _encoderState;

	CartError _transact(zs01::Packet &request, zs01::Packet &response);

public:
	inline ZS01Cart(Dump &dump)
	: Cart(dump, ZS01, DUMP_HAS_CART_ID), _encoderState(0) {}

	CartError readCartID(void);
	CartError readPublicData(void);
	CartError readPrivateData(void);
	CartError writeData(void);
	CartError erase(void);
	CartError setDataKey(const uint8_t *key);
};

Cart *createCart(Dump &dump);

}
