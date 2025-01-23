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
#include "common/nvram/region.hpp"
#include "common/util/hash.hpp"
#include "common/util/misc.hpp"
#include "common/util/string.hpp"
#include "common/util/templates.hpp"

namespace nvram {

/* BIOS ROM and RTC RAM drivers */

class BIOSRegion : public Region {
public:
	inline BIOSRegion(void)
	: Region(1, 0x80000) {}

	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;
};

class RTCRegion : public Region {
public:
	inline RTCRegion(void)
	: Region(1, 0x1ff8) {}

	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	void writeWord(uint32_t offset, uint32_t value);
	RegionError flushWrite(uint32_t offset, uint32_t value);

	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	RegionError flushErase(uint32_t offset);
};

/* BIOS ROM headers */

class SonyKernelHeader {
public:
	uint8_t  day, month;
	uint16_t year;
	uint32_t flags;
	uint8_t  magic[32], _pad[4], version[36];

	inline bool validateMagic(void) const {
		return (
			util::hash(magic, sizeof(magic)) ==
			"Sony Computer Entertainment Inc."_h
		);
	}
};

class OpenBIOSHeader {
public:
	uint32_t magic[2];
	uint32_t idNameLength, idDescLength, idType;
	uint8_t  idData[24];

	inline bool validateMagic(void) const {
		return (magic[0] == "Open"_c) && (magic[1] == "BIOS"_c);
	}
	inline size_t getBuildID(char *output) const {
		return util::hexToString(output, &idData[idNameLength], idDescLength);
	}
};

class ShellInfo {
public:
	const char *name, *bootFileName;
	util::Hash headerHash;

	const util::ExecutableHeader *header;

	inline bool validateHash(void) const {
		return (util::hash(
			reinterpret_cast<const uint8_t *>(header),
			sizeof(util::ExecutableHeader)
		) == headerHash);
	}
};

static const auto &sonyKernelHeader =
	*reinterpret_cast<const SonyKernelHeader *>(DEV2_BASE | 0x100);
static const auto &openBIOSHeader =
	*reinterpret_cast<const OpenBIOSHeader *>(DEV2_BASE | 0x78);

bool getShellInfo(ShellInfo &output);

}
