
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/util.hpp"
#include "ps1/registers.h"

namespace rom {

/* ROM region dumpers */

static constexpr size_t   FLASH_BANK_LENGTH       = 0x400000;
static constexpr uint32_t FLASH_HEADER_OFFSET     = 0x00;
static constexpr uint32_t FLASH_CRC_OFFSET        = 0x20;
static constexpr uint32_t FLASH_EXECUTABLE_OFFSET = 0x24;

class Driver;

class Region {
public:
	uintptr_t ptr;
	size_t    regionLength;

	inline Region(uintptr_t ptr, size_t regionLength)
	: ptr(ptr), regionLength(regionLength) {}

	virtual bool isPresent(void) const { return true; }
	virtual uint16_t *getRawPtr(uint32_t offset, bool alignToChip = false) const;
	virtual void read(void *data, uint32_t offset, size_t length) const;
	virtual uint32_t zipCRC32(
		uint32_t offset, size_t length, uint32_t crc = 0
	) const;

	virtual bool hasBootExecutable(void) const { return false; }
	virtual uint32_t getJEDECID(void) const { return 0; }
	virtual Driver *newDriver(void) const { return nullptr; }
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

	Driver *newDriver(void) const;
};

class FlashRegion : public Region {
public:
	int      bank;
	uint32_t inputs;

	inline FlashRegion(int bank, size_t regionLength, uint32_t inputs = 0)
	: Region(DEV0_BASE, regionLength), bank(bank), inputs(inputs) {}

	bool isPresent(void) const;
	uint16_t *getRawPtr(uint32_t offset, bool alignToChip = false) const;
	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	bool hasBootExecutable(void) const;
	uint32_t getJEDECID(void) const;
	Driver *newDriver(void) const;
};

extern const BIOSRegion  bios;
extern const RTCRegion   rtc;
extern const FlashRegion flash, pcmcia[2];

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
