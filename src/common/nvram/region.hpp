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

namespace nvram {

/* Base NVRAM region class */

enum RegionError {
	NO_ERROR        = 0,
	UNSUPPORTED_OP  = 1,
	NO_DEVICE       = 2,
	CHIP_TIMEOUT    = 3,
	CHIP_ERROR      = 4,
	VERIFY_MISMATCH = 5,
	WRITE_PROTECTED = 6
};

class Region {
public:
	// Note that sectorLength is always in byte units, even when wordLength > 1.
	size_t wordLength;
	size_t sectorLength;
	size_t sectorsPerChip;
	size_t numBanks;

	inline Region(
		size_t _wordLength,
		size_t _sectorLength,
		size_t _sectorsPerChip = 1,
		size_t _numBanks       = 1
	) :
		wordLength(_wordLength),
		sectorLength(_sectorLength),
		sectorsPerChip(_sectorsPerChip),
		numBanks(_numBanks) {}

	inline size_t getChipLength(void) const {
		return sectorLength * sectorsPerChip;
	}

	virtual void read(void *data, uint32_t offset, size_t length) const {}
	virtual uint32_t zipCRC32(
		uint32_t offset, size_t length, uint32_t crc = 0
	) const { return crc; }

	virtual void writeWord(uint32_t offset, uint32_t value) {}
	virtual RegionError flushWrite(uint32_t offset, uint32_t value) {
		return UNSUPPORTED_OP;
	}

	virtual void eraseSector(uint32_t offset) {}
	virtual void eraseChip(uint32_t offset) {}
	virtual RegionError flushErase(uint32_t offset) { return UNSUPPORTED_OP; }
};

/* Utilities */

extern const char *const REGION_ERROR_NAMES[];

static inline const char *getErrorString(RegionError error) {
	return REGION_ERROR_NAMES[error];
}

}
