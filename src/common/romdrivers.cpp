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

#include <stddef.h>
#include <stdint.h>
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/rom.hpp"
#include "common/romdrivers.hpp"

namespace rom {

/* Data common to all chip drivers */

static constexpr int _FLASH_WRITE_TIMEOUT = 10000000;
static constexpr int _FLASH_ERASE_TIMEOUT = 20000000;

const char *const DRIVER_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"CHIP_TIMEOUT",
	"CHIP_ERROR",
	"VERIFY_MISMATCH",
	"WRITE_PROTECTED"
};

static const ChipSize _DUMMY_CHIP_SIZE{
	.chipLength        = 0,
	.eraseSectorLength = 0
};

// The onboard flash and all Konami-supplied flash cards use 2 MB chips with 64
// KB sectors and an 8-bit bus.
static const ChipSize _STANDARD_CHIP_SIZE{
	.chipLength        = 2 * 0x200000,
	.eraseSectorLength = 2 * 0x10000
};

static const ChipSize _ALT_CHIP_SIZE{
	.chipLength        = 2 * 0x80000,
	.eraseSectorLength = 2 * 0x10000
};

const ChipSize &Driver::getChipSize(void) const {
	return _DUMMY_CHIP_SIZE;
}

/* RTC RAM driver */

static const ChipSize _RTC_CHIP_SIZE{
	.chipLength        = 0x1ff8,
	.eraseSectorLength = 0x1ff8
};

void RTCDriver::write(uint32_t offset, uint16_t value) {
	auto ptr = reinterpret_cast<volatile uint16_t *>(_region.ptr + offset * 2);
	ptr[0]   = value & 0xff;
	ptr[1]   = value >> 8;
}

void RTCDriver::eraseSector(uint32_t offset) {
	auto ptr = reinterpret_cast<void *>(_region.ptr);

	__builtin_memset(ptr, 0, _region.regionLength * 2);
}

void RTCDriver::eraseChip(uint32_t offset) {
	eraseSector(offset);
}

DriverError RTCDriver::flushWrite(uint32_t offset, uint16_t value) {
	auto ptr = reinterpret_cast<volatile uint16_t *>(_region.ptr + offset * 2);

	uint16_t actualValue = util::concat2(ptr[0] & 0xff, ptr[1] & 0xff);

	if (value != actualValue) {
		LOG_NVRAM(
			"ptr=0x%06x, exp=0x%02x, got=0x%04x", offset, value, actualValue
		);
		return VERIFY_MISMATCH;
	}

	return NO_ERROR;
}

DriverError RTCDriver::flushErase(uint32_t offset) {
	return flushWrite(offset, 0);
}

const ChipSize &RTCDriver::getChipSize(void) const {
	return _RTC_CHIP_SIZE;
}

/* AMD AM29F016/017 (Fujitsu MBM29F016A/017A) driver */

DriverError AM29F016Driver::_flush(
	uint32_t offset, uint16_t value, int timeout
) {
	volatile uint16_t *ptr = _region.getRawPtr(offset & ~1);

	int     shift = (offset & 1) * 8;
	uint8_t byte  = (value >> shift) & 0xff;

	uint8_t status, diff;

	for (; timeout >= 0; timeout--) {
		status = (*ptr >> shift) & 0xff;
		diff   = status ^ byte;

		if (!(diff & JEDEC_STATUS_POLL_BIT))
			return NO_ERROR;
		if (status & JEDEC_STATUS_ERROR)
			break;
	}

	// If the error flag was set, make sure an error actually occurred.
	status = (*ptr >> shift) & 0xff;
	diff   = status ^ byte;

	if (!(diff & JEDEC_STATUS_POLL_BIT))
		return NO_ERROR;

	*ptr = JEDEC_RESET;

	if (status & JEDEC_STATUS_ERROR) {
		LOG_NVRAM("JEDEC error, ptr=0x%06x, st=0x%02x", offset, status);
		return CHIP_ERROR;
	} else {
		LOG_NVRAM("JEDEC timeout, ptr=0x%06x, st=0x%02x", offset, status);
		return CHIP_TIMEOUT;
	}
}

void AM29F016Driver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x000]  = JEDEC_RESET;
	ptr[0x555]  = JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = JEDEC_HANDSHAKE2;
	ptr[0x555]  = JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void AM29F016Driver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x000]  = JEDEC_RESET;
	ptr[0x555]  = JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = JEDEC_HANDSHAKE2;
	ptr[0x555]  = JEDEC_ERASE_HANDSHAKE;
	ptr[0x555]  = JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = JEDEC_HANDSHAKE2;
	ptr[offset] = JEDEC_ERASE_SECTOR;
}

