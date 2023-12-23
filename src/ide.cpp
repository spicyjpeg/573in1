
#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"
#include "ps1/system.h"
#include "ide.hpp"
#include "util.hpp"

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

static constexpr int _STATUS_TIMEOUT       = 1000000;
static constexpr int _RESET_STATUS_TIMEOUT = 2000000;
static constexpr int _DATA_STATUS_TIMEOUT  = 2000000;
static constexpr int _DMA_TIMEOUT          = 10000;

/* Utilities */

static void _copyString(char *output, const uint16_t *input, size_t length) {
	// The strings in the identification block are byte-swapped and padded with
	// spaces. To make them printable, any span of consecutive space characters
	// at the end is replaced with null bytes.
	bool isPadding = true;

	output += length;
	input  += length / 2;

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

void Device::_setLBA(uint64_t lba, uint16_t count) {
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

DeviceError Device::_waitForStatus(uint8_t mask, uint8_t value, int timeout) {
	for (; timeout > 0; timeout--) {
		uint8_t status = _read(CS0_STATUS);

		if (status & CS0_STATUS_ERR) {
			LOG("IDE error, stat=0x%02x, err=0x%02x", _read(CS0_STATUS), _read(CS0_ERROR));
			return DRIVE_ERROR;
		}
		if ((status & mask) == value)
			return NO_ERROR;

		delayMicroseconds(1);
	}

	LOG("IDE timeout, stat=0x%02x, err=0x%02x", _read(CS0_STATUS), _read(CS0_ERROR));
	return STATUS_TIMEOUT;
}

DeviceError Device::_command(uint8_t cmd, bool drdy) {
	DeviceError error;
	uint8_t     mask = drdy ? CS0_STATUS_DRDY : 0;

	error = _waitForStatus(CS0_STATUS_BSY | mask, mask, _STATUS_TIMEOUT);
	if (error)
		return error;

	_write(CS0_COMMAND, cmd);
	return _waitForStatus(CS0_STATUS_BSY, 0, _STATUS_TIMEOUT);
}

DeviceError Device::_transferPIO(void *data, size_t length, bool write) {
	util::assertAligned<uint16_t>(data);

	auto error = _waitForStatus(
		CS0_STATUS_DRQ, CS0_STATUS_DRQ, _DATA_STATUS_TIMEOUT
	);
	if (error)
		return error;

	auto ptr = reinterpret_cast<uint16_t *>(data);

	if (write) {
		for (; length; length -= 2)
			SYS573_IDE_CS0_BASE[CS0_DATA] = *(ptr++);
	} else {
		for (; length; length -= 2)
			*(ptr++) = SYS573_IDE_CS0_BASE[CS0_DATA];
	}

	return NO_ERROR;
}

DeviceError Device::_transferDMA(void *data, size_t length, bool write) {
	length /= 4;

	util::assertAligned<uint32_t>(data);

	auto error = _waitForStatus(
		CS0_STATUS_DRQ, CS0_STATUS_DRQ, _DATA_STATUS_TIMEOUT
	);
	if (error)
		return error;

	uint32_t flags = DMA_CHCR_MODE_BURST | DMA_CHCR_ENABLE | DMA_CHCR_TRIGGER;
	flags         |= write ? DMA_CHCR_WRITE : DMA_CHCR_READ;

	// TODO: is this actually needed?
	BIU_DEV0_ADDR = reinterpret_cast<uint32_t>(SYS573_IDE_CS0_BASE) & 0x1fffffff;

	DMA_MADR(DMA_PIO) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = flags;

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT)) {
		LOG("DMA transfer timeout");
		return INCOMPLETE_DATA;
	}

	BIU_DEV0_ADDR = DEV0_BASE & 0x1fffffff;
	return NO_ERROR;
}

