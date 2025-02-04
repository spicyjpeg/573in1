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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/blkdev/device.hpp"
#include "common/blkdev/idebase.hpp"

namespace blkdev {

/* ATA command definitions */

enum ATACommand : uint8_t {
	ATA_NOP                  = 0x00, // ATAPI
	ATA_DEVICE_RESET         = 0x08, // ATAPI
	ATA_READ_SECTORS         = 0x20, // ATA
	ATA_READ_SECTORS_EXT     = 0x24, // ATA
	ATA_READ_DMA_EXT         = 0x25, // ATA
	ATA_READ_DMA_QUEUED_EXT  = 0x26, // ATA
	ATA_WRITE_SECTORS        = 0x30, // ATA
	ATA_WRITE_SECTORS_EXT    = 0x34, // ATA
	ATA_WRITE_DMA_EXT        = 0x35, // ATA
	ATA_WRITE_DMA_QUEUED_EXT = 0x36, // ATA
	ATA_SEEK                 = 0x70, // ATA
	ATA_EXECUTE_DIAGNOSTIC   = 0x90, // ATA/ATAPI
	ATA_PACKET               = 0xa0, // ATAPI
	ATA_IDENTIFY_PACKET      = 0xa1, // ATAPI
	ATA_SERVICE              = 0xa2, // ATA/ATAPI
	ATA_DEVICE_CONFIG        = 0xb1, // ATA
	ATA_ERASE_SECTORS        = 0xc0, // ATA
	ATA_READ_DMA_QUEUED      = 0xc7, // ATA
	ATA_READ_DMA             = 0xc8, // ATA
	ATA_WRITE_DMA            = 0xca, // ATA
	ATA_WRITE_DMA_QUEUED     = 0xcc, // ATA
	ATA_STANDBY_IMMEDIATE    = 0xe0, // ATA/ATAPI
	ATA_IDLE_IMMEDIATE       = 0xe1, // ATA/ATAPI
	ATA_STANDBY              = 0xe2, // ATA
	ATA_IDLE                 = 0xe3, // ATA
	ATA_CHECK_POWER_MODE     = 0xe5, // ATA/ATAPI
	ATA_SLEEP                = 0xe6, // ATA/ATAPI
	ATA_FLUSH_CACHE          = 0xe7, // ATA
	ATA_FLUSH_CACHE_EXT      = 0xea, // ATA
	ATA_IDENTIFY             = 0xec, // ATA
	ATA_SET_FEATURES         = 0xef  // ATA/ATAPI
};

enum ATAFeature : uint8_t {
	ATA_FEATURE_8BIT_DATA     = 0x01,
	ATA_FEATURE_WRITE_CACHE   = 0x02,
	ATA_FEATURE_TRANSFER_MODE = 0x03,
	ATA_FEATURE_APM           = 0x05,
	ATA_FEATURE_AAM           = 0x42,
	ATA_FEATURE_RELEASE_IRQ   = 0x5d,
	ATA_FEATURE_SERVICE_IRQ   = 0x5e,
	ATA_FEATURE_DISABLE       = 0x80
};

enum ATATransferModeFlag : uint8_t {
	ATA_TRANSFER_MODE_PIO_DEFAULT = 0 << 3,
	ATA_TRANSFER_MODE_PIO         = 1 << 3,
	ATA_TRANSFER_MODE_DMA         = 1 << 5,
	ATA_TRANSFER_MODE_UDMA        = 1 << 6
};

/* ATA block device class */

class ATADevice : public IDEDevice {
private:
	DeviceError _setLBA(uint64_t lba, size_t count, int timeout = 0);
	DeviceError _transfer(
		uintptr_t ptr, uint64_t lba, size_t count, bool write
	);

public:
	inline ATADevice(int index)
	: IDEDevice(index) {}

	DeviceError enumerate(void);
	DeviceError poll(void);
	void handleInterrupt(void);

	DeviceError read(void *data, uint64_t lba, size_t count);
	DeviceError write(const void *data, uint64_t lba, size_t count);
	DeviceError trim(uint64_t lba, size_t count);
	DeviceError flushCache(void);

	DeviceError goIdle(bool standby = false);
};

}
