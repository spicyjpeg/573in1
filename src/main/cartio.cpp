
#include <stdint.h>
#include "common/io.hpp"
#include "main/cart.hpp"
#include "main/cartio.hpp"
#include "main/zs01.hpp"
#include "ps1/system.h"

namespace cart {

const char *const DRIVER_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"DS2401_NO_RESP",
	"DS2401_ID_ERROR",
	"X76_NACK",
	"X76_POLL_FAIL",
	"X76_VERIFY_FAIL",
	"ZS01_NACK",
	"ZS01_ERROR",
	"ZS01_CRC_MISMATCH"
};

/* Dummy cartridge driver */

Dump dummyDriverDump;

DriverError DummyDriver::readSystemID(void) {
	if (dummyDriverDump.flags & DUMP_SYSTEM_ID_OK) {
		_dump.systemID.copyFrom(dummyDriverDump.systemID.data);
		_dump.flags |= DUMP_SYSTEM_ID_OK;
		return NO_ERROR;
	}

	return DS2401_NO_RESP;
}

DriverError DummyDriver::readCartID(void) {
	if (dummyDriverDump.flags & DUMP_ZS_ID_OK) {
		_dump.zsID.copyFrom(dummyDriverDump.zsID.data);
		_dump.flags |= DUMP_ZS_ID_OK;
	}
	if (dummyDriverDump.flags & DUMP_CART_ID_OK) {
		_dump.cartID.copyFrom(dummyDriverDump.cartID.data);
		_dump.flags |= DUMP_CART_ID_OK;
		return NO_ERROR;
	}

	return DS2401_NO_RESP;
}

DriverError DummyDriver::readPublicData(void) {
	if (dummyDriverDump.chipType != ZS01)
		return UNSUPPORTED_OP;

	if (dummyDriverDump.flags & DUMP_PUBLIC_DATA_OK) {
		_dump.copyDataFrom(dummyDriverDump.data);
		_dump.flags |= DUMP_PUBLIC_DATA_OK;
		return NO_ERROR;
	}

	return _getErrorCode();
}

DriverError DummyDriver::readPrivateData(void) {
	if ((dummyDriverDump.flags & DUMP_PRIVATE_DATA_OK) && !__builtin_memcmp(
		_dump.dataKey, dummyDriverDump.dataKey, sizeof(_dump.dataKey)
	)) {
		_dump.copyDataFrom(dummyDriverDump.data);
		_dump.copyConfigFrom(dummyDriverDump.config);
		_dump.flags |= DUMP_PRIVATE_DATA_OK | DUMP_CONFIG_OK;
		return NO_ERROR;
	}

	return _getErrorCode();
}

DriverError DummyDriver::writeData(void) {
	if (!__builtin_memcmp(
		_dump.dataKey, dummyDriverDump.dataKey, sizeof(_dump.dataKey)
	)) {
		dummyDriverDump.copyDataFrom(_dump.data);
		return NO_ERROR;
	}

	return _getErrorCode();
}

DriverError DummyDriver::erase(void) {
	if (!__builtin_memcmp(
		_dump.dataKey, dummyDriverDump.dataKey, sizeof(_dump.dataKey)
	)) {
		dummyDriverDump.clearData();
		dummyDriverDump.clearKey();
		// TODO: clear config registers as well

		_dump.clearKey();
		return NO_ERROR;
	}

	return _getErrorCode();
}

DriverError DummyDriver::setDataKey(const uint8_t *key) {
	if (!__builtin_memcmp(
		_dump.dataKey, dummyDriverDump.dataKey, sizeof(_dump.dataKey)
	)) {
		dummyDriverDump.copyKeyFrom(key);

		_dump.copyKeyFrom(key);
		return NO_ERROR;
	}

	return _getErrorCode();
}

/* Functions common to all cartridge drivers */

// TODO: _ZS01_SEND_DELAY and _ZS01_PACKET_DELAY could be tweaked to make the
// tool faster, however setting both to 30000 results in bricked carts when
// attempting to reflash.
static constexpr int _X76_MAX_ACK_POLLS = 5;
static constexpr int _X76_WRITE_DELAY   = 12000;
static constexpr int _X76_PACKET_DELAY  = 12000;
static constexpr int _ZS01_SEND_DELAY   = 100000;
static constexpr int _ZS01_PACKET_DELAY = 300000;

