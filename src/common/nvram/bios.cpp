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

#include <stddef.h>
#include <stdint.h>
#include "common/nvram/bios.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"

namespace nvram {

/* BIOS ROM and RTC RAM drivers */

BIOSRegion bios;
RTCRegion  rtc;

void BIOSRegion::read(void *data, uint32_t offset, size_t length) const {
	__builtin_memcpy(
		data,
		reinterpret_cast<const void *>(DEV2_BASE | offset),
		length
	);
}

uint32_t BIOSRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	return util::zipCRC32.update(
		reinterpret_cast<const uint8_t *>(DEV2_BASE | offset),
		length,
		crc
	);
}

void RTCRegion::read(void *data, uint32_t offset, size_t length) const {
	auto ptr  = &SYS573_RTC_BASE[offset];
	auto dest = reinterpret_cast<uint8_t *>(data);

	// The RTC is an 8-bit device connected to a 16-bit bus, i.e. each byte must
	// be read as a 16-bit value and the upper 8 bits must be discarded.
	for (; length > 0; length--)
		*(dest++) = uint8_t(*(ptr++));
}

uint32_t RTCRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	auto ptr = &SYS573_RTC_BASE[offset];
	crc      = ~crc;

	for (; length > 0; length--)
		crc = util::zipCRC32.update(*(ptr++), crc);

	return ~crc;
}

void RTCRegion::writeWord(uint32_t offset, uint32_t value) {
	SYS573_RTC_BASE[offset] = value & 0xff;
}

RegionError RTCRegion::flushWrite(uint32_t offset, uint32_t value) {
	auto written = SYS573_RTC_BASE[offset];

	if ((value & 0xff) != written) {
		LOG_NVRAM("mismatch: exp=0x%02x, got=0x%02x", value, written);
		return VERIFY_MISMATCH;
	}

	return NO_ERROR;
}

void RTCRegion::eraseSector(uint32_t offset) {
	auto ptr    = SYS573_RTC_BASE;
	auto endPtr = &SYS573_RTC_BASE[sectorLength];

	while (ptr < endPtr)
		*(ptr++) = 0xff;
}

void RTCRegion::eraseChip(uint32_t offset) {
	return eraseSector(offset);
}

RegionError RTCRegion::flushErase(uint32_t offset) {
	return flushWrite(offset, 0xff);
}

/* BIOS ROM headers */

static const ShellInfo _KONAMI_SHELLS[]{
	{
		.name         = "700A01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerHash   = 0x9c615f57,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x40000
		)
	}, {
		.name         = "700A01 (Gachagachamp)",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerHash   = 0x7e31a844,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x40000
		)
	}, {
		.name         = "899A01",
		.bootFileName = nullptr,
		.headerHash   = 0xecdeaad0,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x40000
		)
	}, {
		.name         = "700B01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x61334),
		.headerHash   = 0xb257d3b5,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x28000
		)
	}
};

bool getShellInfo(ShellInfo &output) {
	for (auto &shell : _KONAMI_SHELLS) {
		if (!shell.validateHash())
			continue;

		util::copy(output, shell);
		return true;
	}

	// If no official shell was found, fall back to searching the entire ROM for
	// a valid PS1 executable. Note that the executable has to be 32-byte
	// aligned for this to work.
	for (uintptr_t ptr = DEV2_BASE; ptr < (DEV2_BASE + 0x80000); ptr += 32) {
		auto header = reinterpret_cast<const util::ExecutableHeader *>(ptr);

		if (!header->validateMagic())
			continue;

		output.name         = header->getRegionString();
		output.bootFileName = nullptr;
#if 0
		output.headerHash   = util::hash(
			reinterpret_cast<const uint8_t *>(header),
			sizeof(util::ExecutableHeader)
		);
#else
		output.headerHash   = 0;
#endif
		output.header       = header;
		return true;
	}

	return false;
}

}
