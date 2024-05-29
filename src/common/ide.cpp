
#include <stddef.h>
#include <stdint.h>
#include "common/ide.hpp"
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

static constexpr int _WAIT_TIMEOUT   = 30000000;
static constexpr int _DETECT_TIMEOUT = 500000;
static constexpr int _DMA_TIMEOUT    = 10000;
static constexpr int _SRST_DELAY     = 5000;

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
	nullptr,
	nullptr,
	"ABORTED_COMMAND",
	nullptr,
	nullptr,
	"MISCOMPARE",
	nullptr
};

const char *const DEVICE_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"NO_DRIVE",
	"STATUS_TIMEOUT",
	"DRIVE_ERROR",
	"INCOMPLETE_DATA",
	"CHECKSUM_MISMATCH"
};

/* Utilities */

#ifdef ENABLE_FULL_IDE_DRIVER
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
#endif

bool IdentifyBlock::validateChecksum(void) const {
	if ((checksum & 0xff) != 0xa5)
		return true;

	// FIXME: is this right?
	uint8_t value = (-int(util::sum(
		reinterpret_cast<const uint8_t *>(&deviceFlags), ATA_SECTOR_SIZE - 1
	))) & 0xff;

	if (value != (checksum >> 8)) {
		LOG("mismatch, exp=0x%02x, got=0x%02x", value, checksum >> 8);
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

Device devices[2]{ (DEVICE_PRIMARY), (DEVICE_SECONDARY) };

void Device::_setLBA(uint64_t lba, size_t count) {
	if (flags & DEVICE_HAS_LBA48) {
		//assert(lba < (1ULL << 48));
		//assert(count <= (1 << 16));

		_select(CS0_DEVICE_SEL_LBA);

		_write(CS0_COUNT,      (count >>  8) & 0xff);
		_write(CS0_SECTOR,     (lba   >> 24) & 0xff);
		_write(CS0_CYLINDER_L, (lba   >> 32) & 0xff);
		_write(CS0_CYLINDER_H, (lba   >> 40) & 0xff);
	} else {
		//assert(lba < (1ULL << 28));
		//assert(count <= (1 << 8));

		_select(CS0_DEVICE_SEL_LBA | ((lba >> 24) & 15));
	}

	_write(CS0_COUNT,      (count >>  0) & 0xff);
	_write(CS0_SECTOR,     (lba   >>  0) & 0xff);
	_write(CS0_CYLINDER_L, (lba   >>  8) & 0xff);
	_write(CS0_CYLINDER_H, (lba   >> 16) & 0xff);
}

DeviceError Device::_waitForStatus(
	uint8_t mask, uint8_t value, int timeout, bool ignoreErrors
) {
	if (!timeout)
		timeout = _WAIT_TIMEOUT;

	for (; timeout > 0; timeout -= 10) {
		uint8_t status = _read(CS0_STATUS);

		if (!ignoreErrors && (status & CS0_STATUS_ERR)) {
			LOG(
				"IDE error, stat=0x%02x, err=0x%02x", _read(CS0_STATUS),
				_read(CS0_ERROR)
			);

			_write(CS0_COMMAND, ATA_DEVICE_RESET);
			return DRIVE_ERROR;
		}

		if ((status & mask) == value)
			return NO_ERROR;

		delayMicroseconds(10);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG(
		"IDE timeout, stat=0x%02x, err=0x%02x", _read(CS0_STATUS),
		_read(CS0_ERROR)
	);

	_write(CS0_COMMAND, ATA_DEVICE_RESET);
	return STATUS_TIMEOUT;
}

DeviceError Device::_command(
	uint8_t cmd, uint8_t status, int timeout, bool ignoreErrors
) {
	auto error = _waitForStatus(
		CS0_STATUS_BSY | status, status, timeout, ignoreErrors
	);

	if (error)
		return error;

	_write(CS0_COMMAND, cmd);
	return _waitForStatus(CS0_STATUS_BSY, 0, timeout);
}

DeviceError Device::_detectDrive(void) {
	// Issue a software reset, which affects both devices on the bus.
	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN | CS1_DEVICE_CTRL_SRST);
	delayMicroseconds(_SRST_DELAY);
	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN);
	delayMicroseconds(_SRST_DELAY);

	_select();
#ifndef ENABLE_FULL_IDE_DRIVER
	io::clearWatchdog();
#endif

	// Issue dummy writes to the sector count register and attempt to read back
	// the written value. This should not fail even if the drive is busy.
	uint8_t pattern = 0x55;

	for (int timeout = _DETECT_TIMEOUT; timeout > 0; timeout -= 10) {
		_write(CS0_COUNT, pattern);

		// Note that ATA drives will also assert DRDY when ready, but ATAPI
		// drives will not.
		if (_read(CS0_COUNT) == pattern)
			return _waitForStatus(CS0_STATUS_BSY, 0);

		pattern >>= 1;
		if (!(pattern & 1))
			pattern |= 1 << 7;

		delayMicroseconds(10);
#ifndef ENABLE_FULL_IDE_DRIVER
		io::clearWatchdog();
#endif
	}

	LOG("drive %d not found", (flags / DEVICE_SECONDARY) & 1);
	return NO_DRIVE;
}

DeviceError Device::_readPIO(void *data, size_t length, int timeout) {
	util::assertAligned<uint16_t>(data);

	auto error = _waitForStatus(CS0_STATUS_DRQ, CS0_STATUS_DRQ, timeout);

	if (error)
		return error;

	auto ptr = reinterpret_cast<uint16_t *>(data);

	for (; length; length -= 2)
		*(ptr++) = SYS573_IDE_CS0_BASE[CS0_DATA];

	return NO_ERROR;
}

DeviceError Device::_writePIO(const void *data, size_t length, int timeout) {
	util::assertAligned<uint16_t>(data);

	auto error = _waitForStatus(CS0_STATUS_DRQ, CS0_STATUS_DRQ, timeout);

	if (error)
		return error;

	auto ptr = reinterpret_cast<const uint16_t *>(data);

	for (; length; length -= 2)
		SYS573_IDE_CS0_BASE[CS0_DATA] = *(ptr++);

	return NO_ERROR;
}

DeviceError Device::_readDMA(void *data, size_t length, int timeout) {
	length /= 4;

	util::assertAligned<uint32_t>(data);

	auto error = _waitForStatus(CS0_STATUS_DRQ, CS0_STATUS_DRQ, timeout);

	if (error)
		return error;

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT)) {
		LOG("DMA transfer timeout");
		return INCOMPLETE_DATA;
	}

	return NO_ERROR;
}

