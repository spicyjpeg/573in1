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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/bus.hpp"

namespace cart {

static constexpr size_t SECTOR_LENGTH = 8;
static constexpr size_t KEY_LENGTH    = 8;
static constexpr size_t CONFIG_LENGTH = 8;

/* Base security cartridge driver class */

enum ChipType : uint8_t {
	NONE    = 0,
	X76F041 = 1,
	X76F100 = 2,
	ZS01    = 3
};

enum CartError {
	NO_ERROR          = 0,
	UNSUPPORTED_OP    = 1,
	NO_DEVICE         = 2,
	CHIP_TIMEOUT      = 3,
	CHIP_ERROR        = 4,
	VERIFY_MISMATCH   = 5,
	CHECKSUM_MISMATCH = 6,
	INVALID_ID        = 7
};

class Cart {
protected:
	const bus::I2CDriver &_i2c;

public:
	ChipType type;
	uint16_t capacity;

	inline Cart(
		const bus::I2CDriver &i2c,
		ChipType             _type,
		uint16_t             _capacity
	) :
		_i2c(i2c),
		type(_type),
		capacity(_capacity) {}

	virtual CartError read(
		void          *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key = nullptr
	) { return UNSUPPORTED_OP; }
	virtual CartError write(
		const void    *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key
	) { return UNSUPPORTED_OP; }
	virtual CartError erase(const uint8_t *key) { return UNSUPPORTED_OP; }

	virtual CartError readConfig(
		uint8_t       *config,
		const uint8_t *key
	) { return UNSUPPORTED_OP; }
	virtual CartError writeConfig(
		const uint8_t *config,
		const uint8_t *key
	) { return UNSUPPORTED_OP; }
	virtual CartError setKey(
		const uint8_t *newKey,
		const uint8_t *oldKey
	) { return UNSUPPORTED_OP; }

	virtual CartError readID(bus::OneWireID &output);
	virtual CartError readInternalID(bus::OneWireID &output) {
		return UNSUPPORTED_OP;
	}
};

Cart *newCartDriver(const bus::I2CDriver &i2c);
Cart *newCartDriver(void);

/* Utilities */

extern const char *const CART_ERROR_NAMES[];

static inline const char *getErrorString(CartError error) {
	return CART_ERROR_NAMES[error];
}

}
