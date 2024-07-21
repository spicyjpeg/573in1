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
#include "common/ide.hpp"
#include "common/idedefs.hpp"
#include "common/io.hpp"
#include "common/util.hpp"
#include "ps1/registers573.h"
#include "ps1/system.h"

/*
 * Based on the following specifications:
 *
 * - "AT Attachment with Packet Interface - 6", 2001-06-26
 * - "CF+ and CompactFlash Specification Revision 3.0", 2004-12-23
 * - SFF-8020i "ATA Packet Interface for CD-ROMs 2.6", 1996-01-22 (seems to be
 *   rather inaccurate about the IDE side of things, but some drives actually
 *   implement those inaccuracies!)
 *
 * https://www.cs.utexas.edu/~dahlin/Classes/UGOS/reading/ide.html
 * https://web.archive.org/web/20060427142409/http://www.stanford.edu/~csapuntz/blackmagic.html
 */

namespace ide {

static const char *const _SENSE_KEY_NAMES[]{
	"NO_SENSE",
	"RECOVERED_ERROR",
	"NOT_READY",
	"MEDIUM_ERROR",
	"HARDWARE_ERROR",
	"ILLEGAL_REQUEST",
	"UNIT_ATTENTION",
	"DATA_PROTECT",
	"BLANK_CHECK",
	"UNKNOWN_9",
	"UNKNOWN_A",
	"ABORTED_COMMAND",
	"UNKNOWN_C",
	"UNKNOWN_D",
	"MISCOMPARE",
	"UNKNOWN_F"
};

const char *const DEVICE_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"NO_DRIVE",
	"NOT_YET_READY",
	"STATUS_TIMEOUT",
	"CHECKSUM_MISMATCH",
	"DRIVE_ERROR",
	"DISC_ERROR",
	"DISC_CHANGED"
};

/* Utilities */

static void _copyString(char *output, const uint16_t *input, size_t length) {
	// The strings in the identification block are byte-swapped and padded with
	// spaces. To make them printable, any span of consecutive space characters
	// at the end is replaced with null bytes.
	bool isPadding = true;

	output += length;
	input  += length / 2;
	*output = 0;

	for (; length; length -= 2) {
		uint16_t packed = *(--input);
		char a = packed & 0xff, b = packed >> 8;

		if (isPadding && !__builtin_isgraph(a))
			a = 0;
		else
			isPadding = false;
		if (isPadding && !__builtin_isgraph(b))
			b = 0;
		else
			isPadding = false;

		*(--output) = a;
		*(--output) = b;
	}
}

static DeviceError _senseDataToError(const SenseData &data) {
	auto key = data.senseKey & 15;
	auto asc = data.getPackedASC();
	auto lba = data.getErrorLBA();

	LOG_IDE("%s", _SENSE_KEY_NAMES[key]);
	LOG_IDE("err=0x%02x, key=0x%02x", data.errorCode, data.senseKey);
	LOG_IDE("asc=0x%02x, ascq=0x%02x", data.asc, data.ascQualifier);

	if (lba) {
		LOG_IDE("lba=0x%08x", lba);
	}

	switch (key) {
		case SENSE_KEY_NO_SENSE:
		case SENSE_KEY_RECOVERED_ERROR:
			return NO_ERROR;

		case SENSE_KEY_NOT_READY:
			return (
				!asc ||
				(asc == ASC_NOT_READY) ||
				(asc == ASC_NOT_READY_IN_PROGRESS)
			)
				? NOT_YET_READY
				: DISC_ERROR;

		case SENSE_KEY_MEDIUM_ERROR:
		case SENSE_KEY_DATA_PROTECT:
			return DISC_ERROR;

		case SENSE_KEY_UNIT_ATTENTION:
			return (asc == ASC_RESET_OCCURRED)
				? NOT_YET_READY
				: DISC_CHANGED;

		case SENSE_KEY_ABORTED_COMMAND:
			return UNSUPPORTED_OP;

		default:
			return DRIVE_ERROR;
	}
}

bool IdentifyBlock::validateChecksum(void) const {
	if ((checksum & 0xff) != 0xa5)
		return true;

	// FIXME: is this right?
	uint8_t value = (-int(util::sum(
		reinterpret_cast<const uint8_t *>(&deviceFlags), ATA_SECTOR_SIZE - 1
	))) & 0xff;

	if (value != (checksum >> 8)) {
		LOG_IDE("mismatch, exp=0x%02x, got=0x%02x", value, checksum >> 8);
		return false;
	}

	return true;
}

