
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"
#include "util.hpp"

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
	CS0_DEVICE_SEL = 6,
	CS0_STATUS     = 7,
	CS0_COMMAND    = 7
};

enum CS1Register {
	CS1_ALT_STATUS  = 6,
	CS1_DEVICE_CTRL = 6
};

enum CS0StatusFlag : uint8_t {
	CS0_STATUS_ERR  = 1 << 0, // Error
	CS0_STATUS_DRQ  = 1 << 3, // Data request
	CS0_STATUS_DSC  = 1 << 4, // Device seek complete (ATA)
	CS0_STATUS_SERV = 1 << 4, // Service (ATAPI)
	CS0_STATUS_DF   = 1 << 5, // Device fault
	CS0_STATUS_DRDY = 1 << 6, // Device ready
	CS0_STATUS_BSY  = 1 << 7  // Busy
};

enum CS0DeviceSelectFlag : uint8_t {
	CS0_DEVICE_SEL_PRIMARY   = 10 << 4,
	CS0_DEVICE_SEL_SECONDARY = 11 << 4,
	CS0_DEVICE_SEL_LBA       =  1 << 6
};

enum CS1DeviceControlFlag : uint8_t {
	CS1_DEVICE_CTRL_IEN  = 1 << 1, // Interrupt enable
	CS1_DEVICE_CTRL_SRST = 1 << 2, // Software reset
	CS1_DEVICE_CTRL_HOB  = 1 << 7  // High-order bit (LBA48)
};

/* ATA protocol definitions */

static constexpr size_t ATA_SECTOR_SIZE = 512;

enum ATACommand : uint8_t {
	ATA_NOP                  = 0x00,
	ATA_DEVICE_RESET         = 0x08,
	ATA_READ_SECTORS         = 0x20,
	ATA_READ_SECTORS_EXT     = 0x24,
	ATA_READ_DMA_EXT         = 0x25,
	ATA_READ_DMA_QUEUED_EXT  = 0x26,
	ATA_WRITE_SECTORS        = 0x30,
	ATA_WRITE_SECTORS_EXT    = 0x34,
	ATA_WRITE_DMA_EXT        = 0x35,
	ATA_WRITE_DMA_QUEUED_EXT = 0x36,
	ATA_SEEK                 = 0x70,
	ATA_EXECUTE_DIAGNOSTIC   = 0x90,
	ATA_PACKET               = 0xa0,
	ATA_IDENTIFY_PACKET      = 0xa1,
	ATA_SERVICE              = 0xa2,
	ATA_DEVICE_CONFIG        = 0xb1,
	ATA_ERASE_SECTORS        = 0xc0,
	ATA_READ_DMA_QUEUED      = 0xc7,
	ATA_READ_DMA             = 0xc8,
	ATA_WRITE_DMA            = 0xca,
	ATA_WRITE_DMA_QUEUED     = 0xcc,
	ATA_STANDBY_IMMEDIATE    = 0xe0,
	ATA_IDLE_IMMEDIATE       = 0xe1,
	ATA_STANDBY              = 0xe2,
	ATA_IDLE                 = 0xe3,
	ATA_SLEEP                = 0xe6,
	ATA_FLUSH_CACHE          = 0xe7,
	ATA_FLUSH_CACHE_EXT      = 0xea,
	ATA_IDENTIFY             = 0xec,
	ATA_SET_FEATURES         = 0xef
};

enum ATAFeature : uint8_t {
	FEATURE_8BIT_DATA     = 0x01,
	FEATURE_WRITE_CACHE   = 0x02,
	FEATURE_TRANSFER_MODE = 0x03,
	FEATURE_APM           = 0x05,
	FEATURE_AAM           = 0x42,
	FEATURE_RELEASE_IRQ   = 0x5d,
	FEATURE_SERVICE_IRQ   = 0x5e,
	FEATURE_DISABLE       = 0x80
};

/* ATAPI protocol definitions */

static constexpr size_t ATAPI_SECTOR_SIZE = 2048;

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

/* Identification blocks */

enum IdentifyDeviceFlag : uint16_t {
	IDENTIFY_DEV_PACKET_LENGTH_BITMASK =  3 <<  0,
	IDENTIFY_DEV_PACKET_LENGTH_12      =  0 <<  0,
	IDENTIFY_DEV_PACKET_LENGTH_16      =  1 <<  0,
	IDENTIFY_DEV_DRQ_TYPE_BITMASK      =  3 <<  5,
	IDENTIFY_DEV_DRQ_TYPE_SLOW         =  0 <<  5,
	IDENTIFY_DEV_DRQ_TYPE_INTERRUPT    =  1 <<  5,
	IDENTIFY_DEV_DRQ_TYPE_FAST         =  2 <<  5,
	IDENTIFY_DEV_REMOVABLE             =  1 <<  7,
	IDENTIFY_DEV_ATAPI_TYPE_BITMASK    = 31 <<  8,
	IDENTIFY_DEV_ATAPI_TYPE_CDROM      =  5 <<  8,
	IDENTIFY_DEV_ATAPI                 =  1 << 15
};

