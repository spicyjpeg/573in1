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
#include "common/nvram/flash.hpp"
#include "common/nvram/flashdetect.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers573.h"

namespace nvram {

/* Utilities */

static void _issueReset(void) {
	*SYS573_FLASH_BASE = JEDEC_RESET;
	*SYS573_FLASH_BASE = INTEL_RESET;
}

static void _issueGetID(void) {
	// JEDEC_GET_ID has the same opcode as INTEL_GET_ID, so an Intel chip will
	// ignore the handshake sequence but still return its identifiers.
	SYS573_FLASH_BASE[0x5555] = JEDEC_HANDSHAKE1;
	SYS573_FLASH_BASE[0x2aaa] = JEDEC_HANDSHAKE2;
	SYS573_FLASH_BASE[0x5555] = JEDEC_GET_ID;
}

static uint32_t _readIDs(void) {
	return util::concat4(SYS573_FLASH_BASE[0], SYS573_FLASH_BASE[1]);
}

/* Flash region constructor */

enum FlashChipType : uint8_t {
	_TYPE_JEDEC8  = 0,
	_TYPE_INTEL8  = 1,
	_TYPE_JEDEC16 = 2,
	_TYPE_INTEL16 = 3
};

struct FlashChipInfo {
public:
	const char    *name;
	FlashChipType type;
	uint8_t       manufacturerID, deviceID;
	uint8_t       sectorsPerChip;
	size_t        sectorLength;
};

static const FlashChipInfo _FLASH_CHIPS[]{
	{
		.name           = "AM29F016",
		.type           = _TYPE_JEDEC8,
		.manufacturerID = 0x01,
		.deviceID       = 0xad,
		.sectorsPerChip = 32,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "AM29F040",
		.type           = _TYPE_JEDEC8,
		.manufacturerID = 0x01,
		.deviceID       = 0xa4,
		.sectorsPerChip = 8,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "MBM29F016A",
		.type           = _TYPE_JEDEC8,
		.manufacturerID = 0x04,
		.deviceID       = 0xad,
		.sectorsPerChip = 32,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "MBM29F017A",
		.type           = _TYPE_JEDEC8,
		.manufacturerID = 0x04,
		.deviceID       = 0x3d,
		.sectorsPerChip = 32,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "MBM29F040A",
		.type           = _TYPE_JEDEC8,
		.manufacturerID = 0x04,
		.deviceID       = 0xa4,
		.sectorsPerChip = 8,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "28F016S5/LH28F016S",
		.type           = _TYPE_INTEL8,
		.manufacturerID = 0x89,
		.deviceID       = 0xaa,
		.sectorsPerChip = 32,
		.sectorLength   = 0x10000 * 2
	}, {
		.name           = "28F320J5",
		.type           = _TYPE_INTEL16,
		.manufacturerID = 0x89,
		.deviceID       = 0x14,
		.sectorsPerChip = 32,
		.sectorLength   = 0x20000
	}, {
		.name           = "28F640J5",
		.type           = _TYPE_INTEL16,
		.manufacturerID = 0x89,
		.deviceID       = 0x15,
		.sectorsPerChip = 64,
		.sectorLength   = 0x20000
	}
};

static constexpr size_t _DUMMY_SECTORS_PER_CHIP = 1;
static constexpr size_t _DUMMY_SECTOR_LENGTH    = 0x10000;

FlashRegion *newFlashRegion(int bank) {
	sys573::setFlashBank(bank);
	_issueReset();

	auto resetValue = _readIDs();
	_issueGetID();
	auto id         = _readIDs();

	if (id == resetValue) {
		LOG_NVRAM("chip not responding");
		return new FlashRegion(
			_DUMMY_SECTOR_LENGTH,
			_DUMMY_SECTORS_PER_CHIP,
			0,
			bank
		);
	}

	// Try to detect the number of banks available by searching for mirrors.
	// Mirroring is detected by resetting the first chip of each subsequent bank
	// until the first bank also gets reset and exits JEDEC ID mode.
	size_t numBanks = 1;

	for (; numBanks < MAX_FLASH_BANKS; numBanks++) {
		sys573::setFlashBank(bank + numBanks);
		_issueReset();
		sys573::setFlashBank(bank);

		if (_readIDs() != id)
			break;
	}

	_issueReset();

	// Determine if the chip is a single part with a 16-bit bus or two separate
	// 8-bit ones by checking for mirroring in the ID.
	uint8_t manufacturerLow  = (id >>  0) & 0xff;
	uint8_t manufacturerHigh = (id >>  8) & 0xff;
	uint8_t deviceLow        = (id >> 16) & 0xff;
	uint8_t deviceHigh       = (id >> 24) & 0xff;

	bool is8BitChip = true
		&& (manufacturerLow == manufacturerHigh)
		&& (deviceLow       == deviceHigh);

	for (auto &chip : _FLASH_CHIPS) {
		if (manufacturerLow != chip.manufacturerID)
			continue;
		if (deviceLow       != chip.deviceID)
			continue;
		if (is8BitChip      != bool(chip.type <= _TYPE_INTEL8))
			continue;

		LOG_NVRAM("detected %s, %d banks", chip.name, numBanks);

		switch (chip.type) {
			case _TYPE_JEDEC8:
				return new JEDEC8FlashRegion(
					chip.sectorLength,
					chip.sectorsPerChip,
					numBanks,
					bank
				);

			case _TYPE_INTEL8:
				return new Intel8FlashRegion(
					chip.sectorLength,
					chip.sectorsPerChip,
					numBanks,
					bank
				);

			case _TYPE_JEDEC16:
				return new JEDEC16FlashRegion(
					chip.sectorLength,
					chip.sectorsPerChip,
					numBanks,
					bank
				);

			case _TYPE_INTEL16:
				return new Intel16FlashRegion(
					chip.sectorLength,
					chip.sectorsPerChip,
					numBanks,
					bank
				);
		}
	}

	LOG_NVRAM(
		"unknown %d-bit chip, man=0x%02x, dev=0x%02x",
		is8BitChip ? 8 : 16,
		manufacturerLow,
		deviceLow
	);
	return new FlashRegion(
		_DUMMY_SECTOR_LENGTH,
		_DUMMY_SECTORS_PER_CHIP,
		numBanks,
		bank
	);
}

}
