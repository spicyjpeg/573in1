/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
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
#include "common/util.hpp"
#include "main/cart/cart.hpp"
#include "vendor/miniz.h"

namespace cart {

/* Identifier structure */

void Identifier::updateChecksum(void) {
	data[7] = (util::sum(data, 7) & 0xff) ^ 0xff;
}

bool Identifier::validateChecksum(void) const {
	uint8_t value = (util::sum(data, 7) & 0xff) ^ 0xff;

	if (value != data[7]) {
		LOG_CART_DATA("mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
		return false;
	}

	return true;
}

void Identifier::updateDSCRC(void) {
	data[7] = util::dsCRC8(data, 7);
}

bool Identifier::validateDSCRC(void) const {
	if (!data[0] || (data[0] == 0xff)) {
		LOG_CART_DATA("invalid 1-wire prefix 0x%02x", data[0]);
		return false;
	}

	uint8_t value = util::dsCRC8(data, 7);

	if (value != data[7]) {
		LOG_CART_DATA("mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
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

void CartDump::initConfig(uint8_t maxAttempts, bool hasPublicSection) {
	util::clear(config);

	switch (chipType) {
		case X76F041:
			config[0] = 0xff;
			config[1] = hasPublicSection ? 0xaf : 0xff;
			config[2] = 0x20; // Disable retry counter
			config[3] = maxAttempts;
			break;

		case ZS01:
			//assert(hasPublicSection);
			config[4] = maxAttempts;
			break;

		default:
			break;
	}
}

bool CartDump::isPublicDataEmpty(void) const {
	if (!(flags & DUMP_PUBLIC_DATA_OK))
		return false;

	auto size = getChipSize();
	auto sum  = util::sum(&data[size.publicDataOffset], size.publicDataLength);

	return (!sum || (sum == (0xff * size.publicDataLength)));
}

bool CartDump::isDataEmpty(void) const {
	if (!(flags & DUMP_PUBLIC_DATA_OK) || !(flags & DUMP_PRIVATE_DATA_OK))
		return false;

	size_t length = getChipSize().dataLength;
	auto   sum    = util::sum(data, length);

	return (!sum || (sum == (0xff * length)));
}

bool CartDump::isReadableDataEmpty(void) const {
	// This is more or less a hack. The "right" way to tell if this chip has any
	// public data would be to use getChipSize().publicDataLength, but many
	// X76F041 carts don't actually have a public data area.
	if (chipType == ZS01)
		return isPublicDataEmpty();
	else
		return isDataEmpty();
}

static const char *const _MINIZ_ERROR_NAMES[]{
	"VERSION_ERROR",
	"BUF_ERROR",
	"MEM_ERROR",
	"DATA_ERROR",
	"STREAM_ERROR",
	"ERRNO",
	"OK", // = 0
	"STREAM_END",
	"NEED_DICT"
};

size_t CartDump::toQRString(char *output) const {
	uint8_t compressed[MAX_QR_STRING_LENGTH];
	size_t  uncompLength = getDumpLength();
	size_t  compLength   = MAX_QR_STRING_LENGTH;

	util::clear(compressed);

	auto error = mz_compress2(
		compressed, reinterpret_cast<mz_ulong *>(&compLength),
		reinterpret_cast<const uint8_t *>(this), uncompLength,
		MZ_BEST_COMPRESSION
	);

	if (error) {
		LOG_CART_DATA("%s", _MINIZ_ERROR_NAMES[error - MZ_VERSION_ERROR]);
		return 0;
	}

	LOG_CART_DATA(
		"compressed size: %d bytes (%d%%)", compLength,
		compLength * 100 / uncompLength
	);

	compLength = util::encodeBase41(&output[5], compressed, compLength);
	__builtin_memcpy(&output[0], "573::", 5);
	__builtin_memcpy(&output[compLength + 5], "::", 3);

	return compLength + 7;
}

/* Flash and RTC header dump structure */

bool ROMHeaderDump::isDataEmpty(void) const {
	auto sum = util::sum(data, sizeof(data));

#if 0
	return (!sum || (sum == (0xff * sizeof(data))));
#else
	return (sum == (0xff * sizeof(data)));
#endif
}

}