void AM29F016Driver::eraseChip(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);

	ptr[0x000] = JEDEC_RESET;
	ptr[0x555] = JEDEC_HANDSHAKE1;
	ptr[0x2aa] = JEDEC_HANDSHAKE2;
	ptr[0x555] = JEDEC_ERASE_HANDSHAKE;
	ptr[0x555] = JEDEC_HANDSHAKE1;
	ptr[0x2aa] = JEDEC_HANDSHAKE2;
	ptr[0x555] = JEDEC_ERASE_CHIP;
}

DriverError AM29F016Driver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto error = _flush(offset, value, _FLASH_WRITE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, value, _FLASH_WRITE_TIMEOUT);
}

DriverError AM29F016Driver::flushErase(uint32_t offset) {
	auto error = _flush(offset, 0xffff, _FLASH_ERASE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, 0xffff, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &AM29F016Driver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* AMD AM29F040 (Fujitsu MBM29F040A) driver */

// Konami's drivers handle this chip pretty much identically to the MBM29F016A,
// but using 0x5555/0x2aaa as command addresses instead of 0x555/0x2aa.

void AM29F040Driver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x0000] = JEDEC_RESET;
	ptr[0x5555] = JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = JEDEC_HANDSHAKE2;
	ptr[0x5555] = JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void AM29F040Driver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x0000] = JEDEC_RESET;
	ptr[0x5555] = JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = JEDEC_HANDSHAKE2;
	ptr[0x5555] = JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = JEDEC_HANDSHAKE2;
	ptr[offset] = JEDEC_ERASE_SECTOR;
}

void AM29F040Driver::eraseChip(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);

	ptr[0x0005] = JEDEC_RESET;
	ptr[0x5555] = JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = JEDEC_HANDSHAKE2;
	ptr[0x5555] = JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = JEDEC_HANDSHAKE2;
	ptr[0x5555] = JEDEC_ERASE_CHIP;
}

const ChipSize &AM29F040Driver::getChipSize(void) const {
	return _ALT_CHIP_SIZE;
}

/* Intel 28F016S5 (Sharp LH28F016S) driver */

DriverError Intel28F016S5Driver::_flush(uint32_t offset, int timeout) {
	volatile uint16_t *ptr  = _region.getRawPtr(offset & ~1);

	int     shift  = (offset & 1) * 8;
	uint8_t status = 0;

	*ptr = INTEL_GET_STATUS;

	for (; timeout >= 0; timeout--) {
		status = (*ptr >> shift) & 0xff;

		if (!(status & INTEL_STATUS_WSMS))
			continue;

		*ptr = INTEL_RESET;

		// The datasheet suggests only checking the error flags after WSMS = 1.
		if (status & (INTEL_STATUS_DPS | INTEL_STATUS_VPPS)) {
			*ptr = INTEL_CLEAR_STATUS;

			LOG_NVRAM("Intel WP, ptr=0x%06x, st=0x%02x", offset, status);
			return WRITE_PROTECTED;
		}
		if (status & (INTEL_STATUS_BWSLBS | INTEL_STATUS_ECLBS)) {
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

void Intel28F016S5Driver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset);

	*ptr = INTEL_RESET;
	*ptr = INTEL_CLEAR_STATUS;
	*ptr = INTEL_WRITE_BYTE;
	*ptr = value;
}

void Intel28F016S5Driver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset);

	*ptr = INTEL_RESET;
	*ptr = INTEL_ERASE_SECTOR1;
	*ptr = INTEL_ERASE_SECTOR2;
}

DriverError Intel28F016S5Driver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto error = _flush(offset, _FLASH_WRITE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, _FLASH_WRITE_TIMEOUT);
}

DriverError Intel28F016S5Driver::flushErase(uint32_t offset) {
	auto error = _flush(offset, _FLASH_ERASE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &Intel28F016S5Driver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* Intel 28F640J5 driver */

static const ChipSize _28F640J5_CHIP_SIZE{
	.chipLength        = 0x800000,
	.eraseSectorLength = 0x20000
};

DriverError Intel28F640J5Driver::flushWrite(
	uint32_t offset, uint16_t value
) {
	return _flush(offset, _FLASH_WRITE_TIMEOUT);
}

DriverError Intel28F640J5Driver::flushErase(uint32_t offset) {
	return _flush(offset, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &Intel28F640J5Driver::getChipSize(void) const {
	return _28F640J5_CHIP_SIZE;
}

}
