
#include <stdint.h>
#include "common/io.hpp"
#include "common/util.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartio.hpp"
#include "main/cart/zs01.hpp"
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

CartDump dummyDriverDump;

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
		util::clear(dummyDriverDump.data);
		util::clear(dummyDriverDump.dataKey);
		// TODO: clear config registers as well

		util::clear(_dump.dataKey);
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

enum DS2401Command : uint8_t {
	_DS2401_READ_ROM   = 0x33,
	_DS2401_MATCH_ROM  = 0x55,
	_DS2401_SKIP_ROM   = 0xcc,
	_DS2401_SEARCH_ROM = 0xf0
};

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

	if (!io::digitalIODS2401.reset()) {
		if (enable)
			enableInterrupts();

		LOG_CART_IO("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_SYSTEM_ID;

	io::digitalIODS2401.writeByte(_DS2401_READ_ROM);
	for (int i = 0; i < 8; i++)
		_dump.systemID.data[i] = io::digitalIODS2401.readByte();

	if (enable)
		enableInterrupts();
	if (!_dump.systemID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_SYSTEM_ID_OK;
	return NO_ERROR;
}

DriverError X76Driver::readCartID(void) {
	auto enable = disableInterrupts();

	if (!io::cartDS2401.reset()) {
		if (enable)
			enableInterrupts();

		LOG_CART_IO("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_CART_ID;

	io::cartDS2401.writeByte(_DS2401_READ_ROM);
	for (int i = 0; i < 8; i++)
		_dump.cartID.data[i] = io::cartDS2401.readByte();

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
	io::cartI2C.startWithCS();

	io::cartI2C.writeByte(cmd);
	if (!io::cartI2C.getACK()) {
		io::cartI2C.stopWithCS();
		LOG_CART_IO("NACK while sending cmd=0x%02x", cmd);
		return X76_NACK;
	}

	if (param >= 0) {
		io::cartI2C.writeByte(param);
		if (!io::cartI2C.getACK()) {
			io::cartI2C.stopWithCS();
			LOG_CART_IO("NACK while sending param=0x%02x", param);
			return X76_NACK;
		}
	}

	if (!io::cartI2C.writeBytes(_dump.dataKey, sizeof(_dump.dataKey))) {
		io::cartI2C.stopWithCS();
		LOG_CART_IO("NACK while sending data key");
		return X76_NACK;
	}

#if 0
	char buffer[48];

	util::hexToString(buffer, _dump.dataKey, sizeof(_dump.dataKey), ' ');
	if (param >= 0)
		LOG_CART_IO("S: %02X %02X %s", cmd, param, buffer);
	else
		LOG_CART_IO("S: %02X %s", cmd, buffer);
#endif

	for (int i = _X76_MAX_ACK_POLLS; i; i--) {
		delayMicroseconds(_X76_WRITE_DELAY);
		io::cartI2C.start();
		io::cartI2C.writeByte(pollByte);
		if (io::cartI2C.getACK())
			return NO_ERROR;
	}

	io::cartI2C.stopWithCS();
	LOG_CART_IO("ACK polling timeout (wrong key?)");
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

		io::cartI2C.readByte(); // Ignore "secure read setup" byte
		io::cartI2C.start();

		io::cartI2C.writeByte(i & 0xff);
		if (!io::cartI2C.getACK()) {
			io::cartI2C.stopWithCS();
			LOG_CART_IO("NACK after resending addr=0x%02x", i & 0xff);
			return X76_NACK;
		}

		io::cartI2C.readBytes(&_dump.data[i], 128);
		io::cartI2C.stopWithCS();
	}

	_dump.flags |= DUMP_PRIVATE_DATA_OK;

	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_READ_CONFIG
	);
	if (error)
		return error;

	util::clear(_dump.config);
	io::cartI2C.readBytes(_dump.config, 5);
	io::cartI2C.stopWithCS();

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

		auto ok = io::cartI2C.writeBytes(&_dump.data[i], 8);
		io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART_IO("NACK while sending data bytes");
			return X76_NACK;
		}
	}

	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_WRITE_CONFIG
	);
	if (error)
		return error;

	auto ok = io::cartI2C.writeBytes(_dump.config, 5);
	io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

	if (!ok) {
		LOG_CART_IO("NACK while sending config registers");
		return X76_NACK;
	}

	return NO_ERROR;
}

DriverError X76F041Driver::erase(void) {
	auto error = _x76Command(
		_X76F041_ACK_POLL, _X76F041_CONFIG, _X76F041_CFG_MASS_PROGRAM
	);
	if (error)
		return error;

	io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

	util::clear(_dump.dataKey);
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
		if (!io::cartI2C.writeBytes(key, sizeof(_dump.dataKey))) {
			io::cartI2C.stopWithCS(_X76_WRITE_DELAY);
			LOG_CART_IO("NACK while setting new data key");
			return X76_NACK;
		}
	}

	io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

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

	//io::cartI2C.Start();
	io::cartI2C.readBytes(_dump.data, 112);
	io::cartI2C.stopWithCS();

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

		auto ok = io::cartI2C.writeBytes(&_dump.data[i], 8);
		io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART_IO("NACK while sending data bytes");
			return X76_NACK;
		}
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

		auto ok = io::cartI2C.writeBytes(dummy, 8);
		io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART_IO("NACK while sending data bytes");
			return X76_NACK;
		}
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

		auto ok = io::cartI2C.writeBytes(key, sizeof(_dump.dataKey));
		io::cartI2C.stopWithCS(_X76_WRITE_DELAY);

		if (!ok) {
			LOG_CART_IO("NACK while setting new data key");
			return X76_NACK;
		}
	}

	_dump.copyKeyFrom(key);
	return NO_ERROR;
}

