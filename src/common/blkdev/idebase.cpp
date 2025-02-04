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
#include "common/blkdev/atapi.hpp"
#include "common/blkdev/device.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace blkdev {

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

/* IDE identification block utilities */

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

bool IDEIdentifyBlock::validateChecksum(void) const {
	if ((checksum & 0xff) != 0xa5)
		return true;

	// FIXME: is this right?
	uint8_t value = (-int(util::sum(
		reinterpret_cast<const uint8_t *>(&deviceFlags),
		sizeof(IDEIdentifyBlock) - 1
	))) & 0xff;

	if (value != (checksum >> 8)) {
		LOG_BLKDEV("mismatch, exp=0x%02x, got=0x%02x", value, checksum >> 8);
		return false;
	}

	return true;
}

int IDEIdentifyBlock::getHighestPIOMode(void) const {
	if (timingValidityFlags & (1 << 1)) {
		if (pioModeFlags & (1 << 1))
			return 4;
		if (pioModeFlags & (1 << 0))
			return 3;
	}

	return 1;
}

/* IDE data transfers */

void IDEDevice::_readData(void *data, size_t length) const {
#if 0
	sys573::doDMARead(&SYS573_IDE_CS0_BASE[CS0_DATA], data, length);
#else
	length = (length + 1) / 2;

	util::assertAligned<uint16_t>(data);

	auto ptr = reinterpret_cast<uint16_t *>(data);

	for (; length > 0; length--)
		*(ptr++) = SYS573_IDE_CS0_BASE[CS0_DATA];
#endif
}

void IDEDevice::_writeData(const void *data, size_t length) const {
#if 0
	sys573::doDMAWrite(&SYS573_IDE_CS0_BASE[CS0_DATA], data, length);
#else
	length = (length + 1) / 2;

	util::assertAligned<uint16_t>(data);

	auto ptr = reinterpret_cast<const uint16_t *>(data);

	for (; length > 0; length--)
		SYS573_IDE_CS0_BASE[CS0_DATA] = *(ptr++);
#endif
}

/* IDE status and error polling */

static constexpr int _COMMAND_TIMEOUT = 30000000;
static constexpr int _DRQ_TIMEOUT     = 30000000;
static constexpr int _DETECT_TIMEOUT  = 2500000;

DeviceError IDEDevice::_setup(const IDEIdentifyBlock &block) {
	_copyString(model,        block.model,        sizeof(block.model));
	_copyString(revision,     block.revision,     sizeof(block.revision));
	_copyString(serialNumber, block.serialNumber, sizeof(block.serialNumber));

	// Find out the fastest PIO transfer mode supported and enable it.
	int mode = block.getHighestPIOMode();

	_select(0);

	auto error = _waitForIdle();

	if (error)
		return error;

	_set(CS0_FEATURES, ATA_FEATURE_TRANSFER_MODE);
	_set(CS0_COUNT,    ATA_TRANSFER_MODE_PIO | mode);
	_set(CS0_COMMAND,  ATA_SET_FEATURES);

	error = _waitForIdle();

	if (error)
		return error;

	LOG_BLKDEV("drive %d ready, mode=PIO%d", getDeviceIndex(), mode);

	// Make sure any pending ATAPI sense data is cleared.
	do {
		error = poll();
	} while ((error == NOT_YET_READY) || (error == DISC_CHANGED));

	return error;
}

// Note that ATA drives will always assert DRDY when ready, but ATAPI drives
// will not. This is an intentional feature meant to prevent ATA-only drivers
// from misdetecting ATAPI drives.
DeviceError IDEDevice::_waitForIdle(bool drdy, int timeout, bool ignoreError) {
	if (!timeout)
		timeout = _COMMAND_TIMEOUT;

	for (; timeout >= 0; timeout -= 10) {
		auto status = _get(CS0_STATUS);

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
	}

	LOG_BLKDEV("timeout, ignore=%d", ignoreError);
	_handleError();
	return STATUS_TIMEOUT;
}

DeviceError IDEDevice::_waitForDRQ(int timeout, bool ignoreError) {
	if (!timeout)
		timeout = _DRQ_TIMEOUT;

	for (; timeout >= 0; timeout -= 10) {
		auto status = _get(CS0_STATUS);

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
	}

	LOG_BLKDEV("timeout, ignore=%d", ignoreError);
	_handleError();
	return STATUS_TIMEOUT;
}

void IDEDevice::_handleError(void) {
	_lastStatusReg = _get(CS0_STATUS);
	_lastErrorReg  = _get(CS0_ERROR);
	_lastCountReg  = _get(CS0_COUNT);

	LOG_BLKDEV(
		"%d, st=0x%02x, err=0x%02x, cnt=0x%02x",
		getDeviceIndex(),
		_lastStatusReg,
		_lastErrorReg,
		_lastCountReg
	);

	// Issuing a device reset command to an ATAPI drive would result in the
	// error's sense data being lost.
#if 0
	_set(CS0_COMMAND, ATA_DEVICE_RESET);
#endif
}

/* Device constructor */

enum IDESignature : uint16_t {
	_SIG_PARALLEL_ATA   = util::concat2(0x00, 0x00),
	_SIG_PARALLEL_ATAPI = util::concat2(0x14, 0xeb),
	_SIG_SERIAL_ATA     = util::concat2(0x3c, 0xc3),
	_SIG_SERIAL_ATAPI   = util::concat2(0x69, 0x96)
};

static constexpr int _SRST_SET_DELAY   = 5000;
static constexpr int _SRST_CLEAR_DELAY = 50000;

IDEDevice *newIDEDevice(int index) {
#if 0
	SYS573_IDE_CS1_BASE[CS1_DEVICE_CTRL] =
		CS1_DEVICE_CTRL_IEN | CS1_DEVICE_CTRL_SRST;
	delayMicroseconds(_SRST_SET_DELAY);
	SYS573_IDE_CS1_BASE[CS1_DEVICE_CTRL] = CS1_DEVICE_CTRL_IEN;
	delayMicroseconds(_SRST_CLEAR_DELAY);
#endif

	if (index)
		SYS573_IDE_CS0_BASE[CS0_DEVICE_SEL] = CS0_DEVICE_SEL_SECONDARY;
	else
		SYS573_IDE_CS0_BASE[CS0_DEVICE_SEL] = CS0_DEVICE_SEL_PRIMARY;

	for (int timeout = _DETECT_TIMEOUT; timeout >= 0; timeout -= 10) {
		if (SYS573_IDE_CS0_BASE[CS0_STATUS] & CS0_STATUS_BSY) {
			delayMicroseconds(10);
			continue;
		}

		IDEDevice *dev;
		auto      sig = util::concat2(
			SYS573_IDE_CS0_BASE[CS0_CYLINDER_L],
			SYS573_IDE_CS0_BASE[CS0_CYLINDER_H]
		);

		switch (sig) {
			case _SIG_PARALLEL_ATA:
			case _SIG_SERIAL_ATA:
				dev = new ATADevice(index);
				break;

			case _SIG_PARALLEL_ATAPI:
			case _SIG_SERIAL_ATAPI:
				dev = new ATAPIDevice(index);
				break;

			default:
				LOG_BLKDEV("drive %d: invalid type 0x%04x", index, sig);
				return nullptr;
		}

		auto error = dev->enumerate();

		if (error) {
			LOG_BLKDEV("drive %d: %s", index, getErrorString(error));
			delete dev;
			return nullptr;
		}

		return dev;
	}

	LOG_BLKDEV("drive %d timeout", index);
	return nullptr;
}

}
