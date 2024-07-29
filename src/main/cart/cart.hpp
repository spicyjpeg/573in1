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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/util/string.hpp"
#include "common/util/templates.hpp"
#include "common/rom.hpp"

namespace cart {

/* Definitions */

enum ChipType : uint8_t {
	NONE    = 0,
	X76F041 = 1,
	X76F100 = 2,
	ZS01    = 3
};

enum DumpFlag : uint8_t {
	DUMP_HAS_SYSTEM_ID   = 1 << 0,
	DUMP_HAS_CART_ID     = 1 << 1,
	DUMP_CONFIG_OK       = 1 << 2,
	DUMP_SYSTEM_ID_OK    = 1 << 3,
	DUMP_CART_ID_OK      = 1 << 4,
	DUMP_ZS_ID_OK        = 1 << 5,
	DUMP_PUBLIC_DATA_OK  = 1 << 6,
	DUMP_PRIVATE_DATA_OK = 1 << 7
};

static constexpr int    NUM_CHIP_TYPES       = 4;
static constexpr size_t MAX_QR_STRING_LENGTH = 0x600;

/* Identifier structure */

class Identifier {
public:
	uint8_t data[8];

	inline void copyFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, sizeof(data));
	}
	inline void copyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, sizeof(data));
	}
	inline bool isEmpty(void) const {
		return (util::sum(data, sizeof(data)) == 0);
	}

	inline size_t toString(char *output) const {
		return util::hexToString(output, data, sizeof(data), '-');
	}
	inline size_t toSerialNumber(char *output) const {
		return util::serialNumberToString(output, &data[1]);
	}

	void updateChecksum(void);
	bool validateChecksum(void) const;
	void updateDSCRC(void);
	bool validateDSCRC(void) const;
};

/* Cartridge dump structure */

static constexpr uint16_t CART_DUMP_HEADER_MAGIC       = 0x573d;
static constexpr uint16_t ROM_HEADER_DUMP_HEADER_MAGIC = 0x573e;

struct ChipSize {
public:
	size_t dataLength, publicDataOffset, publicDataLength;
};

extern const ChipSize CHIP_SIZES[NUM_CHIP_TYPES];

class CartDump {
public:
	uint16_t magic;
	ChipType chipType;
	uint8_t  flags;

	Identifier systemID, cartID, zsID;

	uint8_t dataKey[8], config[8];
	uint8_t data[512];

	inline CartDump(void)
	: magic(CART_DUMP_HEADER_MAGIC), chipType(NONE), flags(0) {}

	inline const ChipSize &getChipSize(void) const {
		return CHIP_SIZES[chipType];
	}
	inline bool validateMagic(void) const {
		return
			(magic == CART_DUMP_HEADER_MAGIC) &&
			(chipType > 0) &&
			(chipType < NUM_CHIP_TYPES);
	}
	inline size_t getDumpLength(void) const {
		return (sizeof(CartDump) - sizeof(data)) + getChipSize().dataLength;
	}
	inline void clearIdentifiers(void) {
		util::clear(systemID);
		util::clear(cartID);
		util::clear(zsID);
	}
	inline void copyDataFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, getChipSize().dataLength);
	}
	inline void copyDataTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, getChipSize().dataLength);
	}
	inline void copyKeyFrom(const uint8_t *source) {
		__builtin_memcpy(dataKey, source, sizeof(dataKey));
	}
	inline void copyKeyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, dataKey, sizeof(dataKey));
	}
	inline void copyConfigFrom(const uint8_t *source) {
		__builtin_memcpy(config, source, sizeof(config));
	}
	inline void copyConfigTo(uint8_t *dest) const {
		__builtin_memcpy(dest, config, sizeof(config));
	}

	void initConfig(uint8_t maxAttempts, bool hasPublicSection = false);
	bool isPublicDataEmpty(void) const;
	bool isDataEmpty(void) const;
	bool isReadableDataEmpty(void) const;
	size_t toQRString(char *output) const;
};

/* Flash and RTC header dump structure */

class ROMHeaderDump {
public:
	uint16_t magic;
	uint8_t  _reserved, flags;

	Identifier systemID;

	uint8_t data[rom::FLASH_CRC_OFFSET - rom::FLASH_HEADER_OFFSET];

	inline ROMHeaderDump(void)
	: magic(ROM_HEADER_DUMP_HEADER_MAGIC), _reserved(0), flags(0) {}

	inline bool validateMagic(void) const {
		return (magic == ROM_HEADER_DUMP_HEADER_MAGIC);
	}

	bool isDataEmpty(void) const;
};

}
