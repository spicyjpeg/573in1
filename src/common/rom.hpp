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
#include "common/util.hpp"
#include "ps1/registers.h"

namespace rom {

/* ROM region dumpers */

static constexpr size_t   FLASH_BANK_LENGTH       = 0x400000;
static constexpr uint32_t FLASH_HEADER_OFFSET     = 0x00;
static constexpr uint32_t FLASH_CRC_OFFSET        = 0x20;
static constexpr uint32_t FLASH_EXECUTABLE_OFFSET = 0x24;

class Driver;

class Region {
public:
	uintptr_t ptr;
	size_t    regionLength;
	int       bank;

	inline Region(uintptr_t ptr, size_t regionLength, int bank = -1)
	: ptr(ptr), regionLength(regionLength), bank(bank) {}

	virtual bool isPresent(void) const {
		return true;
	}

	virtual uint16_t *getRawPtr(uint32_t offset, bool alignToChip = false) const;
	virtual void read(void *data, uint32_t offset, size_t length) const;
	virtual uint32_t zipCRC32(
		uint32_t offset, size_t length, uint32_t crc = 0
	) const;

	virtual const util::ExecutableHeader *getBootExecutableHeader(void) const {
		return nullptr;
	}
	virtual uint32_t getJEDECID(void) const {
		return 0;
	}
	virtual size_t getActualLength(void) const {
		return regionLength;
	}
	virtual Driver *newDriver(void) const {
		return nullptr;
	}
};

class BIOSRegion : public Region {
public:
	inline BIOSRegion(void)
	: Region(DEV2_BASE, 0x80000) {}
};

class RTCRegion : public Region {
public:
	inline RTCRegion(void)
	: Region(DEV0_BASE | 0x620000, 0x1ff8) {}

	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	Driver *newDriver(void) const;
};

class FlashRegion : public Region {
private:
	uint32_t _inputs;

public:
	inline FlashRegion(size_t regionLength, int bank, uint32_t inputs = 0)
	: Region(DEV0_BASE, regionLength, bank), _inputs(inputs) {}

	bool isPresent(void) const;

	uint16_t *getRawPtr(uint32_t offset, bool alignToChip = false) const;
	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	const util::ExecutableHeader *getBootExecutableHeader(void) const;
	uint32_t getJEDECID(void) const;
	size_t getActualLength(void) const;
	Driver *newDriver(void) const;
};

extern const BIOSRegion  bios;
extern const RTCRegion   rtc;
extern const FlashRegion flash, pcmcia[2];

/* BIOS ROM headers */

class SonyKernelHeader {
public:
	uint8_t  day, month;
	uint16_t year;
	uint32_t flags;
	uint8_t  magic[32], _pad[4], version[36];

	bool validateMagic(void) const;
};

class OpenBIOSHeader {
public:
	uint8_t  magic[8];
	uint32_t idNameLength, idDescLength, idType;
	uint8_t  idData[24];

	inline size_t getBuildID(char *output) const {
		return util::hexToString(output, &idData[idNameLength], idDescLength);
	}

	bool validateMagic(void) const;
};

class ShellInfo {
public:
	const char *name, *bootFileName;
	util::Hash headerHash;

	const util::ExecutableHeader *header;

	bool validateHash(void) const;
};

static const auto &sonyKernelHeader =
	*reinterpret_cast<const SonyKernelHeader *>(DEV2_BASE | 0x100);
static const auto &openBIOSHeader =
	*reinterpret_cast<const OpenBIOSHeader *>(DEV2_BASE | 0x78);

bool getShellInfo(ShellInfo &output);

}
