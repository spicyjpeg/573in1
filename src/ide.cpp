
#include <stddef.h>
#include <stdint.h>
#include "ide.hpp"
#include "vendor/diskio.h"

namespace ide {

/* Device class */

Device devices[2]{
	(DEV_PRIMARY), (DEV_SECONDARY)
};

/* FatFs API glue */

extern "C" DSTATUS disk_initialize(uint8_t drive) {
	return RES_NOTRDY;
}

extern "C" DSTATUS disk_status(uint8_t drive) {
	return RES_NOTRDY;
}

extern "C" DRESULT disk_read(uint8_t drive, uint8_t *data, uint64_t lba, size_t length) {
	return RES_NOTRDY;
}

extern "C" DRESULT disk_write(uint8_t drive, const uint8_t *data, uint64_t lba, size_t length) {
	return RES_NOTRDY;
}

extern "C" DRESULT disk_ioctl(uint8_t drive, uint8_t cmd, void *buff) {
	switch (cmd) {
		case CTRL_SYNC:
			return RES_NOTRDY;

		case GET_SECTOR_COUNT:
			return RES_NOTRDY;

		case GET_SECTOR_SIZE:
			return RES_NOTRDY;

		case GET_BLOCK_SIZE:
			return RES_NOTRDY;

		case CTRL_TRIM:
			return RES_NOTRDY;

		default:
			return RES_ERROR;
	}
}

extern "C" uint32_t get_fattime(void) {
	return 0;
}

}
