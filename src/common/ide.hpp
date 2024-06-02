
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/idedefs.hpp"
#include "common/util.hpp"
#include "ps1/registers573.h"

namespace ide {

static constexpr size_t ATA_SECTOR_SIZE   = 512;
static constexpr size_t ATAPI_SECTOR_SIZE = 2048;

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

class alignas(uint32_t) IdentifyBlock {
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

/* ATAPI data structures */

class alignas(uint32_t) SenseData {
public:
	uint8_t errorCode;              // 0
	uint8_t _reserved;              // 1
	uint8_t senseKey;               // 2
	uint8_t info[4];                // 3-6
	uint8_t additionalLength;       // 7
	uint8_t commandSpecificInfo[4]; // 8-11
	uint8_t asc;                    // 12
	uint8_t ascQualifier;           // 13
	uint8_t unitCode;               // 14
	uint8_t senseKeySpecificHeader; // 15
	uint8_t senseKeySpecific[2];    // 16-17

	inline uint32_t getErrorLBA(void) const {
		return 0
			| (info[0] << 24)
			| (info[1] << 16)
			| (info[2] <<  8)
			| (info[3] <<  0);
	}
	inline uint16_t getPackedASC(void) const {
		return asc | (ascQualifier << 8);
	}
};

class alignas(uint32_t) Packet {
public:
	uint8_t command;
	uint8_t param[11];
	uint8_t _reserved[4];

	inline void setTestUnitReady(void) {
		util::clear(*this);

		//command = ATAPI_TEST_UNIT_READY;
	}
	inline void setRequestSense(uint8_t additionalLength = 0) {
		util::clear(*this);

		command  = ATAPI_REQUEST_SENSE;
		param[3] = sizeof(SenseData) + additionalLength;
	}
	inline void setStartStopUnit(ATAPIStartStopMode mode) {
		util::clear(*this);

		command  = ATAPI_START_STOP_UNIT;
		param[3] = mode;
	}
	inline void setModeSense(
		ATAPIModePage page, size_t length,
		ATAPIModePageType type = MODE_PAGE_TYPE_CURRENT
	) {
		util::clear(*this);

		command  = ATAPI_MODE_SENSE;
		param[1] = (page & 0x3f) | (type << 6);
		param[6] = (length >> 8) & 0xff;
		param[7] = (length >> 0) & 0xff;
	}
	inline void setRead(uint32_t lba, size_t count) {
		util::clear(*this);

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
		util::clear(*this);

		command  = ATAPI_SET_CD_SPEED;
		param[1] = (value >> 8) & 0xff;
		param[2] = (value >> 0) & 0xff;
	}
};

/* Device class */

enum DeviceError {
	NO_ERROR          = 0,
	UNSUPPORTED_OP    = 1,
	NO_DRIVE          = 2,
	NOT_YET_READY     = 3,
	STATUS_TIMEOUT    = 4,
	CHECKSUM_MISMATCH = 5,
	DRIVE_ERROR       = 6,
	DISC_ERROR        = 7,
	DISC_CHANGED      = 8
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
	inline uint8_t _read(CS0Register reg) const {
		return uint8_t(SYS573_IDE_CS0_BASE[reg] & 0xff);
	}
	inline void _write(CS0Register reg, uint8_t value) const {
		SYS573_IDE_CS0_BASE[reg] = value;
	}
	inline uint8_t _read(CS1Register reg) const {
		return uint8_t(SYS573_IDE_CS1_BASE[reg] & 0xff);
	}
	inline void _write(CS1Register reg, uint8_t value) const {
		SYS573_IDE_CS1_BASE[reg] = value;
	}

	inline void _select(uint8_t selFlags) const {
		if (flags & DEVICE_SECONDARY)
			_write(CS0_DEVICE_SEL, selFlags | CS0_DEVICE_SEL_SECONDARY);
		else
			_write(CS0_DEVICE_SEL, selFlags | CS0_DEVICE_SEL_PRIMARY);
	}
	inline void _setCylinder(uint16_t value) const {
		_write(CS0_CYLINDER_L, (value >> 0) & 0xff);
		_write(CS0_CYLINDER_H, (value >> 8) & 0xff);
	}
	inline uint16_t _getCylinder(void) const {
		return _read(CS0_CYLINDER_L) | (_read(CS0_CYLINDER_H) << 8);
	}

	void _readPIO(void *data, size_t length) const;
	void _writePIO(const void *data, size_t length) const;
	bool _readDMA(void *data, size_t length) const;
	bool _writeDMA(const void *data, size_t length) const;

	DeviceError _waitForIdle(
		bool drdy = false, int timeout = 0, bool ignoreError = false
	);
	DeviceError _waitForDRQ(int timeout = 0, bool ignoreError = false);
	void _handleError(void);
	void _handleTimeout(void);
	DeviceError _resetDrive(void);

	DeviceError _ataSetLBA(uint64_t lba, size_t count, int timeout = 0);
	DeviceError _ataTransfer(
		uintptr_t ptr, uint64_t lba, size_t count, bool write
	);

	DeviceError _atapiRequestSense(void);
	DeviceError _atapiPacket(const Packet &packet, size_t dataLength = 0);
	DeviceError _atapiRead(uintptr_t ptr, uint32_t lba, size_t count);

public:
	uint32_t flags;

#ifdef ENABLE_FULL_IDE_DRIVER
	char     model[41], revision[9], serialNumber[21];
#endif
	uint64_t capacity;

	uint8_t   lastStatusReg, lastErrorReg, lastCountReg;
	SenseData lastSenseData;

	inline int getDriveIndex(void) const {
		return (flags / DEVICE_SECONDARY) & 1;
	}
	inline size_t getSectorSize(void) const {
		return (flags & DEVICE_ATAPI) ? ATAPI_SECTOR_SIZE : ATA_SECTOR_SIZE;
	}
	inline size_t getPacketSize(void) const {
		return (flags & DEVICE_HAS_PACKET16) ? 16 : 12;
	}
	inline bool isPointerAligned(const void *ptr) const {
		// DMA transfers require 4-byte alignment, while PIO transfers require
		// 2-byte alignment.
#if 0
		return bool(!(reinterpret_cast<uintptr_t>(ptr) % alignof(uint32_t)));
#else
		return bool(!(reinterpret_cast<uintptr_t>(ptr) % alignof(uint16_t)));
#endif
	}

	Device(uint32_t flags);
	DeviceError enumerate(void);
	DeviceError poll(void);

	DeviceError readData(void *data, uint64_t lba, size_t count);
	DeviceError writeData(const void *data, uint64_t lba, size_t count);
	DeviceError goIdle(bool standby = false);
	DeviceError startStopUnit(ATAPIStartStopMode mode);
	DeviceError flushCache(void);
};

extern const char *const DEVICE_ERROR_NAMES[];

extern Device devices[2];

static inline const char *getErrorString(DeviceError error) {
	return DEVICE_ERROR_NAMES[error];
}

}