DeviceError Device::_writeDMA(const void *data, size_t length, int timeout) {
	length /= 4;

	util::assertAligned<uint32_t>(data);

	auto error = _waitForStatus(CS0_STATUS_DRQ, CS0_STATUS_DRQ, timeout);

	if (error)
		return error;

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT)) {
		LOG("DMA transfer timeout");
		return INCOMPLETE_DATA;
	}

	return NO_ERROR;
}

DeviceError Device::_ideReadWrite(
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

		_setLBA(lba, chunkLength);
		auto error = _command(cmd, CS0_STATUS_DRDY);

		if (error)
			return error;

		// Data must be transferred one sector at a time as the drive may
		// deassert DRQ between sectors.
		for (size_t i = chunkLength; i; i--) {
			if (write)
				error = _writePIO(
					reinterpret_cast<const void *>(ptr), ATA_SECTOR_SIZE
				);
			else
				error = _readPIO(
					reinterpret_cast<void *>(ptr), ATA_SECTOR_SIZE
				);

			if (error)
				return error;

			ptr += ATA_SECTOR_SIZE;
		}

		error = _waitForStatus(
			CS0_STATUS_BSY | CS0_STATUS_DRDY, CS0_STATUS_DRDY
		);

		if (error)
			return error;

		lba   += chunkLength;
		count -= chunkLength;
	}

	return NO_ERROR;
}

DeviceError Device::_atapiRead(uintptr_t ptr, uint32_t lba, size_t count) {
	Packet packet;

	packet.setRead(lba, count);
	auto error = atapiPacket(packet);

	if (error)
		return error;

	// Data must be transferred one sector at a time as the drive may deassert
	// DRQ between sectors.
	for (; count; count--) {
		error = _readPIO(reinterpret_cast<void *>(ptr), ATAPI_SECTOR_SIZE);

		if (error)
			return error;

		ptr += ATAPI_SECTOR_SIZE;
	}

	return _waitForStatus(CS0_STATUS_BSY, 0);
}