int IdentifyBlock::getHighestPIOMode(void) const {
	if (timingValidityFlags & (1 << 1)) {
		if (pioModeFlags & (1 << 1))
			return 4;
		if (pioModeFlags & (1 << 0))
			return 3;
	}

	return 1;
}

/* Device class */

static constexpr int _DMA_TIMEOUT = 10000;

Device devices[2]{ (DEVICE_PRIMARY), (DEVICE_SECONDARY) };

Device::Device(uint32_t flags)
: flags(flags), capacity(0), lastStatusReg(0), lastErrorReg(0), lastCountReg(0)
{
	util::clear(lastSenseData);
}

void Device::_readPIO(void *data, size_t length) const {
	length++;
	length /= 2;

	util::assertAligned<uint16_t>(data);

	auto ptr = reinterpret_cast<uint16_t *>(data);

	for (; length > 0; length--)
		*(ptr++) = SYS573_IDE_CS0_BASE[CS0_DATA];
}

void Device::_writePIO(const void *data, size_t length) const {
	length++;
	length /= 2;

	util::assertAligned<uint16_t>(data);

	auto ptr = reinterpret_cast<const uint16_t *>(data);

	for (; length > 0; length--)
		SYS573_IDE_CS0_BASE[CS0_DATA] = *(ptr++);
}

bool Device::_readDMA(void *data, size_t length) const {
	length += 3;
	length /= 4;

	util::assertAligned<uint32_t>(data);

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	return waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);
}

bool Device::_writeDMA(const void *data, size_t length) const {
	length += 3;
	length /= 4;

	util::assertAligned<uint32_t>(data);

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	return waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);
}

/* Status and error polling */

static constexpr int _COMMAND_TIMEOUT = 30000000;
static constexpr int _DRQ_TIMEOUT     = 30000000;
static constexpr int _DETECT_TIMEOUT  = 2500000;

static constexpr int _SRST_SET_DELAY   = 5000;
static constexpr int _SRST_CLEAR_DELAY = 50000;

