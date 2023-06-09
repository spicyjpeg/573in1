
#pragma once

#include <stdint.h>
#include "ps1/registers.h"

namespace ide {

/* Register definitions */

enum CS0Register {
	CS0_DATA       = 0,
	CS0_ERROR      = 1,
	CS0_FEATURES   = 1,
	CS0_COUNT      = 2,
	CS0_SECTOR     = 3,
	CS0_CYLINDER_L = 4,
	CS0_CYLINDER_H = 5,
	CS0_DRIVE_SEL  = 6,
	CS0_STATUS     = 7,
	CS0_COMMAND    = 7
};

enum CS1Register {
	CS1_ALT_STATUS  = 6,
	CS1_DEVICE_CTRL = 6
};

enum StatusFlag : uint8_t {
	STATUS_ERR  = 1 << 0, // Error
	STATUS_DRQ  = 1 << 3, // Data request
	STATUS_DSC  = 1 << 4, // Ready (ATA) / service (ATAPI)
	STATUS_DF   = 1 << 5, // Device fault
	STATUS_DRDY = 1 << 6, // Device ready (ATA only)
	STATUS_BSY  = 1 << 7  // Busy
};

/* IDE protocol definitions */

enum IDECommand : uint_least8_t {
	IDE_NOP            = 0x00,
	IDE_READ_PIO       = 0x20,
	IDE_WRITE_PIO      = 0x30,
	IDE_EXECUTE_DIAG   = 0x90,
	IDE_PACKET         = 0xa0, // ATAPI only
	IDE_ATAPI_IDENTIFY = 0xa1, // ATAPI only
	IDE_ERASE          = 0xc0,
	IDE_READ_DMA       = 0xc8,
	IDE_WRITE_DMA      = 0xca,
	IDE_IDENTIFY       = 0xec, // ATA only
	IDE_SET_FEATURES   = 0xef
};

enum IDEFeature : uint8_t {
	FEATURE_8BIT_DATA     = 0x01,
	FEATURE_WRITE_CACHE   = 0x02,
	FEATURE_TRANSFER_MODE = 0x03,
	FEATURE_RELEASE_IRQ   = 0x5d,
	FEATURE_SERVICE_IRQ   = 0x5e
};

/* ATAPI protocol definitions */

enum ATAPICommand : uint8_t {
	ATAPI_TEST_UNIT_READY  = 0x00,
	ATAPI_REQUEST_SENSE    = 0x03,
	ATAPI_INQUIRY          = 0x12,
	ATAPI_START_STOP_UNIT  = 0x1b,
	ATAPI_PREVENT_REMOVAL  = 0x1e,
	ATAPI_READ_CAPACITY    = 0x25,
	ATAPI_READ10           = 0x28,
	ATAPI_SEEK             = 0x2b,
	ATAPI_READ_SUBCHANNEL  = 0x42,
	ATAPI_READ_TOC         = 0x43,
	ATAPI_READ_HEADER      = 0x44,
	ATAPI_PLAY_AUDIO       = 0x45,
	ATAPI_PLAY_AUDIO_MSF   = 0x47,
	ATAPI_PAUSE_RESUME     = 0x4b,
	ATAPI_STOP             = 0x4e,
	ATAPI_MODE_SELECT      = 0x55,
	ATAPI_MODE_SENSE       = 0x5a,
	ATAPI_LOAD_UNLOAD_CD   = 0xa6,
	ATAPI_READ12           = 0xa8,
	ATAPI_READ_CD_MSF      = 0xb9,
	ATAPI_SCAN             = 0xba,
	ATAPI_SET_CD_SPEED     = 0xbb,
	ATAPI_MECHANISM_STATUS = 0xbd,
	ATAPI_READ_CD          = 0xbe
};

enum ATAPISenseKey : uint8_t {
	SENSE_KEY_NO_SENSE        = 0x0,
	SENSE_KEY_RECOVERED_ERROR = 0x1,
	SENSE_KEY_NOT_READY       = 0x2,
	SENSE_KEY_MEDIUM_ERROR    = 0x3,
	SENSE_KEY_HARDWARE_ERROR  = 0x4,
	SENSE_KEY_ILLEGAL_REQUEST = 0x5,
	SENSE_KEY_UNIT_ATTENTION  = 0x6,
	SENSE_KEY_DATA_PROTECT    = 0x7,
	SENSE_KEY_ABORTED_COMMAND = 0xb,
	SENSE_KEY_MISCOMPARE      = 0xe
};

enum ATAPIStartStopMode : uint8_t {
	START_STOP_MODE_STOP_DISC  = 0x0,
	START_STOP_MODE_START_DISC = 0x1,
	START_STOP_MODE_OPEN_TRAY  = 0x2,
	START_STOP_MODE_CLOSE_TRAY = 0x3
};

/* Device class */

enum DeviceFlag {
	DEV_IS_ATAPI         = 1 << 0,
	DEV_LBA_LENGTH_48    = 1 << 1, // Device supports 48-bit LBA addressing
	DEV_PACKET_LENGTH_16 = 1 << 2, // Device requires 16-byte ATAPI packets
	DEV_PRIMARY          = 0 << 4,
	DEV_SECONDARY        = 1 << 4
};

class Device {
private:
	inline uint8_t _readCS0(CS0Register reg) {
		return uint8_t(SYS573_IDE_CS0_BASE[reg] & 0xff);
	}
	inline void _writeCS0(CS0Register reg, uint8_t value) {
		SYS573_IDE_CS0_BASE[reg] = value;
	}
	inline uint8_t _readCS1(CS0Register reg) {
		return uint8_t(SYS573_IDE_CS1_BASE[reg] & 0xff);
	}
	inline void _writeCS1(CS0Register reg, uint8_t value) {
		SYS573_IDE_CS1_BASE[reg] = value;
	}

public:
	uint32_t flags;

	inline Device(uint32_t flags)
	: flags(flags) {}
};

extern Device devices[2];

}
