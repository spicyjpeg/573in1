
#include <stddef.h>
#include <stdint.h>
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/util.hpp"
#include "vendor/diskio.h"

/* FatFs library API glue */

extern "C" DSTATUS disk_initialize(uint8_t drive) {
#if 0
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY)) {
		if (dev.enumerate())
			return RES_NOTRDY;
	}
#endif

	return disk_status(drive);
}

extern "C" DSTATUS disk_status(uint8_t drive) {
	auto     &dev  = ide::devices[drive];
	uint32_t flags = 0;

	if (!(dev.flags & ide::DEVICE_READY))
		flags |= STA_NOINIT;
	if (!dev.capacity)
		flags |= STA_NODISK;
	if (dev.flags & ide::DEVICE_READ_ONLY)
		flags |= STA_PROTECT;

	return flags;
}

extern "C" DRESULT disk_read(
	uint8_t drive, uint8_t *data, LBA_t lba, size_t count
) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;
	if (dev.ideRead(data, lba, count))
		return RES_ERROR;

	return RES_OK;
}

extern "C" DRESULT disk_write(
	uint8_t drive, const uint8_t *data, LBA_t lba, size_t count
) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;
	if (dev.flags & ide::DEVICE_READ_ONLY)
		return RES_WRPRT;
	if (dev.ideWrite(data, lba, count))
		return RES_ERROR;

	return RES_OK;
}

extern "C" DRESULT disk_ioctl(uint8_t drive, uint8_t cmd, void *data) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;

	switch (cmd) {
		case CTRL_SYNC:
			return dev.ideFlushCache() ? RES_ERROR : RES_OK;

		case GET_SECTOR_COUNT:
			__builtin_memcpy(data, &dev.capacity, sizeof(LBA_t));
			return RES_OK;

		case GET_SECTOR_SIZE:
		//case GET_BLOCK_SIZE:
			*reinterpret_cast<uint16_t *>(data) = dev.getSectorSize();
			return RES_OK;

		default:
			return RES_PARERR;
	}
}

extern "C" uint32_t get_fattime(void) {
	util::Date date;

	io::getRTCTime(date);
	return date.toDOSTime();
}