// Note that ATA drives will always assert DRDY when ready, but ATAPI drives
// will not. This is an intentional feature meant to prevent ATA-only drivers
// from misdetecting ATAPI drives.
DeviceError Device::_waitForIdle(bool drdy, int timeout, bool ignoreError) {
	if (!timeout)
		timeout = _COMMAND_TIMEOUT;

	for (; timeout > 0; timeout -= 10) {
		auto status = _read(CS0_STATUS);

		// Only check for errors *after* BSY is cleared.
		if (!(status & CS0_STATUS_BSY)) {
			if ((status & CS0_STATUS_ERR) && !ignoreError) {
				_handleError();
				return DRIVE_ERROR;
			}

			if ((status & CS0_STATUS_DRDY) || !drdy)
				return NO_ERROR;
		}

		delayMicroseconds(10);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG_IDE("timeout, ignore=%d", ignoreError);
	_handleTimeout();
	return STATUS_TIMEOUT;
}

DeviceError Device::_waitForDRQ(int timeout, bool ignoreError) {
	if (!timeout)
		timeout = _DRQ_TIMEOUT;

	for (; timeout > 0; timeout -= 10) {
		auto status = _read(CS0_STATUS);

		// Check for errors *before* DRQ is set but *after* BSY is cleared.
		// Confused yet?
		if (!(status & CS0_STATUS_BSY)) {
			if ((status & CS0_STATUS_ERR) && !ignoreError) {
				_handleError();
				return DRIVE_ERROR;
			}
		}

		if (status & CS0_STATUS_DRQ)
			return NO_ERROR;

		delayMicroseconds(10);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG_IDE("timeout, ignore=%d", ignoreError);
	_handleTimeout();
	return STATUS_TIMEOUT;
}

void Device::_handleError(void) {
	lastStatusReg = _read(CS0_STATUS);
	lastErrorReg  = _read(CS0_ERROR);
	lastCountReg  = _read(CS0_COUNT);

	LOG_IDE(
		"%d, st=0x%02x, err=0x%02x, cnt=0x%02x", getDriveIndex(), lastStatusReg,
		lastErrorReg, lastCountReg
	);

	// Issuing a device reset command to an ATAPI drive would result in the
	// error's sense data being lost.
#if 0
	if (flags & DEVICE_ATAPI)
		_write(CS0_COMMAND, ATA_DEVICE_RESET);
#endif
}

void Device::_handleTimeout(void) {
	lastStatusReg = _read(CS0_STATUS);
	lastErrorReg  = _read(CS0_ERROR);
	lastCountReg  = _read(CS0_COUNT);

	LOG_IDE(
		"%d, st=0x%02x, err=0x%02x, cnt=0x%02x", getDriveIndex(), lastStatusReg,
		lastErrorReg, lastCountReg
	);

	if (flags & DEVICE_ATAPI)
		_write(CS0_COMMAND, ATA_DEVICE_RESET);
}

DeviceError Device::_resetDrive(void) {
	// Issue a software reset, which affects both devices on the bus.
	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN | CS1_DEVICE_CTRL_SRST);
	delayMicroseconds(_SRST_SET_DELAY);
	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN);
	delayMicroseconds(_SRST_CLEAR_DELAY);

	_select(0);

	if (_waitForIdle(false, _DETECT_TIMEOUT, true)) {
		LOG_IDE("drive %d select timeout", getDriveIndex());
		return NO_DRIVE;
	}

#ifndef ENABLE_FULL_IDE_DRIVER
	io::clearWatchdog();
#endif

#if 0
	// Issue dummy writes to the sector count register and attempt to read back
	// the written value. This should not fail even if the drive is busy.
	uint8_t pattern = 0x55;

	for (int timeout = _DETECT_TIMEOUT; timeout > 0; timeout -= 10) {
		_write(CS0_COUNT, pattern);

		if (_read(CS0_COUNT) == pattern) {
			_write(CS0_COUNT, 0);
			return NO_ERROR;
		}

		pattern ^= 0xff;

		delayMicroseconds(10);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG_IDE("drive %d not responding", getDriveIndex());
	return NO_DRIVE;
#else
	return NO_ERROR;
#endif
}

/* ATA-specific function */

DeviceError Device::_ataSetLBA(uint64_t lba, size_t count, int timeout) {
	if (flags & DEVICE_HAS_LBA48) {
		//assert(lba < (1ULL << 48));
		//assert(count <= (1 << 16));
		_select(CS0_DEVICE_SEL_LBA);

		auto error = _waitForIdle(true);

		if (error)
			return error;

		_write(CS0_COUNT,      (count >>  8) & 0xff);
		_write(CS0_SECTOR,     (lba   >> 24) & 0xff);
		_write(CS0_CYLINDER_L, (lba   >> 32) & 0xff);
		_write(CS0_CYLINDER_H, (lba   >> 40) & 0xff);
	} else {
		//assert(lba < (1ULL << 28));
		//assert(count <= (1 << 8));
		_select(CS0_DEVICE_SEL_LBA | ((lba >> 24) & 15));

		auto error = _waitForIdle(true);

		if (error)
			return error;
	}

	_write(CS0_COUNT,      (count >>  0) & 0xff);
	_write(CS0_SECTOR,     (lba   >>  0) & 0xff);
	_write(CS0_CYLINDER_L, (lba   >>  8) & 0xff);
	_write(CS0_CYLINDER_H, (lba   >> 16) & 0xff);
	return NO_ERROR;
}

DeviceError Device::_ataTransfer(
	uintptr_t ptr, uint64_t lba, size_t count, bool write
) {
	uint8_t cmd;
	size_t  maxLength;

	if (flags & DEVICE_HAS_LBA48) {
		cmd       = write ? ATA_WRITE_SECTORS_EXT : ATA_READ_SECTORS_EXT;
		maxLength = 1 << 16;
	} else {
		cmd       = write ? ATA_WRITE_SECTORS : ATA_READ_SECTORS;
		maxLength = 1 << 8;
	}

	while (count) {
		size_t chunkLength = util::min(count, maxLength);

		auto error = _ataSetLBA(lba, chunkLength);

		if (error)
			return error;

		_write(CS0_COMMAND, cmd);

		// Data must be transferred one sector at a time as the drive may
		// deassert DRQ between sectors.
		for (size_t i = chunkLength; i; i--, ptr += ATA_SECTOR_SIZE) {
			auto error = _waitForDRQ();

			if (error)
				return error;

			if (write)
				_writePIO(reinterpret_cast<const void *>(ptr), ATA_SECTOR_SIZE);
			else
				_readPIO(reinterpret_cast<void *>(ptr), ATA_SECTOR_SIZE);
		}

		lba   += chunkLength;
		count -= chunkLength;
	}

	return _waitForIdle();
}

/* ATAPI-specific functions */

static constexpr int _ATAPI_READY_TIMEOUT = 30000000;
static constexpr int _ATAPI_POLL_DELAY    = 500000;
static constexpr int _REQ_SENSE_TIMEOUT   = 500000;

// ATAPI devices will set the CHK (ERR) status flag whenever new sense data is
// available in response to a command. In such cases, the error should be
// cleared by sending a "request sense" command.
DeviceError Device::_atapiRequestSense(void) {
	Packet packet;

	packet.setRequestSense();
	_select(0);

	auto error = _waitForIdle(false, _REQ_SENSE_TIMEOUT, true);

	if (!error) {
		_write(CS0_FEATURES, 0);
#if 0
		_setCylinder(sizeof(SenseData));
#else
		_setCylinder(ATAPI_SECTOR_SIZE);
#endif
		_write(CS0_COMMAND, ATA_PACKET);

		error = _waitForDRQ(_REQ_SENSE_TIMEOUT, true);
	}
	if (!error) {
		_writePIO(&packet, getPacketSize());

		error = _waitForDRQ(_REQ_SENSE_TIMEOUT, true);
	}

	util::clear(lastSenseData);

	if (!error) {
		size_t length = _getCylinder();

		_readPIO(&lastSenseData, length);
		LOG_IDE("data ok, length=0x%x", length);
	} else {
		// If the request sense command fails, fall back to reading the sense
		// key from the error register.
		lastSenseData.senseKey = lastErrorReg >> 4;

		LOG_IDE("%s", getErrorString(error));
		_write(CS0_COMMAND, ATA_DEVICE_RESET);
	}

	return _senseDataToError(lastSenseData);
}

DeviceError Device::_atapiPacket(const Packet &packet, size_t dataLength) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (!(flags & DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	LOG_IDE("cmd=0x%02x, length=0x%x", packet.command, dataLength);

	// Keep resending the command as long as the drive reports it is in progress
	// of becoming ready (i.e. spinning up).
	for (
		int timeout = _ATAPI_READY_TIMEOUT; timeout > 0;
		timeout -= _ATAPI_POLL_DELAY
	) {
		_select(0);

		auto error = _waitForIdle();

		if (!error) {
			_write(CS0_FEATURES, 0);
#if 0
			_setCylinder(dataLength);
#else
			_setCylinder(ATAPI_SECTOR_SIZE);
#endif
			_write(CS0_COMMAND, ATA_PACKET);

			error = _waitForDRQ();
		}
		if (!error) {
			_writePIO(&packet, getPacketSize());

			error = dataLength
				? _waitForDRQ()
				: _waitForIdle();
		}
		if (!error)
			return NO_ERROR;

		// If an error occurred, fetch sense data to determine whether to resend
		// the command.
		LOG_IDE("%s, cmd=0x%02x", getErrorString(error), packet.command);

		error = _atapiRequestSense();

		if (error && (error != NOT_YET_READY)) {
			LOG_IDE("%s (from sense)", getErrorString(error));
			return error;
		}

		delayMicroseconds(_ATAPI_POLL_DELAY);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG_IDE("retry timeout, cmd=0x%02x", packet.command);
	return STATUS_TIMEOUT;
}

DeviceError Device::_atapiRead(uintptr_t ptr, uint32_t lba, size_t count) {
	Packet packet;

	packet.setRead(lba, count);

	auto error = _atapiPacket(packet, ATAPI_SECTOR_SIZE);

	if (error)
		return error;

	// Data must be transferred one sector at a time as the drive may deassert
	// DRQ between sectors.
	for (; count; count--) {
		auto error = _waitForDRQ();

		if (error)
			return error;

		size_t chunkLength = _getCylinder();

		_readPIO(reinterpret_cast<void *>(ptr), chunkLength);
		ptr += chunkLength;
	}

	return _waitForIdle();
}

/* Public API */

static constexpr uint16_t _ATAPI_SIGNATURE = 0xeb14;

DeviceError Device::enumerate(void) {
	flags &= DEVICE_PRIMARY | DEVICE_SECONDARY;

	auto error = _resetDrive();

	if (error)
		return error;

	// Check whether the ATAPI signature is present and fetch the appropriate
	// identification block.
	// NOTE: the primary drive may respond to all secondary drive register
	// accesses, with the exception of command writes, if no secondary drive is
	// actually present. A strict timeout is used in the commands below in order
	// to prevent blocking for too long.
	IdentifyBlock block;
	auto          signature = _getCylinder();

	if (signature == _ATAPI_SIGNATURE) {
		flags |= DEVICE_ATAPI;

		_write(CS0_COMMAND, ATA_IDENTIFY_PACKET);
	} else {
		_write(CS0_COMMAND, ATA_IDENTIFY);
	}

	if (_waitForDRQ(_DETECT_TIMEOUT))
		return NO_DRIVE;

	_readPIO(&block, sizeof(IdentifyBlock));

	if (!block.validateChecksum())
		return CHECKSUM_MISMATCH;

	// Parse the identification block.
	if (flags & DEVICE_ATAPI) {
		LOG_IDE("ATAPI drive found");

		if (
			(block.deviceFlags & IDENTIFY_DEV_ATAPI_TYPE_BITMASK)
			== IDENTIFY_DEV_ATAPI_TYPE_CDROM
		)
			flags |= DEVICE_READ_ONLY | DEVICE_CDROM;
		if (
			(block.deviceFlags & IDENTIFY_DEV_PACKET_LENGTH_BITMASK)
			== IDENTIFY_DEV_PACKET_LENGTH_16
		)
			flags |= DEVICE_HAS_PACKET16;
	} else {
		LOG_IDE("ATA drive found");

		if (block.commandSetFlags[1] & (1 << 10)) {
			flags   |= DEVICE_HAS_LBA48;
			capacity = block.getSectorCountExt();
		} else {
			capacity = block.getSectorCount();
		}
		if (block.commandSetFlags[1] & (1 << 12))
			flags |= DEVICE_HAS_FLUSH;
	}

#ifdef ENABLE_FULL_IDE_DRIVER
	_copyString(model,        block.model,        sizeof(block.model));
	_copyString(revision,     block.revision,     sizeof(block.revision));
	_copyString(serialNumber, block.serialNumber, sizeof(block.serialNumber));
#endif

	// Find out the fastest PIO transfer mode supported and enable it.
	int mode = block.getHighestPIOMode();

	_select(0);

	error = _waitForIdle();

	if (error)
		return error;

	_write(CS0_FEATURES, FEATURE_TRANSFER_MODE);
	_write(CS0_COUNT,    TRANSFER_MODE_PIO | mode);
	_write(CS0_COMMAND,  ATA_SET_FEATURES);

	error = _waitForIdle();

	if (error)
		return error;

	LOG_IDE("drive %d ready, mode=PIO%d", getDriveIndex(), mode);
	flags |= DEVICE_READY;

	// Make sure any pending ATAPI sense data is cleared.
	do {
		error = poll();
	} while ((error == ide::NOT_YET_READY) || (error == ide::DISC_CHANGED));

	return error;
}

DeviceError Device::poll(void) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;

	if (flags & DEVICE_ATAPI) {
		Packet packet;

		packet.setTestUnitReady();
		return _atapiPacket(packet);
	} else {
		_select(CS0_DEVICE_SEL_LBA);
		return _waitForIdle(true);
	}
}

void Device::handleInterrupt(void) {
	// TODO: use interrupts to yield instead of busy waiting
}

DeviceError Device::readData(void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!(flags & DEVICE_READY))
		return NO_DRIVE;

	if (flags & DEVICE_ATAPI)
		return _atapiRead(
			reinterpret_cast<uintptr_t>(data), static_cast<uint32_t>(lba), count
		);
	else
		return _ataTransfer(
			reinterpret_cast<uintptr_t>(data), lba, count, false
		);
}

DeviceError Device::writeData(const void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (flags & (DEVICE_READ_ONLY | DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	return _ataTransfer(reinterpret_cast<uintptr_t>(data), lba, count, true);
}

#ifdef ENABLE_FULL_IDE_DRIVER

DeviceError Device::goIdle(bool standby) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (flags & DEVICE_ATAPI)
		return startStopUnit(START_STOP_MODE_STOP_DISC);

	_select(CS0_DEVICE_SEL_LBA);

	auto error = _waitForIdle(true);

	if (error)
		return error;

	_write(CS0_COMMAND, standby ? ATA_STANDBY_IMMEDIATE : ATA_IDLE_IMMEDIATE);
	return _waitForIdle();
}

DeviceError Device::startStopUnit(ATAPIStartStopMode mode) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (!(flags & DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	Packet packet;

	packet.setStartStopUnit(mode);
	return _atapiPacket(packet);
}

DeviceError Device::flushCache(void) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (!(flags & DEVICE_HAS_FLUSH))
#if 0
		return UNSUPPORTED_OP;
#else
		return NO_ERROR;
#endif

	_select(CS0_DEVICE_SEL_LBA);

	auto error = _waitForIdle(true);

	if (error)
		return error;

	_write(
		CS0_COMMAND,
		(flags & DEVICE_HAS_LBA48) ? ATA_FLUSH_CACHE_EXT : ATA_FLUSH_CACHE
	);
	return _waitForIdle();
}

#endif

}
