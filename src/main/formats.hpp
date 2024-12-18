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
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"

namespace formats {

/* Game database flags */

enum CartPCBType : uint8_t {
	CART_UNKNOWN_X76F041        =  1,
	CART_UNKNOWN_X76F041_DS2401 =  2,
	CART_UNKNOWN_ZS01           =  3,
	CART_GX700_PWB_D            =  4,
	CART_GX700_PWB_E            =  5,
	CART_GX700_PWB_J            =  6,
	CART_GX883_PWB_D            =  7,
	CART_GX894_PWB_D            =  8,
	CART_GX896_PWB_A_A          =  9,
	CART_GE949_PWB_D_A          = 10,
	CART_GE949_PWB_D_B          = 12,
	CART_PWB0000068819          = 12,
	CART_PWB0000088954          = 13
};

enum HeaderFlag : uint8_t {
	FORMAT_BITMASK        = 7 << 0,
	FORMAT_NONE           = 0 << 0,
	FORMAT_SIMPLE         = 1 << 0,
	FORMAT_BASIC          = 2 << 0,
	FORMAT_EXTENDED       = 3 << 0,
	SPEC_TYPE_BITMASK     = 3 << 3,
	SPEC_TYPE_NONE        = 0 << 3,
	SPEC_TYPE_ACTUAL      = 1 << 3,
	SPEC_TYPE_WILDCARD    = 2 << 3,
	HEADER_SCRAMBLED      = 1 << 5,
	HEADER_IN_PUBLIC_AREA = 1 << 6,
	REGION_LOWERCASE      = 1 << 7
};

enum ChecksumFlag : uint8_t {
	CHECKSUM_WIDTH_BITMASK     = 3 << 0,
	CHECKSUM_WIDTH_NONE        = 0 << 0,
	CHECKSUM_WIDTH_8           = 1 << 0,
	CHECKSUM_WIDTH_8_IN_16_OUT = 2 << 0,
	CHECKSUM_WIDTH_16          = 3 << 0,
	CHECKSUM_INPUT_BIG_ENDIAN  = 1 << 2,
	CHECKSUM_OUTPUT_BIG_ENDIAN = 1 << 3,
	CHECKSUM_INVERTED          = 1 << 4,
	CHECKSUM_FORCE_GX_SPEC     = 1 << 5
};

enum IdentifierFlag : uint8_t {
	PRIVATE_TID_TYPE_BITMASK     = 3 << 0,
	PRIVATE_TID_TYPE_NONE        = 0 << 0,
	PRIVATE_TID_TYPE_STATIC      = 1 << 0,
	PRIVATE_TID_TYPE_SID_HASH_LE = 2 << 0,
	PRIVATE_TID_TYPE_SID_HASH_BE = 3 << 0,
	PRIVATE_SID_PRESENT          = 1 << 2,
	PRIVATE_MID_PRESENT          = 1 << 3,
	PRIVATE_XID_PRESENT          = 1 << 4,
	ALLOCATE_DUMMY_PUBLIC_AREA   = 1 << 5,
	PUBLIC_MID_PRESENT           = 1 << 6,
	PUBLIC_XID_PRESENT           = 1 << 7
};

enum SignatureFlag : uint8_t {
	SIGNATURE_TYPE_BITMASK  = 3 << 0,
	SIGNATURE_TYPE_NONE     = 0 << 0,
	SIGNATURE_TYPE_STATIC   = 1 << 0,
	SIGNATURE_TYPE_CHECKSUM = 2 << 0,
	SIGNATURE_TYPE_MD5      = 3 << 0,
	SIGNATURE_PAD_WITH_FF   = 1 << 2
};

enum GameFlag : uint8_t {
	GAME_IO_BOARD_BITMASK            = 7 << 0,
	GAME_IO_BOARD_NONE               = 0 << 0,
	GAME_IO_BOARD_ANALOG             = 1 << 0,
	GAME_IO_BOARD_KICK               = 2 << 0,
	GAME_IO_BOARD_FISHING_REEL       = 3 << 0,
	GAME_IO_BOARD_DIGITAL            = 4 << 0,
	GAME_IO_BOARD_DDR_KARAOKE        = 5 << 0,
	GAME_IO_BOARD_GUNMANIA           = 6 << 0,
	GAME_INSTALL_RTC_HEADER_REQUIRED = 1 << 3,
	GAME_RTC_HEADER_REQUIRED         = 1 << 4
};

/* Game database structures */

static constexpr size_t MAX_SPECIFICATIONS = 4;
static constexpr size_t MAX_REGIONS        = 12;

struct ROMHeaderInfo {
public:
	uint8_t signatureField[4], yearField[2];

	uint8_t headerFlags, checksumFlags, signatureFlags;
	uint8_t _reserved;
};

struct CartInfo {
public:
	uint8_t dataKey[8], yearField[2];

	CartPCBType pcb;
	uint8_t     tidWidth, midValue;
	uint8_t     headerFlags, checksumFlags, idFlags;
};

struct GameInfo {
public:
	char specifications[MAX_SPECIFICATIONS];
	char regions[MAX_REGIONS][3];
	char code[3];

	uint8_t  flags;
	uint16_t nameOffset;
	uint16_t year;

	ROMHeaderInfo rtcHeader, flashHeader;
	CartInfo      installCart, gameCart;
};

/* Game database parser */

static constexpr size_t NUM_SORT_TABLES = 4;

enum SortOrder : uint8_t {
	SORT_CODE = 0,
	SORT_NAME = 1,
	SORT_YEAR = 2
};

class GameDBHeader {
public:
	uint32_t magic[2];
	uint16_t sortTableOffsets[NUM_SORT_TABLES];

	inline bool validateMagic(void) const {
		return (magic[0] == "573g"_c) && (magic[1] == "medb"_c);
	}
};

class GameDB : public util::Data {
public:
	// TODO: implement
};

/* String table parser */

class StringTableHeader {
public:
	uint32_t magic[2];
	uint16_t numBuckets, numEntries;

	inline bool validateMagic(void) const {
		return (magic[0] == "573s"_c) && (magic[1] == "trng"_c);
	}
};

class StringTableEntry {
public:
	util::Hash id;
	uint16_t   offset, chained;

	inline util::Hash getHash(void) const {
		return id;
	}
	inline uint16_t getChained(void) const {
		return chained;
	}
};

class StringTable : public util::Data {
public:
	inline const char *operator[](util::Hash id) const {
		return get(id);
	}

	const char *get(util::Hash id) const;
	size_t format(char *buffer, size_t length, util::Hash id, ...) const;
};

}
