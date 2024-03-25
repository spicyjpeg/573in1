
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

/* Chip drivers */

enum DriverError {
	NO_ERROR        = 0,
	UNSUPPORTED_OP  = 1,
	CHIP_TIMEOUT    = 2,
	CHIP_ERROR      = 3,
	VERIFY_MISMATCH = 4,
	WRITE_PROTECTED = 5
};

struct ChipSize {
public:
	size_t chipLength, eraseSectorLength;
};

class Driver {
protected:
	const Region &_region;

public:
	inline Driver(const Region &region)
	: _region(region) {}

	// Note that all offsets must be multiples of 2, as writes are done in
	// halfwords.
	virtual ~Driver(void) {}
	virtual void write(uint32_t offset, uint16_t value) {}
	virtual void eraseSector(uint32_t offset) {}
	virtual void eraseChip(uint32_t offset) {}
	virtual DriverError flushWrite(uint32_t offset, uint16_t value) {
		return UNSUPPORTED_OP;
	}
	virtual DriverError flushErase(uint32_t offset) {
		return UNSUPPORTED_OP;
	}
	virtual const ChipSize &getChipSize(void) const;
};

class RTCDriver : public Driver {
public:
	inline RTCDriver(const RTCRegion &region)
	: Driver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class MBM29F016ADriver : public Driver {
private:
	DriverError _flush(uint32_t offset, uint16_t value, int timeout);

public:
	inline MBM29F016ADriver(const FlashRegion &region)
	: Driver(region) {}

	virtual void write(uint32_t offset, uint16_t value);
	virtual void eraseSector(uint32_t offset);
	virtual void eraseChip(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

class FujitsuUnknownDriver : public MBM29F016ADriver {
public:
	inline FujitsuUnknownDriver(const FlashRegion &region)
	: MBM29F016ADriver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	void eraseChip(uint32_t offset);
};

class LH28F016SDriver : public Driver {
private:
	DriverError _flush(uint32_t offset, int timeout);

public:
	inline LH28F016SDriver(const FlashRegion &region)
	: Driver(region) {}

	void write(uint32_t offset, uint16_t value);
	void eraseSector(uint32_t offset);
	DriverError flushWrite(uint32_t offset, uint16_t value);
	DriverError flushErase(uint32_t offset);
	const ChipSize &getChipSize(void) const;
};

extern const char *const DRIVER_ERROR_NAMES[];

static inline const char *getErrorString(DriverError error) {
	return DRIVER_ERROR_NAMES[error];
}

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
