
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "zs01.hpp"

namespace cart {

enum Error {
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
	TYPE_NONE    = 0,
	TYPE_X76F041 = 1,
	TYPE_X76F100 = 2,
	TYPE_ZS01    = 3
};

enum CartFlag : uint8_t {
	HAS_DIGITAL_IO  = 1 << 0,
	HAS_DS2401      = 1 << 1,
	CONFIG_OK       = 1 << 2,
	SYSTEM_ID_OK    = 1 << 3,
	CART_ID_OK      = 1 << 4,
	ZS_ID_OK        = 1 << 5,
	PUBLIC_DATA_OK  = 1 << 6,
	PRIVATE_DATA_OK = 1 << 7
};

static constexpr int DUMP_VERSION         = 1;
static constexpr int NUM_CHIP_TYPES       = 4;
static constexpr int MAX_QR_STRING_LENGTH = 0x600;

class Cart;

size_t getDataLength(ChipType type);
Cart *createCart(void);

class [[gnu::packed]] Cart {
public:
	uint8_t  version;
	ChipType chipType;

	uint8_t flags, _state;
	uint8_t dataKey[8], config[8];
	uint8_t systemID[8], cartID[8], zsID[8];

	inline size_t getDumpLength(void) {
		return getDataLength(chipType) + 44;
	}

	Cart(void);
	size_t toQRString(char *output);
	virtual Error readSystemID(void);
	virtual Error readCartID(void) { return UNSUPPORTED_OP; }
	virtual Error readPublicData(void) { return UNSUPPORTED_OP; }
	virtual Error readPrivateData(void) { return UNSUPPORTED_OP; }
	virtual Error writeData(void) { return UNSUPPORTED_OP; }
	virtual Error erase(void) { return UNSUPPORTED_OP; }
	virtual Error setDataKey(const uint8_t *newKey) { return UNSUPPORTED_OP; }
};

class [[gnu::packed]] X76Cart : public Cart {
protected:
	Error _readDS2401(void);
	Error _x76Command(uint8_t command, uint8_t param, uint8_t pollByte) const;

public:
	uint8_t data[512];

	Error readCartID(void);
};

class [[gnu::packed]] X76F041Cart : public X76Cart {
public:
	uint8_t data[512];

	X76F041Cart(void);
	Error readPrivateData(void);
	Error writeData(void);
	Error erase(void);
	Error setDataKey(const uint8_t *newKey);
};

class [[gnu::packed]] X76F100Cart : public X76Cart {
public:
	uint8_t data[112];

	X76F100Cart(void);
	//Error readPrivateData(void);
	//Error writeData(void);
	//Error erase(void);
	//Error setDataKey(const uint8_t *newKey);
};

class [[gnu::packed]] ZS01Cart : public Cart {
private:
	Error _transact(zs01::Packet &request, zs01::Packet &response);

public:
	uint8_t data[112];

	ZS01Cart(void);
	Error readCartID(void);
	Error readPublicData(void);
	Error readPrivateData(void);
	Error writeData(void);
	Error erase(void);
	Error setDataKey(const uint8_t *newKey);
};

}
