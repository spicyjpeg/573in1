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
#include "common/blkdev/ata.hpp"
#include "common/blkdev/device.hpp"
#include "common/blkdev/idebase.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"

/*
 * Based on the following specifications:
 *
 * - "AT Attachment with Packet Interface - 6", 2001-06-26
 * - "CF+ and CompactFlash Specification Revision 3.0", 2004-12-23
 *
 * https://www.cs.utexas.edu/~dahlin/Classes/UGOS/reading/ide.html
 */

namespace blkdev {

static constexpr size_t _SECTOR_LENGTH = 512;

/* ATA utilities */

DeviceError ATADevice::_setLBA(uint64_t lba, size_t count, int timeout) {
	if (flags & SUPPORTS_EXT_LBA) {
		assert(lba < (1ULL << 48));
		assert(count <= (1 << 16));

		_select(CS0_DEVICE_SEL_LBA);

		auto error = _waitForIdle(true);

		if (error)
			return error;

		_set(CS0_COUNT,      (count >>  8) & 0xff);
		_set(CS0_SECTOR,     (lba   >> 24) & 0xff);
		_set(CS0_CYLINDER_L, (lba   >> 32) & 0xff);
		_set(CS0_CYLINDER_H, (lba   >> 40) & 0xff);
	} else {
		assert(lba < (1ULL << 28));
		assert(count <= (1 << 8));

		_select(CS0_DEVICE_SEL_LBA | ((lba >> 24) & 15));

		auto error = _waitForIdle(true);

		if (error)
			return error;
	}

	_set(CS0_COUNT,      (count >>  0) & 0xff);
	_set(CS0_SECTOR,     (lba   >>  0) & 0xff);
	_set(CS0_CYLINDER_L, (lba   >>  8) & 0xff);
	_set(CS0_CYLINDER_H, (lba   >> 16) & 0xff);
	return NO_ERROR;
}

DeviceError ATADevice::_transfer(
	uintptr_t ptr, uint64_t lba, size_t count, bool write
) {
	uint8_t cmd;
	size_t  maxLength;

	if (flags & SUPPORTS_EXT_LBA) {
		cmd       = write ? ATA_WRITE_SECTORS_EXT : ATA_READ_SECTORS_EXT;
		maxLength = 1 << 16;
	} else {
		cmd       = write ? ATA_WRITE_SECTORS : ATA_READ_SECTORS;
		maxLength = 1 << 8;
	}

	while (count) {
		size_t chunkLength = util::min(count, maxLength);
		auto   error       = _setLBA(lba, chunkLength);

		if (error)
			return error;

		_set(CS0_COMMAND, cmd);

		// Data must be transferred one sector at a time as the drive may
		// deassert DRQ between sectors.
		for (size_t i = chunkLength; i > 0; i--) {
			auto error = _waitForDRQ();

			if (error)
				return error;

			if (write)
				_writeData(reinterpret_cast<const void *>(ptr), _SECTOR_LENGTH);
			else
				_readData(reinterpret_cast<void *>(ptr), _SECTOR_LENGTH);

			ptr += _SECTOR_LENGTH;
		}

		lba   += chunkLength;
		count -= chunkLength;
	}

	return _waitForIdle();
}

/* ATA block device class */

static constexpr int _DETECT_TIMEOUT = 2500000;

DeviceError ATADevice::enumerate(void) {
	// NOTE: the primary drive may respond to all secondary drive register
	// accesses, with the exception of command writes, if no secondary drive is
	// actually present. A strict timeout is used in the commands below in order
	// to prevent blocking for too long.
	IDEIdentifyBlock block;

	_set(CS0_COMMAND, ATA_IDENTIFY);

	if (_waitForDRQ(_DETECT_TIMEOUT))
		return NO_DRIVE;

	_readData(&block, sizeof(IDEIdentifyBlock));

	if (!block.validateChecksum())
		return CHECKSUM_MISMATCH;

	type         = ATA;
	sectorLength = _SECTOR_LENGTH;

	if (block.commandSetFlags[1] & (1 << 10)) {
		flags   |= SUPPORTS_EXT_LBA;
		capacity = block.getSectorCountExt();
	} else {
		flags   &= ~SUPPORTS_EXT_LBA;
		capacity = block.getSectorCount();
	}

	if (block.commandSetFlags[1] & (1 << 12))
		flags |= SUPPORTS_FLUSH;
	else
		flags &= ~SUPPORTS_FLUSH;

	LOG_BLKDEV("drive %d is ATA", getDeviceIndex());
	return _setup(block);
}

DeviceError ATADevice::poll(void) {
	if (!type)
		return NO_DRIVE;

	_select(CS0_DEVICE_SEL_LBA);
	return _waitForIdle(true);
}

void ATADevice::handleInterrupt(void) {
	// TODO: use interrupts to yield instead of busy waiting
}

DeviceError ATADevice::read(void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!type)
		return NO_DRIVE;

	return _transfer(reinterpret_cast<uintptr_t>(data), lba, count, false);
}

DeviceError ATADevice::write(const void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!type)
		return NO_DRIVE;

	return _transfer(reinterpret_cast<uintptr_t>(data), lba, count, true);
}

DeviceError ATADevice::trim(uint64_t lba, size_t count) {
	// TODO: implement
	return UNSUPPORTED_OP;
}

DeviceError ATADevice::flushCache(void) {
	if (!type)
		return NO_DRIVE;
	if (!(flags & SUPPORTS_FLUSH))
#if 0
		return UNSUPPORTED_OP;
#else
		return NO_ERROR;
#endif

	_select(CS0_DEVICE_SEL_LBA);

	auto error = _waitForIdle(true);

	if (error)
		return error;

	_set(
		CS0_COMMAND,
		(flags & SUPPORTS_EXT_LBA) ? ATA_FLUSH_CACHE_EXT : ATA_FLUSH_CACHE
	);
	return _waitForIdle();
}

DeviceError ATADevice::goIdle(bool standby) {
	if (!type)
		return NO_DRIVE;

	_select(CS0_DEVICE_SEL_LBA);

	auto error = _waitForIdle(true);

	if (error)
		return error;

	_set(CS0_COMMAND, standby ? ATA_STANDBY_IMMEDIATE : ATA_IDLE_IMMEDIATE);
	return _waitForIdle();
}

}
