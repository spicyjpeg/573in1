
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ps1/system.h"
#include "vendor/miniz.h"
#include "cart.hpp"
#include "io.hpp"
#include "util.hpp"
#include "zs01.hpp"

namespace cart {

/* Common functions */

enum ChipID : uint32_t {
	_ID_X76F041 = 0x55aa5519,
	_ID_X76F100 = 0x55aa0019,
	_ID_ZS01    = 0x5a530001
};

static const size_t _DATA_LENGTHS[NUM_CHIP_TYPES]{ 0, 512, 112, 112 };

static constexpr int _X76_MAX_ACK_POLLS = 5;
static constexpr int _X76_WRITE_DELAY   = 10000;
static constexpr int _ZS01_PACKET_DELAY = 30000;

static bool _validateID(const uint8_t *id) {
	if (!id[0] || (id[0] == 0xff)) {
		LOG("invalid device type 0x%02x", id[0]);
		return false;
	}

	uint8_t crc = util::dsCRC8(id, 7);
	if (crc != id[7]) {
		LOG("CRC mismatch, exp=0x%02x, got=0x%02x", crc, id[7]);
		return false;
	}

	return true;
}

size_t getDataLength(ChipType type) {
	return _DATA_LENGTHS[type];
}

Cart *createCart(void) {
	if (!io::getCartInsertionStatus()) {
		LOG("DSR not asserted");
		return new Cart();
	}

	uint32_t id1 = io::i2cResetX76();
	LOG("id1=0x%08x", id1);

	switch (id1) {
		case _ID_X76F041:
			return new X76F041Cart();

		//case _ID_X76F100:
			//return new X76F100Cart();

		default:
			uint32_t id2 = io::i2cResetZS01();
			LOG("id2=0x%08x", id2);

			if (id2 == _ID_ZS01)
				return new ZS01Cart();

			return new Cart();
	}
}

/* Base cart class */

Cart::Cart(void) {
	memset(&version, 0, 44);

	version  = DUMP_VERSION;
	chipType = TYPE_NONE;
	flags    = 0;
}

size_t Cart::toQRString(char *output) {
	uint8_t compressed[MAX_QR_STRING_LENGTH];
	size_t  uncompLength = getDumpLength();
	size_t  compLength   = MAX_QR_STRING_LENGTH;

	int error = mz_compress2(
		compressed,
		reinterpret_cast<mz_ulong *>(&compLength),
		&version,
		uncompLength,
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
	memcpy(&output[0], "573::", 5);
	memcpy(&output[compLength + 5], "::", 3);

	return compLength + 7;
}

Error Cart::readSystemID(void) {
	uint8_t id[8];
	auto    mask = setInterruptMask(0);

	if (!io::dsDIOReset()) {
		if (mask)
			setInterruptMask(mask);

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	flags |= HAS_DIGITAL_IO;

	io::dsDIOWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		id[i] = io::dsDIOReadByte();

	if (mask)
		setInterruptMask(mask);
	if (!_validateID(id))
		return DS2401_ID_ERROR;

	memcpy(systemID, id, sizeof(id));

	flags |= SYSTEM_ID_OK;
	return NO_ERROR;
}

Error X76Cart::readCartID(void) {
	uint8_t id[8];
	auto    mask = setInterruptMask(0);

	if (!io::dsCartReset()) {
		if (mask)
			setInterruptMask(mask);

		LOG("no 1-wire device found");
		return DS2401_NO_RESP;
	}

	flags |= HAS_DS2401;

	io::dsCartWriteByte(0x33);
	for (int i = 0; i < 8; i++)
		id[i] = io::dsCartReadByte();

	if (mask)
		setInterruptMask(mask);
	if (!_validateID(id))
		return DS2401_ID_ERROR;

	memcpy(cartID, id, sizeof(id));

	flags |= CART_ID_OK;
	return NO_ERROR;
}

Error X76Cart::_x76Command(
	uint8_t command, uint8_t param, uint8_t pollByte
) const {
	io::i2cStartWithCS();

	io::i2cWriteByte(command);
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

	if (!io::i2cWriteBytes(dataKey, sizeof(dataKey))) {
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

X76F041Cart::X76F041Cart(void) {
	Cart();

	chipType = TYPE_X76F041;
}

Error X76F041Cart::readPrivateData(void) {
	// Reads can be done with any block size, but a single read operation can't
	// cross 128-byte block boundaries.
	for (int i = 0; i < 512; i += 128) {
		Error error = _x76Command(
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

		io::i2cReadBytes(&data[i], 128);
		io::i2cStopWithCS();
	}

	return NO_ERROR;
}

Error X76F041Cart::writeData(void) {
	// Writes can only be done in 8-byte blocks.
	for (int i = 0; i < 512; i += 8) {
		Error error = _x76Command(
			_X76F041_WRITE | (i >> 8), i & 0xff, _X76F041_ACK_POLL
		);
		if (error)
			return error;

		if (!io::i2cWriteBytes(&data[i], 8)) {
			LOG("NACK while sending data bytes");
			return X76_NACK;
		}

		io::i2cStopWithCS(_X76_WRITE_DELAY);
	}

	return NO_ERROR;
}

Error X76F041Cart::erase(void) {
	Error error = _x76Command(
		_X76F041_CONFIG, _X76F041_CFG_ERASE, _X76F041_ACK_POLL
	);
	if (error)
		return error;

	io::i2cStopWithCS(_X76_WRITE_DELAY);
	return NO_ERROR;
}

Error X76F041Cart::setDataKey(const uint8_t *newKey) {
	Error error = _x76Command(
		_X76F041_CONFIG, _X76F041_CFG_SET_DATA_KEY, _X76F041_ACK_POLL
	);
	if (error)
		return error;

	// The X76F041 requires the key to be sent twice as a way of ensuring it
	// gets received correctly.
	for (int i = 2; i; i--) {
		if (!io::i2cWriteBytes(newKey, sizeof(dataKey))) {
			io::i2cStopWithCS();
			LOG("NACK while setting new data key");
			return X76_NACK;
		}
	}

	io::i2cStopWithCS(_X76_WRITE_DELAY);

	// Update the data key stored in the class.
	memcpy(dataKey, newKey, sizeof(dataKey));
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

X76F100Cart::X76F100Cart(void) {
	Cart();

	chipType = TYPE_X76F100;
}

// TODO: actually implement this (even though no X76F100 carts were ever made)

/* ZS01 driver */

ZS01Cart::ZS01Cart(void) {
	Cart();

	chipType = TYPE_ZS01;
	flags    = HAS_DS2401;
}

Error ZS01Cart::_transact(zs01::Packet &request, zs01::Packet &response) {
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

	_state = response.address;
	if (response.command != zs01::RESP_NO_ERROR) {
		LOG("ZS01 error, code=0x%02x", response.command);
		return ZS01_ERROR;
	}

	return NO_ERROR;
}

Error ZS01Cart::readCartID(void) {
	zs01::Packet request, response;
	Error        error;

	request.address = zs01::ADDR_ZS01_ID;
	request.encodeReadRequest();

	error = _transact(request, response);
	if (error)
		return error;
	if (!_validateID(response.data))
		return DS2401_ID_ERROR;

	response.copyDataTo(zsID);

	flags |= ZS_ID_OK;

	request.address = zs01::ADDR_DS2401_ID;
	request.encodeReadRequest();

	error = _transact(request, response);
	if (error)
		return error;
	if (!_validateID(response.data))
		return DS2401_ID_ERROR;

	response.copyDataTo(cartID);

	flags |= CART_ID_OK;
	return NO_ERROR;
}

Error ZS01Cart::readPublicData(void) {
	zs01::Packet request, response;

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PUBLIC_END; i++) {
		request.address = i;
		request.encodeReadRequest();

		Error error = _transact(request, response);
		if (error)
			return error;

		response.copyDataTo(&data[i * sizeof(response.data)]);
	}

	flags |= PUBLIC_DATA_OK;
	return NO_ERROR;
}

Error ZS01Cart::readPrivateData(void) {
	zs01::Packet request, response;
	zs01::Key    key;
	key.unpackFrom(dataKey);

	for (int i = zs01::ADDR_PRIVATE; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.encodeReadRequest(key, _state);

		Error error = _transact(request, response);
		if (error)
			return error;

		response.copyDataTo(&data[i * sizeof(response.data)]);
	}

	flags |= PRIVATE_DATA_OK;

	request.address = zs01::ADDR_CONFIG;
	request.encodeReadRequest(key, _state);

	Error error = _transact(request, response);
	if (error)
		return error;

	response.copyDataTo(config);

	flags |= CONFIG_OK;
	return NO_ERROR;
}

Error ZS01Cart::writeData(void) {
	zs01::Packet request, response;
	zs01::Key    key;
	key.unpackFrom(dataKey);

	for (int i = zs01::ADDR_PUBLIC; i < zs01::ADDR_PRIVATE_END; i++) {
		request.address = i;
		request.copyDataFrom(&data[i * sizeof(request.data)]);
		request.encodeWriteRequest(key, _state);

		Error error = _transact(request, response);
		if (error)
			return error;
	}

	request.address = zs01::ADDR_CONFIG;
	request.copyDataFrom(config);
	request.encodeWriteRequest(key, _state);

	return _transact(request, response);
}

Error ZS01Cart::erase(void) {
	zs01::Packet request, response;
	zs01::Key    key;
	key.unpackFrom(dataKey);

	memset(request.data, 0, sizeof(request.data));
	request.address = zs01::ADDR_ERASE;
	request.encodeWriteRequest(key, _state);

	return _transact(request, response);
}

Error ZS01Cart::setDataKey(const uint8_t *newKey) {
	zs01::Packet request, response;
	zs01::Key    key;
	key.unpackFrom(dataKey);

	request.address = zs01::ADDR_DATA_KEY;
	request.copyDataFrom(newKey);
	request.encodeWriteRequest(key, _state);

	Error error = _transact(request, response);
	if (error)
		return error;

	// Update the data key stored in the class.
	memcpy(dataKey, newKey, sizeof(dataKey));
	return NO_ERROR;
}

}