DeviceError Device::enumerate(void) {
	flags &= DEVICE_PRIMARY | DEVICE_SECONDARY;

	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN | CS1_DEVICE_CTRL_SRST);
	delayMicroseconds(5000);
	_write(CS1_DEVICE_CTRL, CS1_DEVICE_CTRL_IEN);
	delayMicroseconds(5000);

	auto error = _waitForStatus(CS0_STATUS_BSY, 0, _RESET_STATUS_TIMEOUT);
	if (error)
		return error;

	// Check whether the ATAPI signature is present. Note that ATAPI drives will
	// not assert DRDY until the first command is issued.
	// FIXME: some drives may not provide the signature immediately
	IdentifyBlock block;

	_select(0);

	if ((_read(CS0_CYLINDER_L) == 0x14) && (_read(CS0_CYLINDER_H) == 0xeb)) {
		flags |= DEVICE_ATAPI;

		error = _command(ATA_IDENTIFY_PACKET, false);
		if (error)
			return error;

		error = _transferPIO(&block, sizeof(IdentifyBlock));
		if (error)
			return error;

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
		error = _command(ATA_IDENTIFY);
		if (error)
			return error;

		error = _transferPIO(&block, sizeof(IdentifyBlock));
		if (error)
			return error;

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

	_copyString(model, block.model, sizeof(model));
	_copyString(revision, block.revision, sizeof(revision));
	_copyString(serialNumber, block.serialNumber, sizeof(serialNumber));

	LOG("%s=%s", (flags & DEVICE_SECONDARY) ? "sec" : "pri", model);

	// Find out the fastest PIO transfer mode supported and enable it.
	int mode = block.getHighestPIOMode();

	_write(CS0_FEATURES, FEATURE_TRANSFER_MODE);
	_write(CS0_COUNT,    (1 << 3) | mode);
	error = _command(ATA_SET_FEATURES, false);
	if (error)
		return error;

	LOG("done, stat=0x%02x, mode=PIO%d", _read(CS0_STATUS), mode);
	flags |= DEVICE_READY;
	return NO_ERROR;
}

DeviceError Device::_ideReadWrite(
	uintptr_t ptr, uint64_t lba, size_t count, bool write
) {
	if (flags & DEVICE_ATAPI)
		return UNSUPPORTED_OP;

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
		size_t length = util::min(count, maxLength);

		_setLBA(lba, count);
		auto error = _command(cmd);
		if (error)
			return error;

		// Data must be transferred one sector at a time as the drive may
		// deassert DRQ between sectors.
		for (size_t i = length; i; i--) {
			error = _transferPIO(
				reinterpret_cast<void *>(ptr), ATA_SECTOR_SIZE, write
			);
			if (error)
				return error;

			ptr += ATA_SECTOR_SIZE;
		}

		error = _waitForStatus(
			CS0_STATUS_BSY | CS0_STATUS_DRDY, CS0_STATUS_DRDY, _STATUS_TIMEOUT
		);
		if (error)
			return error;

		count -= length;
	}

	return NO_ERROR;
}

DeviceError Device::ideFlushCache(void) {
	if (!(flags & DEVICE_HAS_FLUSH))
		return NO_ERROR;
		//return UNSUPPORTED_OP;

	_select(CS0_DEVICE_SEL_LBA);
	return _command(
		(flags & DEVICE_HAS_LBA48) ? ATA_FLUSH_CACHE_EXT : ATA_FLUSH_CACHE
	);
}

DeviceError Device::atapiPacket(Packet &packet, size_t transferLength) {
	if (!(flags & DEVICE_ATAPI))
		return UNSUPPORTED_OP;

	_select(0);

	_write(CS0_CYLINDER_L, (transferLength >> 0) & 0xff);
	_write(CS0_CYLINDER_H, (transferLength >> 8) & 0xff);
	auto error = _command(ATA_PACKET, false);
	if (error)
		return error;

	error = _transferPIO(&packet, (flags & DEVICE_HAS_PACKET16) ? 16 : 12);
	if (error)
		return error;

	return _waitForStatus(CS0_STATUS_BSY, 0, _STATUS_TIMEOUT);
}

}
