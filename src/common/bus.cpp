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

#include <stddef.h>
#include <stdint.h>
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/bus.hpp"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace bus {

/* Hardware serial port driver */

size_t UARTDriver::readBytes(uint8_t *data, size_t length, int timeout) const {
	auto remaining = length;

	for (; timeout >= 0; timeout -= 10) {
		while (isRXAvailable()) {
			*(data++) = readByte();

			if (!(--remaining))
				break;
		}

		delayMicroseconds(10);
	}

	return length - remaining;
}

void UARTDriver::writeBytes(const uint8_t *data, size_t length) const {
	for (; length; length--)
		writeByte(*(data++));
}

int SIO1Driver::init(int baud) const {
	SIO_CTRL(1) = SIO_CTRL_RESET;

	int divider = F_CPU / baud;

	SIO_MODE(1) = 0
		| SIO_MODE_BAUD_DIV1
		| SIO_MODE_DATA_8
		| SIO_MODE_STOP_1;
	SIO_BAUD(1) = divider;
	SIO_CTRL(1) = 0
		| SIO_CTRL_TX_ENABLE
		| SIO_CTRL_RX_ENABLE
		| SIO_CTRL_RTS;

	return F_CPU / divider;
}

uint8_t SIO1Driver::readByte(void) const {
	while (!(SIO_STAT(1) & SIO_STAT_RX_NOT_EMPTY))
		__asm__ volatile("");

	return SIO_DATA(1);
}

void SIO1Driver::writeByte(uint8_t value) const {
	// The serial interface will buffer but not send any data if the CTS input
	// is not asserted, so we are going to abort if CTS is not set to avoid
	// waiting forever.
	while (
		(SIO_STAT(1) & (SIO_STAT_TX_NOT_FULL | SIO_STAT_CTS)) == SIO_STAT_CTS
	)
		__asm__ volatile("");

	if (SIO_STAT(1) & SIO_STAT_CTS)
		SIO_DATA(1) = value;
}

bool SIO1Driver::isConnected(void) const {
	return (SIO_STAT(1) / SIO_STAT_CTS) & 1;
}

bool SIO1Driver::isRXAvailable(void) const {
	return (SIO_STAT(1) / SIO_STAT_RX_NOT_EMPTY) & 1;
}

bool SIO1Driver::isTXFull(void) const {
	return ((SIO_STAT(1) / SIO_STAT_TX_NOT_FULL) & 1) ^ 1;
}

/* Bitbanged I2C driver */

static constexpr int _I2C_BUS_DELAY   = 50;
static constexpr int _I2C_RESET_DELAY = 500;

