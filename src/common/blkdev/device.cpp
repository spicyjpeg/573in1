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

#include <stdint.h>
#include "common/blkdev/device.hpp"
#include "common/util/templates.hpp"

namespace blkdev {

/* CD-ROM definitions */

void MSF::fromLBA(uint32_t lba) {
	lba += CDROM_TOC_PREGAP;

	minute = lba / 4500;
	second = (lba / 75) % 60;
	frame  = lba % 75;
}

uint32_t MSF::toLBA(void) const {
	return -CDROM_TOC_PREGAP
		+ minute * 4500
		+ second * 75
		+ frame;
}

void BCDMSF::fromLBA(uint32_t lba) {
	lba += CDROM_TOC_PREGAP;

	minute = util::encodeBCD(lba / 4500);
	second = util::encodeBCD((lba / 75) % 60);
	frame  = util::encodeBCD(lba % 75);
}

uint32_t BCDMSF::toLBA(void) const {
	return -CDROM_TOC_PREGAP
		+ util::decodeBCD(minute) * 4500
		+ util::decodeBCD(second) * 75
		+ util::decodeBCD(frame);
}

/* Base block device class */

// This is a fallback implementation of readStream() used if the device class
// does not provide a more efficient one.
DeviceError Device::readStream(
	StreamCallback callback,
	uint64_t       lba,
	size_t         count,
	void           *arg
) {
	uint8_t sector[MAX_SECTOR_LENGTH];

	for (; count > 0; count--) {
		auto error = read(sector, lba++, 1);

		if (error)
			return error;

		callback(sector, sectorLength, arg);
	}

	return NO_ERROR;
}

/* Utilities */

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

}
