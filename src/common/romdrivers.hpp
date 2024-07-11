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
#include "common/rom.hpp"

namespace rom {

/* Chip drivers */

enum JEDECCommand : uint16_t {
	JEDEC_RESET           = 0xf0f0,
	JEDEC_HANDSHAKE1      = 0xaaaa,
	JEDEC_HANDSHAKE2      = 0x5555,
	JEDEC_GET_ID          = 0x9090,
	JEDEC_WRITE_BYTE      = 0xa0a0,
	JEDEC_ERASE_HANDSHAKE = 0x8080,
	JEDEC_ERASE_CHIP      = 0x1010,
	JEDEC_ERASE_SECTOR    = 0x3030
};

enum JEDECStatusFlag : uint16_t {
	JEDEC_STATUS_ERASE_TOGGLE = 1 << 2,
	JEDEC_STATUS_ERASE_START  = 1 << 3,
	JEDEC_STATUS_ERROR        = 1 << 5,
	JEDEC_STATUS_TOGGLE       = 1 << 6,
	JEDEC_STATUS_POLL_BIT     = 1 << 7
};

enum IntelCommand : uint16_t {
	INTEL_RESET         = 0xffff,
	INTEL_GET_ID        = 0x9090,
	INTEL_WRITE_BYTE    = 0x4040,
	INTEL_ERASE_SECTOR1 = 0x2020,
	INTEL_ERASE_SECTOR2 = 0xd0d0,
	INTEL_GET_STATUS    = 0x7070,
	INTEL_CLEAR_STATUS  = 0x5050,
	INTEL_SUSPEND       = 0xb0b0,
	INTEL_RESUME        = 0xd0d0
};

enum IntelStatusFlag : uint16_t {
	INTEL_STATUS_DPS    = 1 << 1,
	INTEL_STATUS_BWSS   = 1 << 2,
	INTEL_STATUS_VPPS   = 1 << 3,
	INTEL_STATUS_BWSLBS = 1 << 4,
	INTEL_STATUS_ECLBS  = 1 << 5,
	INTEL_STATUS_ESS    = 1 << 6,
	INTEL_STATUS_WSMS   = 1 << 7
};

enum DriverError {
	NO_ERROR        = 0,
	UNSUPPORTED_OP  = 1,
	CHIP_TIMEOUT    = 2,
	CHIP_ERROR      = 3,
	VERIFY_MISMATCH = 4,
	WRITE_PROTECTED = 5
};

struct ChipSize {
public:
	size_t chipLength, eraseSectorLength;
};

class Driver {
protected:
	const Region &_region;

public:
	inline Driver(const Region &region)
	: _region(region) {}

	// Note that all offsets must be multiples of 2, as writes are done in
	// halfwords.
	virtual ~Driver(void) {}
	virtual void write(uint32_t offset, uint16_t value) {}
	virtual void eraseSector(uint32_t offset) {}
	virtual void eraseChip(uint32_t offset) {}
	virtual DriverError flushWrite(uint32_t offset, uint16_t value) {
		return UNSUPPORTED_OP;
	}
	virtual DriverError flushErase(uint32_t offset) {
		return UNSUPPORTED_OP;
	}
	virtual const ChipSize &getChipSize(void) const;
};

class RTCDriver : public Driver {
public:
	inline RTCDriver(const RTCRegion &region)
	: Driver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class AM29F016Driver : public Driver {
protected:
	DriverError _flush(uint32_t offset, uint16_t value, int timeout);

public:
	inline AM29F016Driver(const FlashRegion &region)
	: Driver(region) {}

	virtual void write(uint32_t offset, uint16_t value);
	virtual void eraseSector(uint32_t offset);
	virtual void eraseChip(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class AM29F040Driver : public AM29F016Driver {
public:
	inline AM29F040Driver(const FlashRegion &region)
	: AM29F016Driver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class Intel28F016S5Driver : public Driver {
protected:
	DriverError _flush(uint32_t offset, int timeout);

public:
	inline Intel28F016S5Driver(const FlashRegion &region)
	: Driver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class Intel28F640J5Driver : public Intel28F016S5Driver {
public:
	inline Intel28F640J5Driver(const FlashRegion &region)
	: Intel28F016S5Driver(region) {}

	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

extern const char *const DRIVER_ERROR_NAMES[];

static inline const char *getErrorString(DriverError error) {
	return DRIVER_ERROR_NAMES[error];
}

}