void I2CDriver::start(void) const {
	_setSDA(true);
	_setSCL(true,  _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::startWithCS(int csDelay) const {
	_setSDA(true);
	_setSCL(false);
	_setCS (true, _I2C_BUS_DELAY);

	_setCS (false, _I2C_BUS_DELAY + csDelay);
	_setSCL(true,  _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::stop(void) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high
}

void I2CDriver::stopWithCS(int csDelay) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high

	_setSCL(false, _I2C_BUS_DELAY + csDelay);
	_setCS (true,  _I2C_BUS_DELAY);
}

bool I2CDriver::getACK(void) const {
	delayMicroseconds(_I2C_BUS_DELAY); // Required for ZS01

	_setSCL(true,  _I2C_BUS_DELAY);
	bool ack = _getSDA();
	_setSCL(false, _I2C_BUS_DELAY * 2);

	return ack ^ 1;
}

void I2CDriver::sendACK(bool ack) const {
	_setSDA(ack ^ 1);
	_setSCL(true,  _I2C_BUS_DELAY);
	_setSCL(false, _I2C_BUS_DELAY);
	_setSDA(true,  _I2C_BUS_DELAY);
}

uint8_t I2CDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 7; i >= 0; i--) { // MSB first
		_setSCL(true,  _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	delayMicroseconds(_I2C_BUS_DELAY);
	return value;
}

void I2CDriver::writeByte(uint8_t value) const {
	for (int i = 7; i >= 0; i--) { // MSB first
		_setSDA((value >> i) & 1);
		_setSCL(true,  _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSDA(true, _I2C_BUS_DELAY);
}

void I2CDriver::readBytes(uint8_t *data, size_t length) const {
	for (; length; length--) {
		*(data++) = readByte();

		if (length > 1)
			sendACK(true);
	}
}

bool I2CDriver::writeBytes(
	const uint8_t *data, size_t length, int lastACKDelay
) const {
	for (; length; length--) {
		writeByte(*(data++));

		if (length == 1)
			delayMicroseconds(lastACKDelay);
		if (!getACK())
			return false;
	}

	return true;
}

uint32_t I2CDriver::resetX76(void) const {
	uint32_t value = 0;

	_setSDA  (true);
	_setSCL  (false);
	_setCS   (false);
	_setReset(false);

	_setReset(true,  _I2C_RESET_DELAY);
	_setSCL  (true,  _I2C_BUS_DELAY);
	_setSCL  (false, _I2C_BUS_DELAY);
	_setReset(false, _I2C_RESET_DELAY);

	for (int i = 0; i < 32; i++) { // LSB first
		_setSCL(true,  _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setCS (true, _I2C_BUS_DELAY);
	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

// For whatever reason the ZS01 does not implement the exact same response to
// reset protocol as the X76 chips. The reset pin is also active-low rather
// than active-high, and CS is ignored.
uint32_t I2CDriver::resetZS01(void) const {
	uint32_t value = 0;

	_setSDA  (true);
	_setSCL  (false);
	_setCS   (false);
	_setReset(true);

	_setReset(false, _I2C_RESET_DELAY);
	_setReset(true,  _I2C_RESET_DELAY);
	_setSCL  (true,  _I2C_BUS_DELAY);
	_setSCL  (false, _I2C_BUS_DELAY);

	for (int i = 31; i >= 0; i--) { // MSB first
		value |= _getSDA() << i;
		_setSCL(true,  _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

/* Bitbanged 1-wire driver */

static constexpr int _DS_RESET_LOW_TIME     = 480;
static constexpr int _DS_RESET_SAMPLE_DELAY = 70;
static constexpr int _DS_RESET_DELAY        = 410;

static constexpr int _DS_READ_LOW_TIME     = 3;
static constexpr int _DS_READ_SAMPLE_DELAY = 10;
static constexpr int _DS_READ_DELAY        = 53;

static constexpr int _DS_ZERO_LOW_TIME  = 65;
static constexpr int _DS_ZERO_HIGH_TIME = 5;
static constexpr int _DS_ONE_LOW_TIME   = 10;
static constexpr int _DS_ONE_HIGH_TIME  = 55;

bool OneWireDriver::reset(void) const {
	_set(false, _DS_RESET_LOW_TIME);
	_set(true,  _DS_RESET_SAMPLE_DELAY);
	bool present = _get();

	delayMicroseconds(_DS_RESET_DELAY);
	return present ^ 1;
}

uint8_t OneWireDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 0; i < 8; i++) { // LSB first
		_set(false, _DS_READ_LOW_TIME);
		_set(true,  _DS_READ_SAMPLE_DELAY);
		value |= _get() << i;
		delayMicroseconds(_DS_READ_DELAY);
	}

	return value;
}

void OneWireDriver::writeByte(uint8_t value) const {
	for (int i = 8; i; i--, value >>= 1) { // LSB first
		if (value & 1) {
			_set(false, _DS_ONE_LOW_TIME);
			_set(true,  _DS_ONE_HIGH_TIME);
		} else {
			_set(false, _DS_ZERO_LOW_TIME);
			_set(true,  _DS_ZERO_HIGH_TIME);
		}
	}
}

/* 1-wire chip ID reader */

enum OneWireCommand : uint8_t {
	_CMD_READ_ROM   = 0x33,
	_CMD_MATCH_ROM  = 0x55,
	_CMD_SKIP_ROM   = 0xcc,
	_CMD_SEARCH_ROM = 0xf0
};

void OneWireID::updateChecksum(void) {
	crc = util::dsCRC8(&familyCode, sizeof(OneWireID) - 1);
}

bool OneWireID::validateChecksum(void) const {
	if (!familyCode || (familyCode == 0xff)) {
		LOG_DATA("invalid 1-wire family 0x%02x", familyCode);
		return false;
	}

	uint8_t value = util::dsCRC8(&familyCode, sizeof(OneWireID) - 1);

	if (value != crc) {
		LOG_DATA("mismatch, exp=0x%02x, got=0x%02x", value, crc);
		return false;
	}

	return true;
}

bool OneWireDriver::readID(OneWireID *output) const {
	util::CriticalSection sec;

	if (!reset()) {
		LOG_IO("no 1-wire device found");
		return false;
	}

	writeByte(_CMD_READ_ROM);

	auto ptr = reinterpret_cast<uint8_t *>(output);

	for (int i = 8; i > 0; i--)
		*(ptr++) = readByte();

	return output->validateChecksum();
}

}