enum IdentifyCapabilitiesFlag : uint16_t {
	IDENTIFY_CAP_FLAG_DMA            = 1 <<  8,
	IDENTIFY_CAP_FLAG_LBA            = 1 <<  9,
	IDENTIFY_CAP_FLAG_IORDY_DISABLE  = 1 << 10,
	IDENTIFY_CAP_FLAG_IORDY          = 1 << 11,
	IDENTIFY_CAP_FLAG_ATAPI_OVERLAP  = 1 << 13,
	IDENTIFY_CAP_FLAG_COMMAND_QUEUE  = 1 << 14,
	IDENTIFY_CAP_FLAG_DMA_INTERLEAVE = 1 << 15
};

class IdentifyBlock {
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
		return sectorCount[0] | (sectorCount[1] << 16);
	}
	inline uint64_t getSectorCountExt(void) const {
		return 0
			| (uint64_t(sectorCountExt[0]) <<  0)
			| (uint64_t(sectorCountExt[1]) << 16)
			| (uint64_t(sectorCountExt[2]) << 32)
			| (uint64_t(sectorCountExt[3]) << 48);
	}

	bool validateChecksum(void) const;
	int getHighestPIOMode(void) const;
};

class [[gnu::packed]] Packet {
public:
	uint8_t command;
	uint8_t param[11];
	uint8_t _reserved[4];

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(Packet));
	}
	inline void setStartStopUnit(ATAPIStartStopMode mode) {
		clear();

		command  = ATAPI_START_STOP_UNIT;
		param[3] = mode & 3;
	}
	inline void setRead(uint32_t lba, size_t count) {
		clear();

		command  = ATAPI_READ12;
		param[1] = (lba >> 24) & 0xff;
		param[2] = (lba >> 16) & 0xff;
		param[3] = (lba >>  8) & 0xff;
		param[4] = (lba >>  0) & 0xff;
		param[5] = (count >> 24) & 0xff;
		param[6] = (count >> 16) & 0xff;
		param[7] = (count >>  8) & 0xff;
		param[8] = (count >>  0) & 0xff;
	}
	inline void setSetCDSpeed(uint16_t value) {
		clear();

		command  = ATAPI_SET_CD_SPEED;
		param[1] = (value >> 8) & 0xff;
		param[2] = (value >> 0) & 0xff;
	}
};

/* Device class */

enum DeviceError {
	NO_ERROR          = 0,
	UNSUPPORTED_OP    = 1,
	STATUS_TIMEOUT    = 2,
	DRIVE_ERROR       = 3,
	INCOMPLETE_DATA   = 4,
	CHECKSUM_MISMATCH = 5
};

enum DeviceFlag {
	DEVICE_PRIMARY      = 0 << 0,
	DEVICE_SECONDARY    = 1 << 0,
	DEVICE_READY        = 1 << 1,
	DEVICE_READ_ONLY    = 1 << 2,
	DEVICE_ATAPI        = 1 << 3,
	DEVICE_CDROM        = 1 << 4,
	DEVICE_HAS_TRIM     = 1 << 5, // Device supports TRIM/sector erasing
	DEVICE_HAS_FLUSH    = 1 << 6, // Device supports cache flushing
	DEVICE_HAS_LBA48    = 1 << 7, // Device supports 48-bit LBA addressing
	DEVICE_HAS_PACKET16 = 1 << 8  // Device requires 16-byte ATAPI packets
};

class Device {
private:
	inline uint8_t _read(CS0Register reg) {
		return uint8_t(SYS573_IDE_CS0_BASE[reg] & 0xff);
	}
	inline void _write(CS0Register reg, uint8_t value) {
		SYS573_IDE_CS0_BASE[reg] = value;
	}
	inline uint8_t _read(CS1Register reg) {
		return uint8_t(SYS573_IDE_CS1_BASE[reg] & 0xff);
	}
	inline void _write(CS1Register reg, uint8_t value) {
		SYS573_IDE_CS1_BASE[reg] = value;
	}

	inline void _select(uint8_t regFlags) {
		if (flags & DEVICE_SECONDARY)
			_write(CS0_DEVICE_SEL, regFlags | CS0_DEVICE_SEL_SECONDARY);
		else
			_write(CS0_DEVICE_SEL, regFlags | CS0_DEVICE_SEL_PRIMARY);
	}

	void _setLBA(uint64_t lba, uint16_t count);
	DeviceError _waitForStatus(uint8_t mask, uint8_t value, int timeout);
	DeviceError _command(uint8_t cmd, bool drdy = true);
	DeviceError _transferPIO(void *data, size_t length, bool write = false);
	DeviceError _transferDMA(void *data, size_t length, bool write = false);

	DeviceError _ideReadWrite(
		uintptr_t ptr, uint64_t lba, size_t count, bool write
	);

public:
	uint32_t flags;

	char     model[40], revision[8], serialNumber[20];
	uint64_t capacity;

	inline Device(uint32_t flags)
	: flags(flags), capacity(0) {}

	inline size_t getSectorSize(void) const {
		return (flags & DEVICE_ATAPI) ? ATAPI_SECTOR_SIZE : ATA_SECTOR_SIZE;
	}
	inline DeviceError ideRead(void *data, uint64_t lba, size_t count) {
		return _ideReadWrite(reinterpret_cast<uint32_t>(data), lba, count, false);
	}
	inline DeviceError ideWrite(const void *data, uint64_t lba, size_t count) {
		return _ideReadWrite(reinterpret_cast<uint32_t>(data), lba, count, true);
	}

	DeviceError enumerate(void);
	DeviceError ideFlushCache(void);
	DeviceError atapiPacket(
		Packet &packet, size_t transferLength = ATAPI_SECTOR_SIZE
	);
};

extern Device devices[2];

}