DeviceError Device::enumerate(void) {
	flags &= DEVICE_PRIMARY | DEVICE_SECONDARY;

	auto error = _detectDrive();

	if (error)
		return error;

	// Check whether the ATAPI signature is present and fetch the appropriate
	// identification block.
	// NOTE: the primary drive may respond to all secondary drive register
	// accesses, with the exception of command writes, if no secondary drive is
	// actually present. A strict timeout is used in the commands below in order
	// to prevent blocking for too long.
	IdentifyBlock block;

	if ((_read(CS0_CYLINDER_L) == 0x14) && (_read(CS0_CYLINDER_H) == 0xeb)) {
		flags |= DEVICE_ATAPI;

		if (_command(ATA_IDENTIFY_PACKET, 0, _DETECT_TIMEOUT))
			return NO_DRIVE;
		if (_readPIO(&block, sizeof(IdentifyBlock), _DETECT_TIMEOUT))
			return NO_DRIVE;

		if (!block.validateChecksum())
			return CHECKSUM_MISMATCH;
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
		if (_command(ATA_IDENTIFY, CS0_STATUS_DRDY, _DETECT_TIMEOUT))
			return NO_DRIVE;
		if (_readPIO(&block, sizeof(IdentifyBlock), _DETECT_TIMEOUT))
			return NO_DRIVE;

		if (!block.validateChecksum())
			return CHECKSUM_MISMATCH;
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
	_copyString(model, block.model, sizeof(block.model));
	_copyString(revision, block.revision, sizeof(block.revision));
	_copyString(serialNumber, block.serialNumber, sizeof(block.serialNumber));

	LOG("drive %d: %s", (flags / DEVICE_SECONDARY) & 1, model);
#endif

	// Find out the fastest PIO transfer mode supported and enable it.
	int mode = block.getHighestPIOMode();

	_write(CS0_FEATURES, FEATURE_TRANSFER_MODE);
	_write(CS0_COUNT,    (1 << 3) | mode);

	error = _command(ATA_SET_FEATURES, 0);

	if (error)
		return error;

	LOG("done, stat=0x%02x, mode=PIO%d", _read(CS0_STATUS), mode);
	flags |= DEVICE_READY;
	return NO_ERROR;
}

DeviceError Device::atapiPacket(const Packet &packet, size_t transferLength) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (!(flags & DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	_select();

	_write(CS0_CYLINDER_L, (transferLength >> 0) & 0xff);
	_write(CS0_CYLINDER_H, (transferLength >> 8) & 0xff);

	auto error = _command(ATA_PACKET, 0);

	if (!error)
		error = _writePIO(&packet, (flags & DEVICE_HAS_PACKET16) ? 16 : 12);
	if (!error)
		return _waitForStatus(CS0_STATUS_BSY, 0);

	return atapiPoll();
}

DeviceError Device::atapiPoll(void) {
	Packet    packet;
	SenseData data;

	packet.setRequestSense();

	// If an error occurs, the error flag in the status register will be set but
	// the drive will still accept a request sense command.
	auto error = _command(ATA_PACKET, 0, 0, true);

	if (!error)
		error = _writePIO(&packet, (flags & DEVICE_HAS_PACKET16) ? 16 : 12);
	if (!error)
		error = _waitForStatus(CS0_STATUS_BSY, 0);
	if (!error)
		error = _readPIO(&data, sizeof(data));

	int senseKey;

	if (error) {
		// If the request sense command fails, fall back to reading the sense
		// key from the IDE error register.
		senseKey = (_read(CS0_ERROR) >> 4) & 15;
		LOG("request sense failed");
	} else {
		senseKey = data.senseKey & 15;
		LOG(
			"key=0x%02x, asc=0x%02x, ascq=0x%02x", data.senseKey, data.asc,
			data.ascQualifier
		);
	}

	LOG("%s (%d)", _SENSE_KEY_NAMES[senseKey], senseKey);

	switch (senseKey) {
		case SENSE_KEY_NO_SENSE:
			return NO_ERROR;

		case SENSE_KEY_NOT_READY:
		case SENSE_KEY_MEDIUM_ERROR:
		case SENSE_KEY_DATA_PROTECT:
			return DISC_ERROR;

		case SENSE_KEY_UNIT_ATTENTION:
			return DISC_CHANGED;

		default:
			return DRIVE_ERROR;
	}
}

DeviceError Device::read(void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!(flags & DEVICE_READY))
		return NO_DRIVE;

	if (flags & DEVICE_ATAPI) {
		auto error = _atapiRead(
			reinterpret_cast<uintptr_t>(data), static_cast<uint32_t>(lba), count
		);

#ifdef ENABLE_FULL_IDE_DRIVER
		if (error)
			error = atapiPoll();
#endif

		return error;
	} else {
		return _ideReadWrite(
			reinterpret_cast<uintptr_t>(data), lba, count, false
		);
	}
}

DeviceError Device::write(const void *data, uint64_t lba, size_t count) {
	util::assertAligned<uint32_t>(data);

	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (flags & (DEVICE_READ_ONLY | DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	return _ideReadWrite(reinterpret_cast<uintptr_t>(data), lba, count, true);
}

DeviceError Device::goIdle(bool standby) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;

	if (flags & DEVICE_ATAPI) {
		Packet packet;

		packet.setStartStopUnit(START_STOP_MODE_STOP_DISC);
		return atapiPacket(packet);
	} else {
		_select();

		auto error = _command(
			standby ? ATA_STANDBY_IMMEDIATE : ATA_IDLE_IMMEDIATE,
			CS0_STATUS_DRDY
		);

#if 0
		if (error) {
			// If the immediate command failed, fall back to setting the
			// inactivity timeout to the lowest allowed value (5 seconds).
			// FIXME: the original timeout would have to be restored once the
			// drive is accessed again
			_write(CS0_COUNT, 1);
			error = _command(standby ? ATA_STANDBY : ATA_IDLE, CS0_STATUS_DRDY);
		}
#endif

		return error;
	}
}

DeviceError Device::flushCache(void) {
	if (!(flags & DEVICE_READY))
		return NO_DRIVE;
	if (!(flags & DEVICE_HAS_FLUSH))
		return NO_ERROR;
		//return UNSUPPORTED_OP;

	_select();

	return _command(
		(flags & DEVICE_HAS_LBA48) ? ATA_FLUSH_CACHE_EXT : ATA_FLUSH_CACHE,
		CS0_STATUS_DRDY
	);
}

}
