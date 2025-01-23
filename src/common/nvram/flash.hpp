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
#include "common/util/templates.hpp"

namespace nvram {

static constexpr size_t FLASH_BANK_LENGTH = 0x400000;
static constexpr size_t MAX_FLASH_BANKS   = 16;

static constexpr uint32_t FLASH_HEADER_OFFSET     = 0x00;
static constexpr uint32_t FLASH_CRC_OFFSET        = 0x20;
static constexpr uint32_t FLASH_EXECUTABLE_OFFSET = 0x24;

/* Internal and PCMCIA flash base class */

class FlashRegion : public Region {
	friend FlashRegion *newFlashRegion(int bank);

private:
	int _bank;

protected:
	inline FlashRegion(
		size_t _sectorLength,
		size_t _sectorsPerChip,
		size_t _numBanks,
		int    bank
	) :
		Region(
			2,
			_sectorLength,
			_sectorsPerChip,
			_numBanks
		),
		_bank(bank) {}

	uint32_t _getPtrOffset(uint32_t offset) const;

public:
	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;
};

/* JEDEC flash chip drivers */

enum JEDECFlashCommand : uint16_t {
	JEDEC_RESET           = util::mirror2(0xf0),
	JEDEC_HANDSHAKE1      = util::mirror2(0xaa),
	JEDEC_HANDSHAKE2      = util::mirror2(0x55),
	JEDEC_GET_ID          = util::mirror2(0x90),
	JEDEC_WRITE_BYTE      = util::mirror2(0xa0),
	JEDEC_ERASE_HANDSHAKE = util::mirror2(0x80),
	JEDEC_ERASE_CHIP      = util::mirror2(0x10),
	JEDEC_ERASE_SECTOR    = util::mirror2(0x30)
};

enum JEDECFlashStatusFlag : uint8_t {
	JEDEC_STAT_ERASE_TOGGLE = 1 << 2,
	JEDEC_STAT_ERASE_START  = 1 << 3,
	JEDEC_STAT_ERROR        = 1 << 5,
	JEDEC_STAT_TOGGLE       = 1 << 6,
	JEDEC_STAT_POLL_BIT     = 1 << 7
};

class JEDEC8FlashRegion : public FlashRegion {
	friend FlashRegion *newFlashRegion(int bank);

protected:
	inline JEDEC8FlashRegion(
		size_t _sectorLength,
		size_t _sectorsPerChip,
		size_t _numBanks,
		int    bank
	) : FlashRegion(
		_sectorLength,
		_sectorsPerChip,
		_numBanks,
		bank
	) {}

	RegionError _flushByte(uint32_t offset, uint8_t value, int timeout);

public:
	void writeWord(uint32_t offset, uint32_t value);
	virtual RegionError flushWrite(uint32_t offset, uint32_t value);

	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	virtual RegionError flushErase(uint32_t offset);
};

class JEDEC16FlashRegion : public JEDEC8FlashRegion {
	friend FlashRegion *newFlashRegion(int bank);

private:
	inline JEDEC16FlashRegion(
		size_t _sectorLength,
		size_t _sectorsPerChip,
		size_t _numBanks,
		int    bank
	) : JEDEC8FlashRegion(
		_sectorLength,
		_sectorsPerChip,
		_numBanks,
		bank
	) {}

public:
	RegionError flushWrite(uint32_t offset, uint32_t value);

	RegionError flushErase(uint32_t offset);
};

/* Intel flash chip drivers */

enum IntelFlashCommand : uint16_t {
	INTEL_RESET         = util::mirror2(0xff),
	INTEL_GET_ID        = util::mirror2(0x90),
	INTEL_WRITE_BYTE    = util::mirror2(0x40),
	INTEL_ERASE_SECTOR1 = util::mirror2(0x20),
	INTEL_ERASE_SECTOR2 = util::mirror2(0xd0),
	INTEL_GET_STATUS    = util::mirror2(0x70),
	INTEL_CLEAR_STATUS  = util::mirror2(0x50),
	INTEL_SUSPEND       = util::mirror2(0xb0),
	INTEL_RESUME        = util::mirror2(0xd0)
};

enum IntelFlashStatusFlag : uint8_t {
	INTEL_STAT_DPS    = 1 << 1,
	INTEL_STAT_BWSS   = 1 << 2,
	INTEL_STAT_VPPS   = 1 << 3,
	INTEL_STAT_BWSLBS = 1 << 4,
	INTEL_STAT_ECLBS  = 1 << 5,
	INTEL_STAT_ESS    = 1 << 6,
	INTEL_STAT_WSMS   = 1 << 7
};

class Intel8FlashRegion : public FlashRegion {
	friend FlashRegion *newFlashRegion(int bank);

protected:
	inline Intel8FlashRegion(
		size_t _sectorLength,
		size_t _sectorsPerChip,
		size_t _numBanks,
		int    bank
	) : FlashRegion(
		_sectorLength,
		_sectorsPerChip,
		_numBanks,
		bank
	) {}

	RegionError _flushByte(uint32_t offset, int timeout);

public:
	void writeWord(uint32_t offset, uint32_t value);
	virtual RegionError flushWrite(uint32_t offset, uint32_t value);

	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	virtual RegionError flushErase(uint32_t offset);
};

class Intel16FlashRegion : public Intel8FlashRegion {
	friend FlashRegion *newFlashRegion(int bank);

private:
	inline Intel16FlashRegion(
		size_t _sectorLength,
		size_t _sectorsPerChip,
		size_t _numBanks,
		int    bank
	) : Intel8FlashRegion(
		_sectorLength,
		_sectorsPerChip,
		_numBanks,
		bank
	) {}

public:
	RegionError flushWrite(uint32_t offset, uint32_t value);

	RegionError flushErase(uint32_t offset);
};

}