/* ZS01 driver */

DriverError ZS01Driver::_transact(ZS01Packet &request, ZS01Packet &response) {
	delayMicroseconds(_ZS01_PACKET_DELAY);
	io::cartI2C.start();

#if 0
	char buffer[48];

	util::hexToString(buffer, &request.command, sizeof(ZS01Packet), ' ');
	LOG_CART_IO("S: %s", buffer);
#endif

	if (!io::cartI2C.writeBytes(
		&request.command, sizeof(ZS01Packet), _ZS01_SEND_DELAY
	)) {
		io::cartI2C.stop();
		LOG_CART_IO("NACK while sending request packet");
		return ZS01_NACK;
	}

	io::cartI2C.readBytes(&response.command, sizeof(ZS01Packet));
	io::cartI2C.stop();

#if 0
	util::hexToString(buffer, &response.command, sizeof(ZS01Packet), ' ');
	LOG_CART_IO("R: %s", buffer);
#endif

	bool ok = response.decodeResponse();

#if 0
	util::hexToString(buffer, &response.command, sizeof(ZS01Packet), ' ');
	LOG_CART_IO("D: %s", buffer);
#endif

	if (!ok)
		return ZS01_CRC_MISMATCH;

	_encoderState = response.address;

	if (response.command != ZS01_RESP_NO_ERROR) {
		LOG_CART_IO("ZS01 error, code=0x%02x", response.command);
		return ZS01_ERROR;
	}

	return NO_ERROR;
}

DriverError ZS01Driver::readCartID(void) {
	ZS01Packet  request, response;
	DriverError error;

	request.address = ZS01_ADDR_ZS01_ID;
	request.encodeReadRequest();

	error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.zsID.data);
	if (!_dump.zsID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_ZS_ID_OK;

	request.address = ZS01_ADDR_DS2401_ID;
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
	ZS01Packet request, response;

	for (int i = ZS01_ADDR_PUBLIC; i < ZS01_ADDR_PUBLIC_END; i++) {
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
	ZS01Packet request, response;
	ZS01Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = ZS01_ADDR_PRIVATE; i < ZS01_ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.encodeReadRequest(key, _encoderState);

		DriverError error = _transact(request, response);
		if (error)
			return error;

		response.copyTo(&_dump.data[i * sizeof(response.data)]);
	}

	_dump.flags |= DUMP_PRIVATE_DATA_OK;

	request.address = ZS01_ADDR_CONFIG;
	request.encodeReadRequest(key, _encoderState);

	DriverError error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.config);

	_dump.flags |= DUMP_CONFIG_OK;
	return NO_ERROR;
}

DriverError ZS01Driver::writeData(void) {
	ZS01Packet request, response;
	ZS01Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = ZS01_ADDR_PUBLIC; i < ZS01_ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.copyFrom(&_dump.data[i * sizeof(request.data)]);
		request.encodeWriteRequest(key, _encoderState);

		DriverError error = _transact(request, response);
		if (error)
			return error;
	}

	request.address = ZS01_ADDR_CONFIG;
	request.copyFrom(_dump.config);
	request.encodeWriteRequest(key, _encoderState);

	return _transact(request, response);
}

DriverError ZS01Driver::erase(void) {
	ZS01Packet request, response;
	ZS01Key    key;

	key.unpackFrom(_dump.dataKey);

	util::clear(request.data);
	request.address = ZS01_ADDR_ERASE;
	request.encodeWriteRequest(key, _encoderState);

	DriverError error = _transact(request, response);
	if (error)
		return error;

	util::clear(_dump.dataKey);
	return NO_ERROR;
}

DriverError ZS01Driver::setDataKey(const uint8_t *key) {
	ZS01Packet request, response;
	ZS01Key    oldKey;

	oldKey.unpackFrom(_dump.dataKey);

	request.address = ZS01_ADDR_DATA_KEY;
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

CartDriver *newCartDriver(CartDump &dump) {
	if (!io::getCartInsertionStatus()) {
		LOG_CART_IO("DSR not asserted");
		return new CartDriver(dump);
	}

#ifdef ENABLE_ZS01_CART_DRIVER
	auto id1 = io::cartI2C.resetZS01();
	LOG_CART_IO("detecting ZS01: 0x%08x", id1);

	if (id1 == _ID_ZS01)
		return new ZS01Driver(dump);
#endif

	auto id2 = io::cartI2C.resetX76();
	LOG_CART_IO("detecting X76: 0x%08x", id2);

	switch (id2) {
#ifdef ENABLE_X76F041_CART_DRIVER
		case _ID_X76F041:
			return new X76F041Driver(dump);
#endif

#ifdef ENABLE_X76F100_CART_DRIVER
		case _ID_X76F100:
			return new X76F100Driver(dump);
#endif

		default:
			return new CartDriver(dump);
	}
}

}
