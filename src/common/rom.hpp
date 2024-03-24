
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/io.hpp"
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

	inline volatile uint16_t *getFlashPtr(void) const {
		io::setFlashBank(bank);

		return reinterpret_cast<volatile uint16_t *>(ptr);
	}

	bool isPresent(void) const;
	void read(void *data, uint32_t offset, size_t length) const;
	uint32_t zipCRC32(uint32_t offset, size_t length, uint32_t crc = 0) const;

	bool hasBootExecutable(void) const;
	uint32_t getJEDECID(void) const;
};

extern const BIOSRegion  bios;
extern const RTCRegion   rtc;
extern const FlashRegion flash, pcmcia[2];

/* Flash chip drivers */

enum FlashDriverError {
	NO_ERROR        = 0,
	UNSUPPORTED_OP  = 1,
	CHIP_TIMEOUT    = 2,
	CHIP_ERROR      = 3,
	VERIFY_MISMATCH = 4,
	WRITE_PROTECTED = 5
};

struct FlashChipSize {
public:
	size_t chipLength, eraseSectorLength;
};

class FlashDriver {
protected:
	FlashRegion &_region;

public:
	inline FlashDriver(FlashRegion &region)
	: _region(region) {}

	virtual ~FlashDriver(void) {}
	virtual void write(uint32_t offset, uint16_t value) {}
	virtual void eraseSector(uint32_t offset) {}
	virtual void eraseChip(uint32_t offset) {}
	virtual FlashDriverError flushWrite(uint32_t offset, uint16_t value) {
		return UNSUPPORTED_OP;
	}
	virtual FlashDriverError flushErase(uint32_t offset) {
		return UNSUPPORTED_OP;
	}
	virtual const FlashChipSize &getChipSize(void) const;
};

class MBM29F016AFlashDriver : public FlashDriver {
private:
	FlashDriverError _flush(
		volatile uint16_t *ptr, uint16_t value, int timeout
	);

public:
	inline MBM29F016AFlashDriver(FlashRegion &region)
	: FlashDriver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	FlashDriverError flushWrite(uint32_t offset, uint16_t value);
	FlashDriverError flushErase(uint32_t offset);
	const FlashChipSize &getChipSize(void) const;
};

class FujitsuUnknownFlashDriver : public MBM29F016AFlashDriver {
public:
	inline FujitsuUnknownFlashDriver(FlashRegion &region)
	: MBM29F016AFlashDriver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
};

class LH28F016SFlashDriver : public FlashDriver {
private:
	FlashDriverError _flush(volatile uint16_t *ptr, int timeout);

public:
	inline LH28F016SFlashDriver(FlashRegion &region)
	: FlashDriver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	FlashDriverError flushWrite(uint32_t offset, uint16_t value);
	FlashDriverError flushErase(uint32_t offset);
	const FlashChipSize &getChipSize(void) const;
};

extern const char *const FLASH_DRIVER_ERROR_NAMES[];

static inline const char *getErrorString(FlashDriverError error) {
	return FLASH_DRIVER_ERROR_NAMES[error];
}

FlashDriver *newFlashDriver(FlashRegion &region);

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
