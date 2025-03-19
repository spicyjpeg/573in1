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
#include "common/cart/cart.hpp"
#include "common/bus.hpp"

namespace cart {

/* X76F041 and X76F100 security cartridge drivers */

class X76F041Cart : public Cart {
	friend Cart *_newCartDriver(const bus::I2CDriver &i2c);

private:
	inline X76F041Cart(const bus::I2CDriver &i2c)
	: Cart(i2c, X76F041, 512) {}

public:
	CartError read(
		void          *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key = nullptr
	);
	CartError write(
		const void    *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key
	);
	CartError erase(const uint8_t *key);

	CartError readConfig(uint8_t *config, const uint8_t *key);
	CartError writeConfig(const uint8_t *config, const uint8_t *key);
	CartError setKey(const uint8_t *newKey, const uint8_t *oldKey);
};

class X76F100Cart : public Cart {
	friend Cart *_newCartDriver(const bus::I2CDriver &i2c);

private:
	inline X76F100Cart(const bus::I2CDriver &i2c)
	: Cart(i2c, X76F100, 112) {}

public:
	CartError read(
		void          *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key = nullptr
	);
	CartError write(
		const void    *data,
		uint16_t      lba,
		size_t        count,
		const uint8_t *key
	);
	CartError erase(const uint8_t *key);

	CartError setKey(const uint8_t *newKey, const uint8_t *oldKey);
};

}
