
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/util.hpp"
#include "ps1/registers.h"

namespace rom {

/* ROM region dumpers */

static constexpr size_t   FLASH_BANK_LENGTH = 0x400000;
static constexpr uint32_t FLASH_CRC_OFFSET  = 0x20;
static constexpr uint32_t FLASH_EXE_OFFSET  = 0x24;

class Region {
public:
	uintptr_t ptr;
	size_t    regionLength;

	inline Region(uintptr_t ptr, size_t regionLength)
	: ptr(ptr), regionLength(regionLength) {}

	virtual bool isPresent(void) const { return true; }
	virtual void read(void *data, uint32_t offset, size_t length) const;
	virtual uint32_t zipCRC32(
		uint32_t offset, size_t length, uint32_t crc = 0
	) const;
};

class BIOSRegion : public Region {
public:
	inline BIOSRegion(void)
	: Region(DEV2_BASE, 0x80000) {}
};

class RTCRegion : public Region {
public:
	inline RTCRegion(void)
	: Region(DEV0_BASE | 0x620000, 0x1ff8) {}

	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;
};

class FlashRegion : public Region {
public:
	int      bank;
	uint32_t inputs;

	inline FlashRegion(int bank, size_t regionLength, uint32_t inputs = 0)
	: Region(DEV0_BASE, regionLength), bank(bank), inputs(inputs) {}

	bool isPresent(void) const;
	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	bool hasBootExecutable(void) const;
	uint16_t getJEDECID(void) const;
};

extern const BIOSRegion  bios;
extern const RTCRegion   rtc;
extern const FlashRegion flash, pcmcia[2];

/* Flash memory driver */

enum FlashManufacturer : uint8_t {
	MANUF_FUJITSU = 0x04,
	MANUF_SHARP   = 0x89
};

enum FlashJEDECID : uint16_t {
	ID_FUJITSU_MBM29F016A     = MANUF_FUJITSU | (0xad << 8),
	ID_FUJITSU_MBM29F016A_ALT = MANUF_FUJITSU | (0x3d << 8),
	ID_FUJITSU_UNKNOWN        = MANUF_FUJITSU | (0xa4 << 8),
	ID_SHARP_LH28F016S        = MANUF_SHARP   | (0xaa << 8)
};

enum FlashCommand : uint16_t {
	SHARP_ERASE_SECTOR = 0x2020,
	SHARP_WRITE        = 0x4040,
	SHARP_CLEAR_STATUS = 0x5050,
	SHARP_GET_STATUS   = 0x7070,
	SHARP_GET_JEDEC_ID = 0x9090,
	SHARP_SUSPEND      = 0xb0b0,
	SHARP_RESUME       = 0xd0d0,
	SHARP_RESET        = 0xffff,

	FUJITSU_ERASE_CHIP   = 0x1010,
	FUJITSU_ERASE_SECTOR = 0x3030,
	FUJITSU_HANDSHAKE2   = 0x5555,
	FUJITSU_ERASE        = 0x8080,
	FUJITSU_GET_JEDEC_ID = 0x9090,
	FUJITSU_WRITE        = 0xa0a0,
	FUJITSU_HANDSHAKE1   = 0xaaaa,
	FUJITSU_RESET        = 0xf0f0
};

/* BIOS ROM headers */

struct [[gnu::packed]] SonyKernelHeader {
public:
	uint8_t  day, month;
	uint16_t year;
	uint32_t flags;
	uint8_t  magic[32], _pad[4], version[36];

	bool validateMagic(void) const;
};

struct [[gnu::packed]] OpenBIOSHeader {
public:
	uint8_t  magic[8];
	uint32_t idNameLength, idDescLength, idType;
	uint8_t  idData[24];

	inline size_t getBuildID(char *output) const {
		return util::hexToString(output, &idData[idNameLength], idDescLength);
	}

	bool validateMagic(void) const;
};

struct ShellInfo {
public:
	const char    *name, *bootFileName;
	const uint8_t *headerPtr;
	util::Hash    headerHash;

	bool validateHash(void) const;
};

static const auto &sonyKernelHeader =
	*reinterpret_cast<const SonyKernelHeader *>(DEV2_BASE | 0x100);
static const auto &openBIOSHeader =
	*reinterpret_cast<const OpenBIOSHeader *>(DEV2_BASE | 0x78);

const ShellInfo *getShellInfo(void);

}
