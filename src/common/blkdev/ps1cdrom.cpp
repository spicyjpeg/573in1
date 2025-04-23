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
#include "common/blkdev/device.hpp"
#include "common/blkdev/ps1cdrom.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/cdrom.h"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace blkdev {

static constexpr size_t _SECTOR_LENGTH = 2048;

static const char *const _IRQ_NAMES[]{
	"NONE",
	"DATA_READY",
	"COMPLETE",
	"ACKNOWLEDGE",
	"DATA_END",
	"ERROR"
};

/* Utilities */

static constexpr int _DMA_TIMEOUT = 100000;

static DeviceError _statusToError(const uint8_t *status) {
	if (status[0] & (0
		| CDROM_CMD_STAT_ERROR
		| CDROM_CMD_STAT_SEEK_ERROR
		| CDROM_CMD_STAT_ID_ERROR
	)) {
		LOG_BLKDEV("stat=0x%02x, err=0x%02x", status[0], status[1]);

		if (status[1] & (0
			| CDROM_CMD_ERR_INVALID_PARAM_VALUE
			| CDROM_CMD_ERR_INVALID_PARAM_COUNT
			| CDROM_CMD_ERR_INVALID_COMMAND
		))
			return COMMAND_ERROR;
		if (status[1] & CDROM_CMD_ERR_LID_OPENED)
			return DISC_CHANGED;
		// CDROM_CMD_ERR_NO_DISC is supposed to be mapped to NOT_YET_READY,
		// however there is no way to tell whether the drive is currently idle
		// or busy detecting the disc.
		if (status[1] & (0
			| CDROM_CMD_ERR_SEEK_FAILED
			| CDROM_CMD_ERR_NO_DISC
		))
			return DISC_ERROR;
	}

	if (status[0] & CDROM_CMD_STAT_LID_OPEN)
		return DISC_CHANGED;

	return NO_ERROR;
}

static void _readData(void *data, size_t length) {
	length = (length + 3) / 4;

	util::assertAligned<uint32_t>(data);

	if (!waitForDMATransfer(DMA_CDROM, _DMA_TIMEOUT))
		return;

	CDROM_ADDRESS = 0;
	CDROM_HCHPCTL = 0;
	CDROM_HCHPCTL = CDROM_HCHPCTL_BFRD;

	DMA_MADR(DMA_CDROM) = uint32_t(data);
	DMA_BCR (DMA_CDROM) = length;
	DMA_CHCR(DMA_CDROM) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	waitForDMATransfer(DMA_CDROM, _DMA_TIMEOUT);
}

/* PS1 CD-ROM block device class */

static constexpr int _ACKNOWLEDGE_TIMEOUT = 100000;
static constexpr int _COMPLETE_TIMEOUT    = 10000000;

PS1CDROMDevice cdrom;

DeviceError PS1CDROMDevice::_waitForIRQ(CDROMIRQType type) {
	int timeout = (type == CDROM_IRQ_ACKNOWLEDGE)
		? _ACKNOWLEDGE_TIMEOUT
		: _COMPLETE_TIMEOUT;

	if (!waitForInterrupt(IRQ_CDROM, timeout))
		return STATUS_TIMEOUT;

	// A delay is required in order for the flags to stabilize on older console
	// revisions that run the CD-ROM microcontroller from an independent clock.
	delayMicroseconds(1);

	CDROM_ADDRESS  = 1;
	int actualType = CDROM_HINTSTS & CDROM_HINT_INT_BITMASK;
	CDROM_HCLRCTL  = CDROM_HCLRCTL_CLRINT_BITMASK;

	lastStatusLength = 0;

	while (CDROM_HSTS & CDROM_HSTS_RSLRRDY)
		lastStatusData[lastStatusLength++] = CDROM_RESULT;

	if (actualType == CDROM_IRQ_ERROR)
		return _statusToError(lastStatusData);
	if (actualType == type)
		return NO_ERROR;

	LOG_BLKDEV("exp=%s, got=%s", _IRQ_NAMES[type], _IRQ_NAMES[actualType]);
	return COMMAND_ERROR;
}

DeviceError PS1CDROMDevice::_issueCommand(
	uint8_t       cmd,
	const uint8_t *param,
	size_t        paramLength,
	bool          waitForComplete
) {
	while (CDROM_HSTS & CDROM_HSTS_BUSYSTS)
		__asm__ volatile("");
	while (CDROM_HSTS & CDROM_HSTS_RSLRRDY)
		CDROM_RDDATA;

	CDROM_ADDRESS = 1;
	CDROM_HCLRCTL = 0
		| CDROM_HCLRCTL_CLRINT_BITMASK
		| CDROM_HCLRCTL_CLRPRM;

	// TODO: this delay is likely superfluous
	delayMicroseconds(1);

	CDROM_ADDRESS = 0;

	for (; paramLength > 0; paramLength--)
		CDROM_PARAMETER = *(param++);

	CDROM_COMMAND = cmd;
	auto error    = _waitForIRQ(CDROM_IRQ_ACKNOWLEDGE);

	if (error)
		return error;

	if (waitForComplete) {
		error = _waitForIRQ(CDROM_IRQ_COMPLETE);

		if (error)
			return error;
	}

	return NO_ERROR;
}

