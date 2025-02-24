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
#include "common/sys573/base.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers.h"

namespace nvram {

static constexpr int _FLASH_WRITE_TIMEOUT = 10000000;
static constexpr int _FLASH_ERASE_TIMEOUT = 20000000;

/* Internal and PCMCIA flash base class */

static inline volatile uint16_t *_toFlashPtr(uint32_t ptrOffset) {
	return reinterpret_cast<volatile uint16_t *>(DEV0_BASE | ptrOffset);
}

uint32_t FlashRegion::_getPtrOffset(uint32_t offset) const {
	auto bankOffset = offset / FLASH_BANK_LENGTH;
	auto ptrOffset  = offset % FLASH_BANK_LENGTH;

	sys573::setFlashBank(_bank + bankOffset);
	return ptrOffset & ~1;
}

void FlashRegion::read(void *data, uint32_t offset, size_t length) const {
	auto ptr = reinterpret_cast<uintptr_t>(data);

	while (length > 0) {
		auto ptrOffset  = _getPtrOffset(offset);
		auto readLength =
			util::min<size_t>(length, FLASH_BANK_LENGTH - ptrOffset);

		__builtin_memcpy(
			reinterpret_cast<void *>(ptr),
			reinterpret_cast<const void *>(DEV0_BASE | ptrOffset),
			readLength
		);

		ptr    += readLength;
		offset += readLength;
		length -= readLength;
	}
}

uint32_t FlashRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	while (length > 0) {
		auto ptrOffset  = _getPtrOffset(offset);
		auto readLength =
			util::min<size_t>(length, FLASH_BANK_LENGTH - ptrOffset);

		crc = util::zipCRC32.update(
			reinterpret_cast<const uint8_t *>(DEV0_BASE | ptrOffset),
			readLength,
			crc
		);

		offset += readLength;
		length -= readLength;
	}

	return crc;
}

/* JEDEC flash chip drivers */

RegionError JEDEC8FlashRegion::_flushByte(
	uint32_t offset, uint8_t value, int timeout
) {
	auto ptr   = _toFlashPtr(_getPtrOffset(offset));
	int  shift = (offset & 1) * 8;

	uint8_t status, diff;

	for (; timeout >= 0; timeout--) {
		status = (*ptr >> shift) & 0xff;
		diff   = status ^ value;

		if (!(diff & JEDEC_STAT_POLL_BIT))
			return NO_ERROR;
		if (status & JEDEC_STAT_ERROR)
			break;
	}

	// If the error flag was set, make sure an error actually occurred.
	status = (*ptr >> shift) & 0xff;
	diff   = status ^ value;

	if (!(diff & JEDEC_STAT_POLL_BIT))
		return NO_ERROR;

	*ptr = JEDEC_RESET;

	if (status & JEDEC_STAT_ERROR) {
		LOG_NVRAM("JEDEC error, ptr=0x%06x, st=0x%02x", offset, status);
		return CHIP_ERROR;
	} else {
		LOG_NVRAM("JEDEC timeout, ptr=0x%06x, st=0x%02x", offset, status);
		return CHIP_TIMEOUT;
	}
}

void JEDEC8FlashRegion::writeWord(uint32_t offset, uint32_t value) {
	auto ptrOffset = _getPtrOffset(offset);
	auto ptr       = _toFlashPtr(ptrOffset);
	auto chip      = _toFlashPtr(ptrOffset & ~(getChipLength() - 1));

	chip[0x0000] = JEDEC_RESET;
	chip[0x5555] = JEDEC_HANDSHAKE1;
	chip[0x2aaa] = JEDEC_HANDSHAKE2;
	chip[0x5555] = JEDEC_WRITE_BYTE;

	*ptr = value;
}

RegionError JEDEC8FlashRegion::flushWrite(uint32_t offset, uint32_t value) {
	auto error = _flushByte(offset, (value >> 0) & 0xff, _FLASH_WRITE_TIMEOUT);

	return error
		? error
		: _flushByte(offset + 1, (value >> 8) & 0xff, _FLASH_WRITE_TIMEOUT);
}

void JEDEC8FlashRegion::eraseSector(uint32_t offset) {
	auto ptrOffset = _getPtrOffset(offset);
	auto ptr       = _toFlashPtr(ptrOffset);
	auto chip      = _toFlashPtr(ptrOffset & ~(getChipLength() - 1));

	*chip        = JEDEC_RESET;
	chip[0x5555] = JEDEC_HANDSHAKE1;
	chip[0x2aaa] = JEDEC_HANDSHAKE2;
	chip[0x5555] = JEDEC_ERASE_HANDSHAKE;
	chip[0x5555] = JEDEC_HANDSHAKE1;
	chip[0x2aaa] = JEDEC_HANDSHAKE2;
	*ptr         = JEDEC_ERASE_SECTOR;
}

