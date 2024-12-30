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
#include "common/util/templates.hpp"

namespace storage {

/* CD-ROM definitions */

static constexpr uint32_t CDROM_TOC_PREGAP = 150;

class MSF {
public:
	uint8_t minute, second, frame;

	inline void fromLBA(uint32_t lba) {
		lba += CDROM_TOC_PREGAP;

		minute = lba / 4500;
		second = (lba / 75) % 60;
		frame  = lba % 75;
	}
	inline uint32_t toLBA(void) const {
		return -CDROM_TOC_PREGAP
			+ minute * 4500
			+ second * 75
			+ frame;
	}
};

class BCDMSF {
public:
	uint8_t minute, second, frame;

	inline void fromLBA(uint32_t lba) {
		lba += CDROM_TOC_PREGAP;

		minute = util::encodeBCD(lba / 4500);
		second = util::encodeBCD((lba / 75) % 60);
		frame  = util::encodeBCD(lba % 75);
	}
	inline uint32_t toLBA(void) const {
		return -CDROM_TOC_PREGAP
			+ util::decodeBCD(minute) * 4500
			+ util::decodeBCD(second) * 75
			+ util::decodeBCD(frame);
	}
};

/* Base block device class */

static constexpr size_t MAX_SECTOR_LENGTH = 2048;

enum DeviceType : uint8_t {
	NONE        = 0,
	ATA         = 1,
	ATAPI       = 2,
	MEMORY_CARD = 3
};

enum DeviceFlag : uint8_t {
	READ_ONLY        = 1 << 0,
	SUPPORTS_TRIM    = 1 << 1,
	SUPPORTS_FLUSH   = 1 << 2,
	SUPPORTS_EXT_LBA = 1 << 3,

	IS_SECONDARY        = 1 << 4,
	REQUIRES_EXT_PACKET = 1 << 5
};

enum DeviceError {
	NO_ERROR          = 0,
	UNSUPPORTED_OP    = 1,
	NO_DRIVE          = 2,
	NOT_YET_READY     = 3,
	STATUS_TIMEOUT    = 4,
	COMMAND_ERROR     = 5,
	CHECKSUM_MISMATCH = 6,
	DRIVE_ERROR       = 7,
	DISC_ERROR        = 8,
	DISC_CHANGED      = 9
};

class Device {
public:
	DeviceType type;
	uint8_t    flags;
	size_t     sectorLength;
	uint64_t   capacity;

	char model[48], revision[12], serialNumber[24];

	inline Device(uint8_t flags = 0)
	: type(NONE), flags(flags), sectorLength(0), capacity(0) {
		model       [0] = 0;
		revision    [0] = 0;
		serialNumber[0] = 0;
	}

	virtual DeviceError enumerate(void) { return UNSUPPORTED_OP; }
	virtual DeviceError poll(void) { return UNSUPPORTED_OP; }
	virtual void handleInterrupt(void) {}

	virtual DeviceError read(void *data, uint64_t lba, size_t count) {
		return UNSUPPORTED_OP;
	}
	virtual DeviceError write(const void *data, uint64_t lba, size_t count) {
		return UNSUPPORTED_OP;
	}
	virtual DeviceError trim(uint64_t lba, size_t count) {
		return UNSUPPORTED_OP;
	}
	virtual DeviceError flushCache(void) { return UNSUPPORTED_OP; }

	virtual DeviceError goIdle(bool standby = false) { return UNSUPPORTED_OP; }
	virtual DeviceError eject(bool close = false) { return UNSUPPORTED_OP; }
};

extern const char *const DEVICE_ERROR_NAMES[];

static inline const char *getErrorString(DeviceError error) {
	return DEVICE_ERROR_NAMES[error];
}

template<typename X> static inline bool isBufferAligned(X *ptr) {
	return !(reinterpret_cast<uintptr_t>(ptr) % alignof(uint32_t));
}

}