DriverError CartDriver::readSystemID(void) {
	auto enable = disableInterrupts();

	if (!io::dsDIOReset()) {
		if (enable)
			enableInterrupts();

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_SYSTEM_ID;

	io::dsDIOWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		_dump.systemID.data[i] = io::dsDIOReadByte();

	if (enable)
		enableInterrupts();
	if (!_dump.systemID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_SYSTEM_ID_OK;
	return NO_ERROR;
}

DriverError X76Driver::readCartID(void) {
	auto enable = disableInterrupts();

	if (!io::dsCartReset()) {
		if (enable)
			enableInterrupts();

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_CART_ID;

	io::dsCartWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		_dump.cartID.data[i] = io::dsCartReadByte();

	if (enable)
		enableInterrupts();
	if (!_dump.cartID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_CART_ID_OK;
	return NO_ERROR;
}

DriverError X76Driver::_x76Command(
	uint8_t pollByte, uint8_t cmd, int param
) const {
	delayMicroseconds(_X76_PACKET_DELAY);
	io::i2cStartWithCS();

	io::i2cWriteByte(cmd);
	if (!io::i2cGetACK()) {
		io::i2cStopWithCS();
		LOG("NACK while sending cmd=0x%02x", cmd);
		return X76_NACK;
	}

	if (param >= 0) {
		io::i2cWriteByte(param);
		if (!io::i2cGetACK()) {
			io::i2cStopWithCS();
			LOG("NACK while sending param=0x%02x", param);
			return X76_NACK;
		}
	}

	if (!io::i2cWriteBytes(_dump.dataKey, sizeof(_dump.dataKey))) {
		io::i2cStopWithCS();
		LOG("NACK while sending data key");
		return X76_NACK;
	}

#ifdef ENABLE_I2C_LOGGING
	char buffer[48];

	util::hexToString(buffer, _dump.dataKey, sizeof(_dump.dataKey), ' ');
	if (param >= 0)
		LOG("S: %02X %02X %s", cmd, param, buffer);
	else
		LOG("S: %02X %s", cmd, buffer);
#endif

	for (int i = _X76_MAX_ACK_POLLS; i; i--) {
		delayMicroseconds(_X76_WRITE_DELAY);
		io::i2cStart();
		io::i2cWriteByte(pollByte);
		if (io::i2cGetACK())
			return NO_ERROR;
	}

	io::i2cStopWithCS();
	LOG("ACK polling timeout (wrong key?)");
	return X76_POLL_FAIL;
}

/* X76F041 driver */

enum X76F041Command : uint8_t {
	_X76F041_READ     = 0x60,
	_X76F041_WRITE    = 0x40,
	_X76F041_CONFIG   = 0x80,
	_X76F041_ACK_POLL = 0xc0
};

enum X76F041ConfigOp : uint8_t {
	_X76F041_CFG_SET_DATA_KEY = 0x20,
	_X76F041_CFG_READ_CONFIG  = 0x60,
	_X76F041_CFG_WRITE_CONFIG = 0x50,
	_X76F041_CFG_MASS_PROGRAM = 0x70
};

DriverError X76F041Driver::readPrivateData(void) {
	// Reads can be done with any block size, but a single read operation can't
	// cross 128-byte block boundaries.
	for (int i = 0; i < 512; i += 128) {
		auto error = _x76Command(
			_X76F041_ACK_POLL, _X76F041_READ | (i >> 8), i & 0xff
		);
		if (error)
			return error;

		io::i2cReadByte(); // Ignore "secure read setup" byte
		io::i2cStart();

		io::i2cWriteByte(i & 0xff);
		if (!io::i2cGetACK()) {
			io::i2cStopWithCS();
			LOG("NACK after resending addr=0x%02x", i & 0xff);
			return X76_NACK;
		}

		io::i2cReadBytes(&_dump.data[i], 128);
		io::i2cStopWithCS();
	}

	_dump.flags |= DUMP_PRIVATE_DATA_OK;

	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_READ_CONFIG
	);
	if (error)
		return error;

	io::i2cReadByte();
	io::i2cStart();

	io::i2cWriteByte(0);
	if (!io::i2cGetACK()) {
		io::i2cStopWithCS();
		LOG("NACK after resending dummy byte");
		return X76_NACK;
	}

	io::i2cReadBytes(_dump.config, 8);
	io::i2cStopWithCS();

	_dump.flags |= DUMP_CONFIG_OK;
	return NO_ERROR;
}

DriverError X76F041Driver::writeData(void) {
	// Writes can only be done in 8-byte blocks.
	for (int i = 0; i < 512; i += 8) {
		auto error = _x76Command(
			_X76F041_ACK_POLL, _X76F041_WRITE | (i >> 8), i & 0xff
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(&_dump.data[i], 8)) {
			io::i2cStopWithCS(_X76_WRITE_DELAY);
			LOG("NACK while sending data bytes");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_WRITE_CONFIG
	);
	if (error)
		return error;

	if (!io::i2cWriteBytes(_dump.config, 8)) {
		io::i2cStopWithCS(_X76_WRITE_DELAY);
		LOG("NACK while sending data bytes");
		return X76_NACK;
	}

	io::i2cStopWithCS(_X76_WRITE_DELAY);

	return NO_ERROR;
}

DriverError X76F041Driver::erase(void) {
	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_MASS_PROGRAM
	);
	if (error)
		return error;

	io::i2cStopWithCS(_X76_WRITE_DELAY);

	_dump.clearKey();
	return NO_ERROR;
}

DriverError X76F041Driver::setDataKey(const uint8_t *key) {
	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_SET_DATA_KEY
	);
	if (error)
		return error;

	// The X76F041 requires the key to be sent twice as a way of ensuring it
	// gets received correctly.
	for (int i = 2; i; i--) {
		if (!io::i2cWriteBytes(key, sizeof(_dump.dataKey))) {
			io::i2cStopWithCS(_X76_WRITE_DELAY);
			LOG("NACK while setting new data key");
			return X76_NACK;
		}
	}

	io::i2cStopWithCS(_X76_WRITE_DELAY);

	_dump.copyKeyFrom(key);
	return NO_ERROR;
}

/* X76F100 driver */

enum X76F100Command : uint8_t {
	_X76F100_READ     = 0x81,
	_X76F100_WRITE    = 0x80,
	_X76F100_SET_KEY  = 0xfc,
	_X76F100_ACK_POLL = 0x55
};

DriverError X76F100Driver::readPrivateData(void) {
	auto error = _x76Command(_X76F100_ACK_POLL, _X76F100_READ);
	if (error)
		return error;

	//io::i2cStart();
	io::i2cReadBytes(_dump.data, 112);
	io::i2cStopWithCS();

	_dump.flags |= DUMP_PRIVATE_DATA_OK;
	return NO_ERROR;
}

DriverError X76F100Driver::writeData(void) {
	// Writes can only be done in 8-byte blocks.
	for (int i = 0; i < 112; i += 8) {
		auto error = _x76Command(
			_X76F100_ACK_POLL, _X76F100_WRITE | (i >> 2)
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(&_dump.data[i], 8)) {
			io::i2cStopWithCS(_X76_WRITE_DELAY);
			LOG("NACK while sending data bytes");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	return NO_ERROR;
}

DriverError X76F100Driver::erase(void) {
	// The chip does not have an erase command, so erasing must be performed
	// manually one block at a time.
	uint8_t dummy[8]{ 0, 0, 0, 0, 0, 0, 0, 0 };

	for (int i = 0; i < 112; i += 8) {
		auto error = _x76Command(
			_X76F100_ACK_POLL, _X76F100_WRITE | (i >> 2)
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(dummy, 8)) {
			io::i2cStopWithCS(_X76_WRITE_DELAY);
			LOG("NACK while sending data bytes");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	return setDataKey(dummy);
}

DriverError X76F100Driver::setDataKey(const uint8_t *key) {
	// There are two separate keys, one for read commands and one for write
	// commands.
	for (int i = 0; i < 2; i++) {
		auto error = _x76Command(
			_X76F100_ACK_POLL, _X76F100_SET_KEY | (i << 1)
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(key, sizeof(_dump.dataKey))) {
			io::i2cStopWithCS(_X76_WRITE_DELAY);
			LOG("NACK while setting new data key");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	_dump.copyKeyFrom(key);
	return NO_ERROR;
}

/* ZS01 driver */

DriverError ZS01Driver::_transact(
	zs01::Packet &request, zs01::Packet &response
) {
	delayMicroseconds(_ZS01_PACKET_DELAY);
	io::i2cStart();

#ifdef ENABLE_I2C_LOGGING
	char buffer[48];

	util::hexToString(buffer, &request.command, sizeof(zs01::Packet), ' ');
	LOG("S: %s", buffer);
#endif

	if (!io::i2cWriteBytes(
		&request.command, sizeof(zs01::Packet), _ZS01_SEND_DELAY
	)) {
		io::i2cStop();
		LOG("NACK while sending request packet");
		return ZS01_NACK;
	}

	io::i2cReadBytes(&response.command, sizeof(zs01::Packet));
	io::i2cStop();

#ifdef ENABLE_I2C_LOGGING
	util::hexToString(buffer, &response.command, sizeof(zs01::Packet), ' ');
	LOG("R: %s", buffer);
#endif

	bool ok = response.decodeResponse();

#ifdef ENABLE_I2C_LOGGING
	util::hexToString(buffer, &response.command, sizeof(zs01::Packet), ' ');
	LOG("D: %s", buffer);
#endif

	if (!ok)
		return ZS01_CRC_MISMATCH;

	_encoderState = response.address;

	if (response.command != zs01::RESP_NO_ERROR) {
		LOG("ZS01 error, code=0x%02x", response.command);
		return ZS01_ERROR;
	}

	return NO_ERROR;
}

DriverError ZS01Driver::readCartID(void) {
	zs01::Packet request, response;
	DriverError  error;

	request.address = zs01::ADDR_ZS01_ID;
	request.encodeReadRequest();

	error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.zsID.data);
	if (!_dump.zsID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_ZS_ID_OK;

	request.address = zs01::ADDR_DS2401_ID;
	request.encodeReadRequest();

	error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.cartID.data);
	if (!_dump.cartID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_CART_ID_OK;
	return NO_ERROR;
}

DriverError ZS01Driver::readPublicData(void) {
	zs01::Packet request, response;

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PUBLIC_END; i++) {
		request.address = i;
		request.encodeReadRequest();

		DriverError error = _transact(request, response);
		if (error)
			return error;

		response.copyTo(&_dump.data[i * sizeof(response.data)]);
	}

	_dump.flags |= DUMP_PUBLIC_DATA_OK;
	return NO_ERROR;
}

DriverError ZS01Driver::readPrivateData(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = zs01::ADDR_PRIVATE; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.encodeReadRequest(key, _encoderState);

		DriverError error = _transact(request, response);
		if (error)
			return error;

		response.copyTo(&_dump.data[i * sizeof(response.data)]);
	}

	_dump.flags |= DUMP_PRIVATE_DATA_OK;

	request.address = zs01::ADDR_CONFIG;
	request.encodeReadRequest(key, _encoderState);

	DriverError error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.config);

	_dump.flags |= DUMP_CONFIG_OK;
	return NO_ERROR;
}

DriverError ZS01Driver::writeData(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.copyFrom(&_dump.data[i * sizeof(request.data)]);
		request.encodeWriteRequest(key, _encoderState);

		DriverError error = _transact(request, response);
		if (error)
			return error;
	}

	request.address = zs01::ADDR_CONFIG;
	request.copyFrom(_dump.config);
	request.encodeWriteRequest(key, _encoderState);

	return _transact(request, response);
}

DriverError ZS01Driver::erase(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	__builtin_memset(request.data, 0, sizeof(request.data));
	request.address = zs01::ADDR_ERASE;
	request.encodeWriteRequest(key, _encoderState);

	DriverError error = _transact(request, response);
	if (error)
		return error;

	_dump.clearKey();
	return NO_ERROR;
}

DriverError ZS01Driver::setDataKey(const uint8_t *key) {
	zs01::Packet request, response;
	zs01::Key    oldKey;

	oldKey.unpackFrom(_dump.dataKey);

	request.address = zs01::ADDR_DATA_KEY;
	request.copyFrom(key);
	request.encodeWriteRequest(oldKey, _encoderState);

	DriverError error = _transact(request, response);
	if (error)
		return error;

	_dump.copyKeyFrom(key);
	return NO_ERROR;
}

/* Cartridge identification */

enum ChipIdentifier : uint32_t {
	_ID_X76F041 = 0x55aa5519,
	_ID_X76F100 = 0x55aa0019,
	_ID_ZS01    = 0x5a530001
};

CartDriver *newCartDriver(Dump &dump) {
	if (!io::getCartInsertionStatus()) {
		LOG("DSR not asserted");
		return new CartDriver(dump);
	}

	uint32_t id1 = io::i2cResetZS01();
	LOG("detecting ZS01, id1=0x%08x", id1);

	if (id1 == _ID_ZS01)
		return new ZS01Driver(dump);

	uint32_t id2 = io::i2cResetX76();
	LOG("detecting X76, id2=0x%08x", id2);

	switch (id2) {
		case _ID_X76F041:
			return new X76F041Driver(dump);

#ifdef ENABLE_X76F100_DRIVER
		case _ID_X76F100:
			return new X76F100Driver(dump);
#endif

		default:
			return new CartDriver(dump);
	}
}

}
