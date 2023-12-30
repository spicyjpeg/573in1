
#pragma once

#include <stdint.h>
#include "common/util.hpp"
#include "main/cart.hpp"
#include "main/zs01.hpp"

namespace cart {

enum DriverError {
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

/* Base classes */

extern Dump dummyDriverDump;

class Driver {
protected:
	Dump &_dump;

public:
	inline Driver(Dump &dump)
	: _dump(dump) {}

	virtual ~Driver(void) {}
	virtual DriverError readSystemID(void) { return UNSUPPORTED_OP; }
	virtual DriverError readCartID(void) { return UNSUPPORTED_OP; }
	virtual DriverError readPublicData(void) { return UNSUPPORTED_OP; }
	virtual DriverError readPrivateData(void) { return UNSUPPORTED_OP; }
	virtual DriverError writeData(void) { return UNSUPPORTED_OP; }
	virtual DriverError erase(void) { return UNSUPPORTED_OP; }
	virtual DriverError setDataKey(const uint8_t *key) { return UNSUPPORTED_OP; }
};

class DummyDriver : public Driver {
private:
	inline DriverError _getErrorCode(void) {
		return (_dump.chipType == ZS01) ? ZS01_ERROR : X76_NACK;
	}

public:
	inline DummyDriver(Dump &dump)
	: Driver(dump) {
		//dump.clearIdentifiers();
		//dump.clearKey();
		//dump.clearData();

		dump.chipType = dummyDriverDump.chipType;
		dump.flags    = dummyDriverDump.flags &
			(DUMP_HAS_SYSTEM_ID | DUMP_HAS_CART_ID);
	}

	DriverError readSystemID(void);
	DriverError readCartID(void);
	DriverError readPublicData(void);
	DriverError readPrivateData(void);
	DriverError writeData(void);
	DriverError erase(void);
	DriverError setDataKey(const uint8_t *key);
};

/* Cartridge driver classes */

class CartDriver : public Driver {
public:
	inline CartDriver(Dump &dump, ChipType chipType = NONE, uint8_t flags = 0)
	: Driver(dump) {
		//dump.clearIdentifiers();
		//dump.clearKey();
		//dump.clearData();

		dump.chipType = chipType;
		dump.flags    = flags;
	}

	DriverError readSystemID(void);
};

class [[gnu::packed]] X76Driver : public CartDriver {
protected:
	DriverError _x76Command(
		uint8_t pollByte, uint8_t cmd, int param = -1
	) const;

public:
	inline X76Driver(Dump &dump, ChipType chipType)
	: CartDriver(dump, chipType) {}

	DriverError readCartID(void);
};

class [[gnu::packed]] X76F041Driver : public X76Driver {
public:
	inline X76F041Driver(Dump &dump)
	: X76Driver(dump, X76F041) {}

	DriverError readPrivateData(void);
	DriverError writeData(void);
	DriverError erase(void);
	DriverError setDataKey(const uint8_t *key);
};

class [[gnu::packed]] X76F100Driver : public X76Driver {
public:
	inline X76F100Driver(Dump &dump)
	: X76Driver(dump, X76F100) {}

	DriverError readPrivateData(void);
	DriverError writeData(void);
	DriverError erase(void);
	DriverError setDataKey(const uint8_t *key);
};

class [[gnu::packed]] ZS01Driver : public CartDriver {
private:
	uint8_t _encoderState;

	DriverError _transact(zs01::Packet &request, zs01::Packet &response);

public:
	inline ZS01Driver(Dump &dump)
	: CartDriver(dump, ZS01, DUMP_HAS_CART_ID), _encoderState(0) {}

	DriverError readCartID(void);
	DriverError readPublicData(void);
	DriverError readPrivateData(void);
	DriverError writeData(void);
	DriverError erase(void);
	DriverError setDataKey(const uint8_t *key);
};

extern const char *const DRIVER_ERROR_NAMES[];

static inline const char *getErrorString(DriverError error) {
	return DRIVER_ERROR_NAMES[error];
}

CartDriver *newCartDriver(Dump &dump);

}