void JEDEC8FlashRegion::eraseChip(uint32_t offset) {
	auto ptrOffset = _getPtrOffset(offset);
	auto chip      = _toFlashPtr(ptrOffset & ~(getChipLength() - 1));

	*chip        = JEDEC_RESET;
	chip[0x5555] = JEDEC_HANDSHAKE1;
	chip[0x2aaa] = JEDEC_HANDSHAKE2;
	chip[0x5555] = JEDEC_ERASE_HANDSHAKE;
	chip[0x5555] = JEDEC_HANDSHAKE1;
	chip[0x2aaa] = JEDEC_HANDSHAKE2;
	chip[0x5555] = JEDEC_ERASE_CHIP;
}

RegionError JEDEC8FlashRegion::flushErase(uint32_t offset) {
	auto error = _flushByte(offset, 0xff, _FLASH_ERASE_TIMEOUT);

	return error ? error : _flushByte(offset + 1, 0xff, _FLASH_ERASE_TIMEOUT);
}

// Writes to chips with a 16-bit bus can be issued in the same way as their
// 8-bit counterparts (the upper 8 bits of each command are ignored), however
// polling needs to be handled differently as the status bits are not mirrored.
RegionError JEDEC16FlashRegion::flushWrite(uint32_t offset, uint32_t value) {
	return _flushByte(offset, value, _FLASH_WRITE_TIMEOUT);
}

RegionError JEDEC16FlashRegion::flushErase(uint32_t offset) {
	return _flushByte(offset, 0xff, _FLASH_ERASE_TIMEOUT);
}

/* Intel flash chip drivers */

RegionError Intel8FlashRegion::_flushByte(uint32_t offset, int timeout) {
	auto ptr   = _toFlashPtr(_getPtrOffset(offset));
	int  shift = (offset & 1) * 8;

	uint8_t status = 0;

	*ptr = INTEL_GET_STATUS;

	for (; timeout >= 0; timeout--) {
		status = (*ptr >> shift) & 0xff;

		if (!(status & INTEL_STAT_WSMS))
			continue;

		*ptr = INTEL_RESET;

		// The datasheet suggests only checking the error flags after WSMS = 1.
		if (status & (INTEL_STAT_DPS | INTEL_STAT_VPPS)) {
			*ptr = INTEL_CLEAR_STATUS;

			LOG_NVRAM("Intel WP, ptr=0x%06x, st=0x%02x", offset, status);
			return WRITE_PROTECTED;
		}
		if (status & (INTEL_STAT_BWSLBS | INTEL_STAT_ECLBS)) {
			*ptr = INTEL_CLEAR_STATUS;

			LOG_NVRAM("Intel error, ptr=0x%06x, st=0x%02x", offset, status);
			return CHIP_ERROR;
		}

		return NO_ERROR;
	}

	*ptr = INTEL_RESET;

	LOG_NVRAM("Intel timeout, ptr=0x%06x, st=0x%02x", offset, status);
	return CHIP_TIMEOUT;
}

void Intel8FlashRegion::writeWord(uint32_t offset, uint32_t value) {
	auto ptr = _toFlashPtr(_getPtrOffset(offset));

	*ptr = INTEL_RESET;
	*ptr = INTEL_CLEAR_STATUS;
	*ptr = INTEL_WRITE_BYTE;
	*ptr = value;
}

RegionError Intel8FlashRegion::flushWrite(uint32_t offset, uint32_t value) {
	auto error = _flushByte(offset, _FLASH_WRITE_TIMEOUT);

	return error ? error : _flushByte(offset + 1, _FLASH_WRITE_TIMEOUT);
}

void Intel8FlashRegion::eraseSector(uint32_t offset) {
	auto ptr = _toFlashPtr(_getPtrOffset(offset));

	*ptr = INTEL_RESET;
	*ptr = INTEL_ERASE_SECTOR1;
	*ptr = INTEL_ERASE_SECTOR2;
}

void Intel8FlashRegion::eraseChip(uint32_t offset) {
	// TODO: implement by calling eraseSector() for each sector
}

RegionError Intel8FlashRegion::flushErase(uint32_t offset) {
	auto error = _flushByte(offset, _FLASH_ERASE_TIMEOUT);

	return error ? error : _flushByte(offset + 1, _FLASH_ERASE_TIMEOUT);
}

RegionError Intel16FlashRegion::flushWrite(uint32_t offset, uint32_t value) {
	return _flushByte(offset, _FLASH_WRITE_TIMEOUT);
}

RegionError Intel16FlashRegion::flushErase(uint32_t offset) {
	return _flushByte(offset, _FLASH_ERASE_TIMEOUT);
}

}
