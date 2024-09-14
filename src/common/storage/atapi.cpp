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
#include "common/storage/ata.hpp"
#include "common/storage/atapi.hpp"
#include "common/storage/device.hpp"
#include "common/util/log.hpp"
#include "ps1/system.h"

/*
 * Based on the following specifications:
 *
 * - "AT Attachment with Packet Interface - 6", 2001-06-26
 * - SFF-8020i "ATA Packet Interface for CD-ROMs 2.6", 1996-01-22 (seems to be
 *   rather inaccurate about the IDE side of things, but some drives actually
 *   implement those inaccuracies!)
 *
 * https://web.archive.org/web/20060427142409/http://www.stanford.edu/~csapuntz/blackmagic.html
 */

namespace storage {

static constexpr size_t _SECTOR_LENGTH = 2048;

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

/* Utilities */

static DeviceError _senseDataToError(const ATAPISenseData &data) {
	auto key = data.senseKey & 15;
	auto asc = data.getPackedASC();
	auto lba = data.getErrorLBA();

	LOG_STORAGE("%s", _SENSE_KEY_NAMES[key]);
	LOG_STORAGE("err=0x%02x, key=0x%02x", data.errorCode, data.senseKey);
	LOG_STORAGE("asc=0x%02x, ascq=0x%02x", data.asc, data.ascQualifier);

	if (lba) {
		LOG_STORAGE("lba=0x%08x", lba);
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

/* ATAPI error handling */

static constexpr int _ATAPI_READY_TIMEOUT = 30000000;
static constexpr int _ATAPI_POLL_DELAY    = 500000;
static constexpr int _REQ_SENSE_TIMEOUT   = 500000;

// ATAPI devices will set the CHK (ERR) status flag whenever new sense data is
// available in response to a command. In such cases, the error should be
// cleared by sending a "request sense" command.
DeviceError ATAPIDevice::_requestSense(void) {
	ATAPIPacket packet;

	packet.setRequestSense();
	_select(0);

	auto error = _waitForIdle(false, _REQ_SENSE_TIMEOUT, true);

	if (!error) {
		_set(CS0_FEATURES, 0);
#if 0
		_setCylinder(sizeof(SenseData));
#else
		_setCylinder(_SECTOR_LENGTH);
#endif
		_set(CS0_COMMAND, ATA_PACKET);

		error = _waitForDRQ(_REQ_SENSE_TIMEOUT, true);
	}
	if (!error) {
		_writePIO(&packet, _getPacketLength());

		error = _waitForDRQ(_REQ_SENSE_TIMEOUT, true);
	}

	util::clear(lastSenseData);

	if (!error) {
		size_t length = _getCylinder();

		_readPIO(&lastSenseData, length);
		LOG_STORAGE("data ok, length=0x%x", length);
	} else {
		// If the request sense command fails, fall back to reading the sense
		// key from the error register.
		lastSenseData.senseKey = _lastErrorReg >> 4;

		LOG_STORAGE("%s", getErrorString(error));
		_set(CS0_COMMAND, ATA_DEVICE_RESET);
	}

	return _senseDataToError(lastSenseData);
}

DeviceError ATAPIDevice::_issuePacket(
	const ATAPIPacket &packet, size_t dataLength
) {
	if (!type)
		return NO_DRIVE;

	LOG_STORAGE("cmd=0x%02x, length=0x%x", packet.command, dataLength);

	// Keep resending the command as long as the drive reports it is in progress
	// of becoming ready (i.e. spinning up).
	for (
		int timeout = _ATAPI_READY_TIMEOUT; timeout > 0;
		timeout -= _ATAPI_POLL_DELAY
	) {
		_select(0);

		auto error = _waitForIdle();

		if (!error) {
			_set(CS0_FEATURES, 0);
#if 0
			_setCylinder(dataLength);
#else
			_setCylinder(_SECTOR_LENGTH);
#endif
			_set(CS0_COMMAND, ATA_PACKET);

			error = _waitForDRQ();
		}
		if (!error) {
			_writePIO(&packet, _getPacketLength());

			error = dataLength
				? _waitForDRQ()
				: _waitForIdle();
		}
		if (!error)
			return NO_ERROR;

		// If an error occurred, fetch sense data to determine whether to resend
		// the command.
		LOG_STORAGE("%s, cmd=0x%02x", getErrorString(error), packet.command);

		error = _requestSense();

		if (error && (error != NOT_YET_READY)) {
			LOG_STORAGE("%s (from sense)", getErrorString(error));
			return error;
		}

		delayMicroseconds(_ATAPI_POLL_DELAY);
	}

	LOG_STORAGE("retry timeout, cmd=0x%02x", packet.command);
	return STATUS_TIMEOUT;
}

/* ATAPI block device class */

static constexpr int _DETECT_TIMEOUT = 2500000;

DeviceError ATAPIDevice::enumerate(void) {
	// NOTE: the primary drive may respond to all secondary drive register
	// accesses, with the exception of command writes, if no secondary drive is
	// actually present. A strict timeout is used in the commands below in order
	// to prevent blocking for too long.
	IDEIdentifyBlock block;

	_set(CS0_COMMAND, ATA_IDENTIFY_PACKET);

	if (_waitForDRQ(_DETECT_TIMEOUT))
		return NO_DRIVE;

	_readPIO(&block, sizeof(IDEIdentifyBlock));

	if (!block.validateChecksum())
		return CHECKSUM_MISMATCH;

	if (
		(block.deviceFlags & IDE_IDENTIFY_DEV_ATAPI_TYPE_BITMASK)
		!= IDE_IDENTIFY_DEV_ATAPI_TYPE_CDROM
	) {
		LOG_STORAGE("ignoring non-CD-ROM drive %d", _getDriveIndex());
		return UNSUPPORTED_OP;
	}

	// TODO: actually fetch the capacity from the drive
	type         = ATAPI;
	flags        = READ_ONLY;
	capacity     = 0;
	sectorLength = _SECTOR_LENGTH;

	if (
		(block.deviceFlags & IDE_IDENTIFY_DEV_PACKET_LENGTH_BITMASK)
		== IDE_IDENTIFY_DEV_PACKET_LENGTH_16
	)
		flags |= REQUIRES_EXT_PACKET;

	LOG_STORAGE("drive %d is ATAPI", _getDriveIndex());
	return _setup(block);
}

DeviceError ATAPIDevice::poll(void) {
	if (!type)
		return NO_DRIVE;

	ATAPIPacket packet;

	packet.setTestUnitReady();
	return _issuePacket(packet);
}

void ATAPIDevice::handleInterrupt(void) {
	// TODO: use interrupts to yield instead of busy waiting
}

DeviceError ATAPIDevice::read(void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!type)
		return NO_DRIVE;

	ATAPIPacket packet;

	packet.setRead(lba, count);

	auto error = _issuePacket(packet, _SECTOR_LENGTH);

	if (error)
		return error;

	// Data must be transferred one sector at a time as the drive may deassert
	// DRQ between sectors.
	auto ptr = reinterpret_cast<uintptr_t>(data);

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

DeviceError ATAPIDevice::goIdle(bool standby) {
	if (!type)
		return NO_DRIVE;

	ATAPIPacket packet;

	packet.setStartStopUnit(START_STOP_MODE_STOP_SPINDLE);
	return _issuePacket(packet);
}

DeviceError ATAPIDevice::eject(bool close) {
	if (!type)
		return NO_DRIVE;

	ATAPIPacket packet;
	auto        mode =
		close ? START_STOP_MODE_CLOSE_TRAY : START_STOP_MODE_OPEN_TRAY;

	packet.setStartStopUnit(mode);
	return _issuePacket(packet);
}

}