DeviceError PS1CDROMDevice::_startRead(uint32_t lba) {
	BCDMSF        msf;
	const uint8_t mode = 0
		| CDROM_MODE_SIZE_2048
		| CDROM_MODE_SPEED_2X;

	msf.fromLBA(lba);

	auto error = _issueCommand(
		CDROM_CMD_SETLOC,
		reinterpret_cast<const uint8_t *>(&msf),
		sizeof(msf)
	);

	if (error)
		return error;

	error = _issueCommand(CDROM_CMD_SETMODE, &mode, sizeof(mode));

	if (error)
		return error;

	return _issueCommand(CDROM_CMD_READ_N);
}

bool PS1CDROMDevice::_issueUnlock(void) {
	// Determine the drive's region then issue unlock commands in order to allow
	// for any disc to be read. This is only supported by non-Japanese consoles.
	const uint8_t cmd = CDROM_TEST_GET_REGION;

	if (_issueCommand(CDROM_CMD_TEST, &cmd, sizeof(cmd))) {
		LOG_BLKDEV("drive region read failed");
		return false;
	}

	lastStatusData[lastStatusLength] = 0;
	LOG_BLKDEV("drive region: %s", lastStatusData);

	const char *unlock[]{
		"",
		"Licensed by",
		"Sony",
		"Computer",
		"Entertainment",
		nullptr,
		""
	};

	switch (util::hash(lastStatusData, lastStatusLength)) {
#if 0
		case "for Japan"_h:
			unlock[5] = "Inc.";
			break;
#endif

		case "for U/C"_h:
			unlock[5] = "of America";
			break;

		case "for Europe"_h:
			unlock[5] = "(Europe)";
			break;

		case "for NETNA"_h:
		case "for NETEU"_h:
			unlock[5] = "World wide";
			break;

		case "for US/AEP"_h:
			return true;

		default:
			return false;
	}

	// Note that the unlock commands will always return an error.
	for (size_t i = 0; i < util::countOf(unlock); i++)
		_issueCommand(
			CDROM_CMD_UNLOCK0 + i,
			reinterpret_cast<const uint8_t *>(unlock[i]),
			__builtin_strlen(unlock[i])
		);

	return true;
}

DeviceError PS1CDROMDevice::enumerate(void) {
	BIU_DEV5_CTRL = 0
		| (3 << 0) // Write delay
		| (4 << 4) // Read delay
		| BIU_CTRL_RECOVERY
		| BIU_CTRL_PRESTROBE
		| BIU_CTRL_WIDTH_8
		| (2 << 16); // Number of address lines
	DMA_DPCR     |= DMA_DPCR_CH_ENABLE(DMA_CDROM);

	// Ensure the CD-ROM is actually available (i.e. we're not running on a 573)
	// by checking for the bank switch register before proceeding.
	for (int i = 0; i < 4; i++) {
		CDROM_ADDRESS = i;

		if ((CDROM_HSTS & CDROM_HSTS_RA_BITMASK) != i)
			return NO_DRIVE;
	}

	CDROM_ADDRESS   = 1;
	CDROM_HCLRCTL   = 0
		| CDROM_HCLRCTL_CLRINT_BITMASK
		| CDROM_HCLRCTL_CLRBFEMPT
		| CDROM_HCLRCTL_CLRBFWRDY
		| CDROM_HCLRCTL_SMADPCLR
		| CDROM_HCLRCTL_CLRPRM;
	CDROM_HINTMSK_W = CDROM_HINT_INT_BITMASK;

	CDROM_ADDRESS = 0;
	CDROM_HCHPCTL = 0;

	auto error = _issueCommand(CDROM_CMD_INIT, nullptr, 0, true);

	if (error)
		return error;

	type         = PS1_CDROM;
	flags       |= READ_ONLY;
	sectorLength = _SECTOR_LENGTH;

	_issueUnlock();
	return poll();
}

DeviceError PS1CDROMDevice::poll(void) {
	return _issueCommand(CDROM_CMD_NOP);
}

void PS1CDROMDevice::handleInterrupt(void) {
	// TODO: use interrupts to yield instead of busy waiting
}

DeviceError PS1CDROMDevice::read(void *data, uint64_t lba, size_t count) {
	auto error = _startRead(uint32_t(lba));

	if (error)
		return error;

	auto ptr = uintptr_t(data);

	for (; count > 0; count--) {
		error = _waitForIRQ(CDROM_IRQ_DATA_READY);

		if (error)
			break;

		_readData(reinterpret_cast<void *>(ptr), _SECTOR_LENGTH);
		ptr += _SECTOR_LENGTH;
	}

	auto pauseError = _issueCommand(CDROM_CMD_PAUSE, nullptr, 0, true);

	return error ? error : pauseError;
}

DeviceError PS1CDROMDevice::readStream(
	StreamCallback callback,
	uint64_t       lba,
	size_t         count,
	void           *arg
) {
	auto error = _startRead(uint32_t(lba));

	if (error)
		return error;

	for (; count > 0; count--) {
		error = _waitForIRQ(CDROM_IRQ_DATA_READY);

		if (error)
			break;

		uint8_t sector[_SECTOR_LENGTH];

		_readData(sector, _SECTOR_LENGTH);
		callback(sector, _SECTOR_LENGTH, arg);
	}

	auto pauseError = _issueCommand(CDROM_CMD_PAUSE, nullptr, 0, true);

	return error ? error : pauseError;
}

DeviceError PS1CDROMDevice::goIdle(bool standby) {
	return _issueCommand(CDROM_CMD_STOP, nullptr, 0, true);
}

}
