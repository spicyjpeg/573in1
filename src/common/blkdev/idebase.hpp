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
#include "common/util/templates.hpp"
#include "ps1/registers573.h"

namespace blkdev {

/* IDE register definitions */

enum IDECS0Register {
	CS0_DATA       = 0,
	CS0_ERROR      = 1,
	CS0_FEATURES   = 1,
	CS0_COUNT      = 2,
	CS0_SECTOR     = 3,
	CS0_CYLINDER_L = 4,
	CS0_CYLINDER_H = 5,
	CS0_DEVICE_SEL = 6,
	CS0_STATUS     = 7,
	CS0_COMMAND    = 7
};

enum IDECS1Register {
	CS1_ALT_STATUS  = 6,
	CS1_DEVICE_CTRL = 6
};

enum IDECS0StatusFlag : uint8_t {
	CS0_STATUS_ERR  = 1 << 0, // Error (ATA)
	CS0_STATUS_CHK  = 1 << 0, // Check condition (ATAPI)
	CS0_STATUS_DRQ  = 1 << 3, // Data request
	CS0_STATUS_DSC  = 1 << 4, // Device seek complete (ATA)
	CS0_STATUS_SERV = 1 << 4, // Service (ATAPI)
	CS0_STATUS_DF   = 1 << 5, // Device fault
	CS0_STATUS_DRDY = 1 << 6, // Device ready
	CS0_STATUS_BSY  = 1 << 7  // Busy
};

enum IDECS0DeviceSelectFlag : uint8_t {
	CS0_DEVICE_SEL_PRIMARY   = 10 << 4,
	CS0_DEVICE_SEL_SECONDARY = 11 << 4,
	CS0_DEVICE_SEL_LBA       =  1 << 6
};

enum IDECS1DeviceControlFlag : uint8_t {
	CS1_DEVICE_CTRL_IEN  = 1 << 1, // Interrupt enable
	CS1_DEVICE_CTRL_SRST = 1 << 2, // Software reset
	CS1_DEVICE_CTRL_HOB  = 1 << 7  // High-order bit (LBA48)
};

enum IDECS0FeaturesFlag : uint8_t {
	CS0_FEATURES_DMA = 1 << 0, // Use DMA for data (ATAPI)
	CS0_FEATURES_OVL = 1 << 1  // Overlap (ATAPI)
};

enum IDECS0CountFlag : uint8_t {
	CS0_COUNT_CD  = 1 << 0, // Command or data (ATAPI)
	CS0_COUNT_IO  = 1 << 1, // Input or output (ATAPI)
	CS0_COUNT_REL = 1 << 2  // Bus release (ATAPI)
};

/* IDE identification block */

enum IDEIdentifyDeviceFlag : uint16_t {
	IDE_IDENTIFY_DEV_PACKET_LENGTH_BITMASK =  3 <<  0,
	IDE_IDENTIFY_DEV_PACKET_LENGTH_12      =  0 <<  0,
	IDE_IDENTIFY_DEV_PACKET_LENGTH_16      =  1 <<  0,
	IDE_IDENTIFY_DEV_DRQ_TYPE_BITMASK      =  3 <<  5,
	IDE_IDENTIFY_DEV_DRQ_TYPE_SLOW         =  0 <<  5,
	IDE_IDENTIFY_DEV_DRQ_TYPE_INTERRUPT    =  1 <<  5,
	IDE_IDENTIFY_DEV_DRQ_TYPE_FAST         =  2 <<  5,
	IDE_IDENTIFY_DEV_REMOVABLE             =  1 <<  7,
	IDE_IDENTIFY_DEV_ATAPI_TYPE_BITMASK    = 31 <<  8,
	IDE_IDENTIFY_DEV_ATAPI_TYPE_CDROM      =  5 <<  8,
	IDE_IDENTIFY_DEV_ATAPI                 =  1 << 15
};

enum IDEIdentifyCapabilitiesFlag : uint16_t {
	IDE_IDENTIFY_CAP_FLAG_DMA            = 1 <<  8,
	IDE_IDENTIFY_CAP_FLAG_LBA            = 1 <<  9,
	IDE_IDENTIFY_CAP_FLAG_IORDY_DISABLE  = 1 << 10,
	IDE_IDENTIFY_CAP_FLAG_IORDY          = 1 << 11,
	IDE_IDENTIFY_CAP_FLAG_ATAPI_OVERLAP  = 1 << 13,
	IDE_IDENTIFY_CAP_FLAG_COMMAND_QUEUE  = 1 << 14,
	IDE_IDENTIFY_CAP_FLAG_DMA_INTERLEAVE = 1 << 15
};

class alignas(uint32_t) IDEIdentifyBlock {
public:
	uint16_t deviceFlags;           // 0
	uint16_t _reserved[9];
	uint16_t serialNumber[10];      // 10-19
	uint16_t _reserved2[3];
	uint16_t revision[4];           // 23-26
	uint16_t model[20];             // 27-46
	uint16_t _reserved3[2];
	uint16_t capabilities;          // 49
	uint16_t _reserved4[3];
	uint16_t timingValidityFlags;   // 53
	uint16_t _reserved5[5];
	uint16_t multiSectorSettings;   // 59
	uint16_t sectorCount[2];        // 60-61
	uint16_t _reserved6;
	uint16_t dmaModeFlags;          // 63
	uint16_t pioModeFlags;          // 64
	uint16_t cycleTimings[4];       // 65-68
	uint16_t _reserved7[2];
	uint16_t atapiBusReleaseTime;   // 71
	uint16_t atapiServiceTime;      // 72
	uint16_t _reserved8[2];
	uint16_t queueDepth;            // 75
	uint16_t _reserved9[4];
	uint16_t versionMajor;          // 80
	uint16_t versionMinor;          // 81
	uint16_t commandSetFlags[7];    // 82-88
	uint16_t secureEraseTimings[2]; // 89-90
	uint16_t currentAPMValue;       // 91
	uint16_t _reserved10;
	uint16_t resetResult;           // 93
	uint16_t currentAAMValue;       // 94
	uint16_t streamSettings[5];     // 95-99
	uint16_t sectorCountExt[4];     // 100-103
	uint16_t _reserved11[23];
	uint16_t removableStatusFlags;  // 127
	uint16_t securityStatus;        // 128
	uint16_t _reserved12[31];
	uint16_t cfPowerMode;           // 160
	uint16_t _reserved13[15];
	uint16_t mediaSerialNumber[30]; // 176-205
	uint16_t _reserved99[49];
	uint16_t checksum;              // 255

