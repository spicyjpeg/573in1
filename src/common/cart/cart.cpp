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

#include <stdint.h>
#include "common/cart/cart.hpp"
#include "common/cart/x76.hpp"
#include "common/cart/zs01.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"

namespace cart {

/* Base security cartridge driver class */

CartError Cart::readID(bus::OneWireID *output) {
	return sys573::cartDS2401.readID(output)
		? NO_ERROR
		: NO_DEVICE;
}

/* Security cartridge detection and constructor */

enum ChipIdentifier : uint32_t {
	_ID_X76F041 = 0x55aa5519,
	_ID_X76F100 = 0x55aa0019,
	_ID_ZS01    = 0x5a530001
};

Cart *newCartDriver(const bus::I2CDriver &i2c) {
	// The X76F041/X76F100 and ZS01 use different reset sequences and output
	// their IDs in different bit orders.
	auto zs01ID = sys573::cartI2C.resetZS01();

	if (zs01ID == _ID_ZS01)
		return new ZS01Cart(i2c);

	LOG_CART("unknown ZS01 ID: 0x%08x", zs01ID);

	auto x76ID = sys573::cartI2C.resetX76();

	switch (x76ID) {
		case _ID_X76F041:
			return new X76F041Cart(i2c);

		case _ID_X76F100:
			return new X76F100Cart(i2c);

		default:
			LOG_CART("unknown X76 ID: 0x%08x", x76ID);
			return nullptr;
	}
}

Cart *newCartDriver(void) {
	if (!sys573::getCartInsertionStatus()) {
		LOG_CART("DSR not asserted");
		return nullptr;
	}

	return newCartDriver(sys573::cartI2C);
}

/* Utilities */

const char *const CART_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"NO_DEVICE",
	"CHIP_TIMEOUT",
	"CHIP_ERROR",
	"VERIFY_MISMATCH",
	"CHECKSUM_MISMATCH"
};

}
