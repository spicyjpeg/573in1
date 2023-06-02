
#include <stddef.h>
#include <stdint.h>
#include "ps1/system.h"
#include "vendor/miniz.h"
#include "cartio.hpp"
#include "io.hpp"
#include "util.hpp"
#include "zs01.hpp"

namespace cart {

/* Common data structures */

void Identifier::updateChecksum(void) {
	data[7] = (util::sum(data, 7) & 0xff) ^ 0xff;
}

bool Identifier::validateChecksum(void) const {
	uint8_t value = (util::sum(data, 7) & 0xff) ^ 0xff;

	if (value != data[7]) {
		LOG("checksum mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
		return false;
	}

	return true;
}

void Identifier::updateDSCRC(void) {
	data[7] = util::dsCRC8(data, 7);
}

bool Identifier::validateDSCRC(void) const {
	uint8_t value = util::dsCRC8(data, 7);

	if (value != data[7]) {
		LOG("CRC mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
		return false;
	}

	return true;
}

/* Dump structure and utilities */

const ChipSize CHIP_SIZES[NUM_CHIP_TYPES]{
	{ .dataLength =   0, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 512, .publicDataOffset = 384, .publicDataLength = 128 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =  32 }
};

size_t Dump::toQRString(char *output) const {
	uint8_t compressed[MAX_QR_STRING_LENGTH];
	size_t  uncompLength = getDumpLength();
	size_t  compLength   = MAX_QR_STRING_LENGTH;

	int error = mz_compress2(
		compressed, reinterpret_cast<mz_ulong *>(&compLength),
		reinterpret_cast<const uint8_t *>(this), uncompLength,
		MZ_BEST_COMPRESSION
	);

	if (error != MZ_OK) {
		LOG("compression error, code=%d", error);
		return 0;
	}
	LOG(
		"dump compressed, size=%d, ratio=%d%%", compLength,
		compLength * 100 / uncompLength
	);

	compLength = util::encodeBase45(&output[5], compressed, compLength);
	__builtin_memcpy(&output[0], "573::", 5);
	__builtin_memcpy(&output[compLength + 5], "::", 3);

	return compLength + 7;
}

/* Functions common to all cartridge drivers */

static constexpr int _X76_MAX_ACK_POLLS = 5;
static constexpr int _X76_WRITE_DELAY   = 10000;
static constexpr int _ZS01_PACKET_DELAY = 30000;

CartError Cart::readSystemID(void) {
	auto mask = setInterruptMask(0);

	if (!io::dsDIOReset()) {
		if (mask)
			setInterruptMask(mask);

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_SYSTEM_ID;

	io::dsDIOWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		_dump.systemID.data[i] = io::dsDIOReadByte();

	if (mask)
		setInterruptMask(mask);
	if (!_dump.systemID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_SYSTEM_ID_OK;
	return NO_ERROR;
}

CartError X76Cart::readCartID(void) {
	auto mask = setInterruptMask(0);

	if (!io::dsCartReset()) {
		if (mask)
			setInterruptMask(mask);

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	_dump.flags |= DUMP_HAS_CART_ID;

	io::dsCartWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		_dump.cartID.data[i] = io::dsCartReadByte();

	if (mask)
		setInterruptMask(mask);
	if (!_dump.cartID.validateDSCRC())
		return DS2401_ID_ERROR;

	_dump.flags |= DUMP_CART_ID_OK;
	return NO_ERROR;
}

CartError X76Cart::_x76Command(
	uint8_t cmd, uint8_t param, uint8_t pollByte
) const {
	io::i2cStartWithCS();

	io::i2cWriteByte(cmd);
	if (!io::i2cGetACK()) {
		io::i2cStopWithCS();
		LOG("NACK while sending command");
		return X76_NACK;
	}

	io::i2cWriteByte(param);
	if (!io::i2cGetACK()) {
		io::i2cStopWithCS();
		LOG("NACK while sending parameter");
		return X76_NACK;
	}

	if (!io::i2cWriteBytes(_dump.dataKey, sizeof(_dump.dataKey))) {
		io::i2cStopWithCS();
		LOG("NACK while sending data key");
		return X76_NACK;
	}

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
	_X76F041_CFG_ERASE        = 0x70
};

CartError X76F041Cart::readPrivateData(void) {
	// Reads can be done with any block size, but a single read operation can't
	// cross 128-byte block boundaries.
	for (int i = 0; i < 512; i += 128) {
		auto error = _x76Command(
			_X76F041_READ | (i >> 8), i & 0xff, _X76F041_ACK_POLL
		);
		if (error)
			return error;

		io::i2cReadByte(); // Ignore "secure read setup" byte
		io::i2cStart();

		io::i2cWriteByte(i & 0xff);
		if (io::i2cGetACK()) {
			LOG("NACK after resending address");
			return X76_NACK;
		}

		io::i2cReadBytes(&_dump.data[i], 128);
		io::i2cStopWithCS();
	}

	return NO_ERROR;
}

CartError X76F041Cart::writeData(void) {
	// Writes can only be done in 8-byte blocks.
	for (int i = 0; i < 512; i += 8) {
		auto error = _x76Command(
			_X76F041_WRITE | (i >> 8), i & 0xff, _X76F041_ACK_POLL
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(&_dump.data[i], 8)) {
			LOG("NACK while sending data bytes");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	return NO_ERROR;
}

CartError X76F041Cart::erase(void) {
	auto error = _x76Command(
		_X76F041_CONFIG, _X76F041_CFG_ERASE, _X76F041_ACK_POLL
	);
	if (error)
		return error;

	io::i2cStopWithCS(_X76_WRITE_DELAY);
	return NO_ERROR;
}

CartError X76F041Cart::setDataKey(const uint8_t *key) {
	auto error = _x76Command(
		_X76F041_CONFIG, _X76F041_CFG_SET_DATA_KEY, _X76F041_ACK_POLL
	);
	if (error)
		return error;

	// The X76F041 requires the key to be sent twice as a way of ensuring it
	// gets received correctly.
	for (int i = 2; i; i--) {
		if (!io::i2cWriteBytes(key, sizeof(_dump.dataKey))) {
			io::i2cStopWithCS();
			LOG("NACK while setting new data key");
			return X76_NACK;
		}
	}

	io::i2cStopWithCS(_X76_WRITE_DELAY);

	// Update the data key stored in the dump.
	__builtin_memcpy(_dump.dataKey, key, sizeof(_dump.dataKey));
	return NO_ERROR;
}

/* X76F100 driver */

enum X76F100Command : uint8_t {
	_X76F100_READ          = 0x81,
	_X76F100_WRITE         = 0x80,
	_X76F100_SET_READ_KEY  = 0xfe,
	_X76F100_SET_WRITE_KEY = 0xfc,
	_X76F100_ACK_POLL      = 0x55
};

// TODO: actually implement this (even though no X76F100 carts were ever made)

CartError X76F100Cart::readPrivateData(void) {
	return UNSUPPORTED_OP;
}

CartError X76F100Cart::writeData(void) {
	return UNSUPPORTED_OP;
}

CartError X76F100Cart::erase(void) {
	return UNSUPPORTED_OP;
}

CartError X76F100Cart::setDataKey(const uint8_t *key) {
	return UNSUPPORTED_OP;
}

/* ZS01 driver */

CartError ZS01Cart::_transact(zs01::Packet &request, zs01::Packet &response) {
	io::i2cStart();
	
	if (!io::i2cWriteBytes(
		&request.command, sizeof(zs01::Packet), _ZS01_PACKET_DELAY
	)) {
		io::i2cStop();
		LOG("NACK while sending request packet");
		return ZS01_NACK;
	}

	io::i2cReadBytes(&response.command, sizeof(zs01::Packet));
	io::i2cStop();

	if (!response.decodeResponse())
		return ZS01_CRC_MISMATCH;

	_encoderState = response.address;

	if (response.command != zs01::RESP_NO_ERROR) {
		LOG("ZS01 error, code=0x%02x", response.command);
		return ZS01_ERROR;
	}

	return NO_ERROR;
}

CartError ZS01Cart::readCartID(void) {
	zs01::Packet request, response;
	CartError    error;

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

CartError ZS01Cart::readPublicData(void) {
	zs01::Packet request, response;

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PUBLIC_END; i++) {
		request.address = i;
		request.encodeReadRequest();

		CartError error = _transact(request, response);
		if (error)
			return error;

		response.copyTo(&_dump.data[i * sizeof(response.data)]);
	}

	_dump.flags |= DUMP_PUBLIC_DATA_OK;
	return NO_ERROR;
}

CartError ZS01Cart::readPrivateData(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = zs01::ADDR_PRIVATE; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.encodeReadRequest(key, _encoderState);

		CartError error = _transact(request, response);
		if (error)
			return error;

		response.copyTo(&_dump.data[i * sizeof(response.data)]);
	}

	_dump.flags |= DUMP_PRIVATE_DATA_OK;

	request.address = zs01::ADDR_CONFIG;
	request.encodeReadRequest(key, _encoderState);

	CartError error = _transact(request, response);
	if (error)
		return error;

	response.copyTo(_dump.config);

	_dump.flags |= DUMP_CONFIG_OK;
	return NO_ERROR;
}

CartError ZS01Cart::writeData(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.copyFrom(&_dump.data[i * sizeof(request.data)]);
		request.encodeWriteRequest(key, _encoderState);

		CartError error = _transact(request, response);
		if (error)
			return error;
	}

	request.address = zs01::ADDR_CONFIG;
	request.copyFrom(_dump.config);
	request.encodeWriteRequest(key, _encoderState);

	return _transact(request, response);
}

CartError ZS01Cart::erase(void) {
	zs01::Packet request, response;
	zs01::Key    key;

	key.unpackFrom(_dump.dataKey);

	__builtin_memset(request.data, 0, sizeof(request.data));
	request.address = zs01::ADDR_ERASE;
	request.encodeWriteRequest(key, _encoderState);

	return _transact(request, response);
}

CartError ZS01Cart::setDataKey(const uint8_t *key) {
	zs01::Packet request, response;
	zs01::Key    newKey;

	newKey.unpackFrom(_dump.dataKey);

	request.address = zs01::ADDR_DATA_KEY;
	request.copyFrom(key);
	request.encodeWriteRequest(newKey, _encoderState);

	CartError error = _transact(request, response);
	if (error)
		return error;

	// Update the data key stored in the dump.
	__builtin_memcpy(_dump.dataKey, key, sizeof(_dump.dataKey));
	return NO_ERROR;
}

/* Cartridge identification */

enum ChipIdentifier : uint32_t {
	_ID_X76F041 = 0x55aa5519,
	_ID_X76F100 = 0x55aa0019,
	_ID_ZS01    = 0x5a530001
};

Cart *createCart(Dump &dump) {
	if (!io::getCartInsertionStatus()) {
		LOG("DSR not asserted");
		return new Cart(dump);
	}

	uint32_t id1 = io::i2cResetZS01();
	LOG("id1=0x%08x", id1);

	if (id1 == _ID_ZS01)
		return new ZS01Cart(dump);

	uint32_t id2 = io::i2cResetX76();
	LOG("id2=0x%08x", id2);

	switch (id2) {
		case _ID_X76F041:
			return new X76F041Cart(dump);

		//case _ID_X76F100:
			//return new X76F100Cart(dump);

		default:
			return new Cart(dump);
	}
}

}