	inline uint32_t getSectorCount(void) const {
		return util::concat<uint32_t>(sectorCount[0], sectorCount[1]);
	}
	inline uint64_t getSectorCountExt(void) const {
		return util::concat<uint64_t>(
			sectorCountExt[0],
			sectorCountExt[1],
			sectorCountExt[2],
			sectorCountExt[3]
		);
	}

	bool validateChecksum(void) const;
	int getHighestPIOMode(void) const;
};

/* IDE (ATA/ATAPI) block device class */

class IDEDevice : public Device {
private:
	void _handleError(void);

protected:
	uint8_t _lastStatusReg, _lastErrorReg, _lastCountReg;

	inline IDEDevice(int index)
	: Device(index * IS_SECONDARY) {}

	inline void _set(IDECS0Register reg, uint8_t value) const {
		SYS573_IDE_CS0_BASE[reg] = value;
	}
	inline void _set(IDECS1Register reg, uint8_t value) const {
		SYS573_IDE_CS1_BASE[reg] = value;
	}
	inline uint8_t _get(IDECS0Register reg) const {
		return uint8_t(SYS573_IDE_CS0_BASE[reg] & 0xff);
	}
	inline uint8_t _get(IDECS1Register reg) const {
		return uint8_t(SYS573_IDE_CS1_BASE[reg] & 0xff);
	}

	inline void _select(uint8_t selFlags) const {
		if (flags & IS_SECONDARY)
			_set(CS0_DEVICE_SEL, selFlags | CS0_DEVICE_SEL_SECONDARY);
		else
			_set(CS0_DEVICE_SEL, selFlags | CS0_DEVICE_SEL_PRIMARY);
	}
	inline void _setCylinder(uint16_t value) const {
		_set(CS0_CYLINDER_L, (value >> 0) & 0xff);
		_set(CS0_CYLINDER_H, (value >> 8) & 0xff);
	}
	inline uint16_t _getCylinder(void) const {
		return util::concat<uint16_t>(
			_get(CS0_CYLINDER_L),
			_get(CS0_CYLINDER_H)
		);
	}

	void _readData(void *data, size_t length) const;
	void _writeData(const void *data, size_t length) const;

	DeviceError _setup(const IDEIdentifyBlock &block);
	DeviceError _waitForIdle(
		bool drdy = false, int timeout = 0, bool ignoreError = false
	);
	DeviceError _waitForDRQ(int timeout = 0, bool ignoreError = false);
};

IDEDevice *newIDEDevice(int index);

}
