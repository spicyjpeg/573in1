
#include <stddef.h>
#include <stdint.h>
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace rom {

/* ROM region dumpers */

// TODO: implement bounds checks in these

uint16_t *Region::getRawPtr(uint32_t offset, bool alignToChip) const {
	if (alignToChip)
		offset = 0;

	auto dest = reinterpret_cast<uint16_t *>(ptr + offset);

	util::assertAligned<uint16_t>(dest);

	return dest;
}

void Region::read(void *data, uint32_t offset, size_t length) const {
	auto source = reinterpret_cast<const uint32_t *>(ptr + offset);
	auto dest   = reinterpret_cast<uint32_t *>(data);

	util::assertAligned<uint32_t>(source);
	util::assertAligned<uint32_t>(dest);

	for (; length; length -= 4)
		*(dest++) = *(source++);
}

uint32_t Region::zipCRC32(uint32_t offset, size_t length, uint32_t crc) const {
	auto source = reinterpret_cast<const uint32_t *>(ptr + offset);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);
	crc         = ~crc;

	util::assertAligned<uint32_t>(source);

	for (; length; length -= 4) {
		uint32_t data = *(source++);

		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
	}

	return ~crc;
}

void RTCRegion::read(void *data, uint32_t offset, size_t length) const {
	auto source = reinterpret_cast<const uint16_t *>(ptr + offset * 2);
	auto dest   = reinterpret_cast<uint8_t *>(data);

	// The RTC is an 8-bit device connected to a 16-bit bus, i.e. each byte must
	// be read as a 16-bit value and then the upper 8 bits must be discarded.
	for (; length; length--)
		*(dest++) = *(source++) & 0xff;
}

uint32_t RTCRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	auto source = reinterpret_cast<const uint32_t *>(ptr + offset * 2);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);
	crc         = ~crc;

	util::assertAligned<uint32_t>(source);

	for (; length; length -= 2) {
		uint32_t data = *(source++);

		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 16;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
	}

	return ~crc;
}

Driver *RTCRegion::newDriver(void) const {
	return new RTCDriver(*this);
}

bool FlashRegion::isPresent(void) const {
	if (!inputs)
		return true;
	if (io::getJAMMAInputs() & inputs)
		return true;

	return false;
}

uint16_t *FlashRegion::getRawPtr(uint32_t offset, bool alignToChip) const {
	// The internal flash and PCMCIA cards can only be accessed 4 MB at a time.
	int bankOffset = offset / FLASH_BANK_LENGTH;
	int ptrOffset  = offset % FLASH_BANK_LENGTH;

	if (alignToChip)
		ptrOffset = 0;

	auto dest = reinterpret_cast<uint16_t *>(ptr + ptrOffset);

	util::assertAligned<uint16_t>(dest);
	io::setFlashBank(bank + bankOffset);

	return dest;
}

void FlashRegion::read(void *data, uint32_t offset, size_t length) const {
	// FIXME: this implementation will not handle unaligned reads and reads that
	// cross bank boundaries properly
	int bankOffset = offset / FLASH_BANK_LENGTH;
	int ptrOffset  = offset % FLASH_BANK_LENGTH;

	auto source = reinterpret_cast<const uint32_t *>(ptr + ptrOffset);
	auto dest   = reinterpret_cast<uint32_t *>(data);

	util::assertAligned<uint32_t>(source);
	util::assertAligned<uint32_t>(dest);
	io::setFlashBank(bank + bankOffset);

	for (; length; length -= 4)
		*(dest++) = *(source++);
}

uint32_t FlashRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	// FIXME: this implementation will not handle unaligned reads and reads that
	// cross bank boundaries properly
	int bankOffset = offset / FLASH_BANK_LENGTH;
	int ptrOffset  = offset % FLASH_BANK_LENGTH;

	auto source = reinterpret_cast<const uint32_t *>(ptr + ptrOffset);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);
	crc         = ~crc;

	util::assertAligned<uint32_t>(source);
	io::setFlashBank(bank + bankOffset);

	for (; length; length -= 4) {
		uint32_t data = *(source++);

		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
		data >>= 8;
		crc    = (crc >> 8) ^ table[(crc ^ data) & 0xff];
	}

	return ~crc;
}

/* Flash-specific functions */

enum JEDECCommand : uint16_t {
	_JEDEC_RESET           = 0xf0f0,
	_JEDEC_HANDSHAKE1      = 0xaaaa,
	_JEDEC_HANDSHAKE2      = 0x5555,
	_JEDEC_GET_ID          = 0x9090,
	_JEDEC_WRITE_BYTE      = 0xa0a0,
	_JEDEC_ERASE_HANDSHAKE = 0x8080,
	_JEDEC_ERASE_CHIP      = 0x1010,
	_JEDEC_ERASE_SECTOR    = 0x3030
};

enum IntelCommand : uint16_t {
	_INTEL_RESET         = 0xffff,
	_INTEL_GET_ID        = 0x9090,
	_INTEL_WRITE_BYTE    = 0x4040,
	_INTEL_ERASE_SECTOR1 = 0x2020,
	_INTEL_ERASE_SECTOR2 = 0xd0d0,
	_INTEL_GET_STATUS    = 0x7070,
	_INTEL_CLEAR_STATUS  = 0x5050,
	_INTEL_SUSPEND       = 0xb0b0,
	_INTEL_RESUME        = 0xd0d0
};

enum FlashIdentifier : uint16_t {
	_ID_MBM29F016A = 0x04 | (0xad << 8),
	_ID_MBM29F017A = 0x04 | (0x3d << 8),
	_ID_MBM29F040A = 0x04 | (0xa4 << 8),
	_ID_28F016S5   = 0x89 | (0xaa << 8),
	_ID_28F640J5   = 0x89 | (0x15 << 8)
};

bool FlashRegion::hasBootExecutable(void) const {
	// FIXME: this implementation will not detect executables that cross bank
	// boundaries (but it shouldn't matter as executables must be <4 MB anyway)
	auto data   = reinterpret_cast<const uint8_t *>(ptr + FLASH_EXECUTABLE_OFFSET);
	auto crcPtr = reinterpret_cast<const uint32_t *>(ptr + FLASH_CRC_OFFSET);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);

	io::setFlashBank(bank);

	auto &header = *reinterpret_cast<const util::ExecutableHeader *>(data);

	if (!header.validateMagic())
		return false;

	// The integrity of the executable is verified by calculating the CRC32 of
	// its bytes whose offsets are powers of 2 (i.e. the bytes at indices 0, 1,
	// 2, 4, 8 and so on). Note that the actual size of the executable is
	// header.textLength + util::EXECUTABLE_BODY_OFFSET, as the CRC is also
	// calculated on the header, but Konami's shell ignores the last 2048 bytes
	// due to a bug.
	size_t   length = header.textLength;
	uint32_t crc    = ~0;

	crc = (crc >> 8) ^ table[(crc ^ *data) & 0xff];

	for (size_t i = 1; i < length; i <<= 1)
		crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xff];

	return (~crc == *crcPtr);
}

uint32_t FlashRegion::getJEDECID(void) const {
	io::setFlashBank(bank);

	auto _ptr = reinterpret_cast<volatile uint16_t *>(ptr);

	_ptr[0x000] = _INTEL_RESET;
	_ptr[0x000] = _INTEL_RESET;
	_ptr[0x555] = _JEDEC_HANDSHAKE1;
	_ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	_ptr[0x555] = _JEDEC_GET_ID; // Same as _INTEL_GET_ID

	return _ptr[0] | (_ptr[1] << 16);
}

Driver *FlashRegion::newDriver(void) const {
	if (!isPresent()) {
		LOG("card not present");
		return new Driver(*this);
	}

	uint32_t id   = getJEDECID();
	uint16_t low  = ((id >> 0) & 0xff) | ((id >>  8) & 0xff00);
	uint16_t high = ((id >> 8) & 0xff) | ((id >> 16) & 0xff00);
	LOG("low=0x%04x, high=0x%04x", low, high);

	if (low == high) {
		// Two 8-bit chips for each bank
		switch (low) {
			case _ID_MBM29F016A:
			case _ID_MBM29F017A:
				// The MBM29F017A datasheet incorrectly lists the device ID as
				// 0xad rather than 0x3d in some places. The chip behaves pretty
				// much identically to the MBM29F016A.
				return new MBM29F016ADriver(*this);

			case _ID_MBM29F040A:
				return new MBM29F040ADriver(*this);

			case _ID_28F016S5:
				// The chip used by Konami is actually the Sharp LH28F016S,
				// which uses the same ID and command set as the Intel 28F016S5.
				return new Intel28F016S5Driver(*this);
		}
	//} else if (!high || (high == 0xff)) {
	} else {
		// Single 16-bit chip for each bank
		switch (low) {
			case _ID_28F640J5:
				// Found in "Centennial" branded flash cards. Not supported by
				// Konami's drivers.
				return new Intel28F640J5Driver(*this);
		}
	}

	return new Driver(*this);
}

const BIOSRegion  bios;
const RTCRegion   rtc;
const FlashRegion flash(SYS573_BANK_FLASH, 0x1000000);
const FlashRegion pcmcia[2]{
	{ SYS573_BANK_PCMCIA1, 0x4000000, io::JAMMA_PCMCIA_CD1 },
	{ SYS573_BANK_PCMCIA2, 0x4000000, io::JAMMA_PCMCIA_CD2 }
};

/* Data common to all chip drivers */

static constexpr int _FLASH_WRITE_TIMEOUT = 10000;
static constexpr int _FLASH_ERASE_TIMEOUT = 10000000;

const char *const DRIVER_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"CHIP_TIMEOUT",
	"CHIP_ERROR",
	"VERIFY_MISMATCH",
	"WRITE_PROTECTED"
};

static const ChipSize _DUMMY_CHIP_SIZE{
	.chipLength        = 0,
	.eraseSectorLength = 0
};

// The onboard flash and all Konami-supplied flash cards use 2 MB chips with 64
// KB sectors and an 8-bit bus.
static const ChipSize _STANDARD_CHIP_SIZE{
	.chipLength        = 2 * 0x200000,
	.eraseSectorLength = 2 * 0x10000
};

const ChipSize &Driver::getChipSize(void) const {
	return _DUMMY_CHIP_SIZE;
}

/* RTC RAM driver */

static const ChipSize _RTC_CHIP_SIZE{
	.chipLength        = 0x1ff8,
	.eraseSectorLength = 0x1ff8
};

void RTCDriver::write(uint32_t offset, uint16_t value) {
	auto ptr = reinterpret_cast<volatile uint32_t *>(_region.ptr + offset * 2);
	*ptr     = (value & 0x00ff) | ((value & 0xff00) << 8);
}

void RTCDriver::eraseSector(uint32_t offset) {
	auto ptr = reinterpret_cast<void *>(_region.ptr);

	__builtin_memset(ptr, 0, _region.regionLength * 2);
}

void RTCDriver::eraseChip(uint32_t offset) {
	eraseSector(offset);
}

DriverError RTCDriver::flushWrite(uint32_t offset, uint16_t value) {
	auto ptr = reinterpret_cast<volatile uint32_t *>(_region.ptr + offset * 2);
	value    = (value & 0x00ff) | ((value & 0xff00) << 8);

	if (ptr[offset] != value) {
		LOG(
			"mismatch @ 0x%08x, exp=0x%02x, got=0x%02x", offset, value,
			ptr[offset]
		);
		return VERIFY_MISMATCH;
	}

	return NO_ERROR;
}

DriverError RTCDriver::flushErase(uint32_t offset) {
	return flushWrite(offset, 0);
}

const ChipSize &RTCDriver::getChipSize(void) const {
	return _RTC_CHIP_SIZE;
}

/* Fujitsu MBM29F016A/017A driver */

enum FujitsuStatusFlag : uint16_t {
	_FUJITSU_STATUS_ERASE_TOGGLE = 1 << 2,
	_FUJITSU_STATUS_ERASE_START  = 1 << 3,
	_FUJITSU_STATUS_ERROR        = 1 << 5,
	_FUJITSU_STATUS_TOGGLE       = 1 << 6,
	_FUJITSU_STATUS_POLL_BIT     = 1 << 7
};

DriverError MBM29F016ADriver::_flush(
	uint32_t offset, uint16_t value, int timeout
) {
	volatile uint16_t *ptr  = _region.getRawPtr(offset & ~1);

	int     shift = (offset & 1) * 8;
	uint8_t byte  = (value >> shift) & 0xff;

	for (; timeout > 0; timeout--) {
		uint8_t status = (*ptr >> shift) & 0xff;

		if (status == byte)
			return NO_ERROR;

		if (!((status ^ byte) & _FUJITSU_STATUS_POLL_BIT)) {
			LOG(
				"mismatch @ 0x%08x, exp=0x%02x, got=0x%02x", offset, byte,
				status
			);

			*ptr = _JEDEC_RESET;
			return VERIFY_MISMATCH;
		}
		if (status & _FUJITSU_STATUS_ERROR) {
			LOG("error @ 0x%08x, stat=0x%02x", offset, status);

			*ptr = _JEDEC_RESET;
			return CHIP_ERROR;
		}

		delayMicroseconds(10);
	}

	LOG("timeout @ 0x%08x, stat=0x%02x", offset, (*ptr >> shift) & 0xff);

	*ptr = _JEDEC_RESET;
	return CHIP_TIMEOUT;
}

void MBM29F016ADriver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x000]  = _JEDEC_RESET;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[0x555]  = _JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void MBM29F016ADriver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x000]  = _JEDEC_RESET;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[0x555]  = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[offset] = _JEDEC_ERASE_SECTOR;
}

void MBM29F016ADriver::eraseChip(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);

	ptr[0x000] = _JEDEC_RESET;
	ptr[0x555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	ptr[0x555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	ptr[0x555] = _JEDEC_ERASE_CHIP;
}

DriverError MBM29F016ADriver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto error = _flush(offset, value, _FLASH_WRITE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, value, _FLASH_WRITE_TIMEOUT);
}

DriverError MBM29F016ADriver::flushErase(uint32_t offset) {
	auto error = _flush(offset, 0xffff, _FLASH_ERASE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, 0xffff, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &MBM29F016ADriver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* Fujitsu MBM29F040A driver */

// Konami's drivers handle this chip pretty much identically to the MBM29F016A,
// but using 0x5555/0x2aaa as command addresses instead of 0x555/0x2aa.

void MBM29F040ADriver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x0000] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void MBM29F040ADriver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);
	offset                 = (offset % FLASH_BANK_LENGTH) / 2;

	ptr[0x0000] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[offset] = _JEDEC_ERASE_SECTOR;
}

void MBM29F040ADriver::eraseChip(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset, true);

	ptr[0x0005] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_CHIP;
}

/* Intel 28F016S5 (Sharp LH28F016S) driver */

enum IntelStatusFlag : uint16_t {
	_INTEL_STATUS_DPS    = 1 << 1,
	_INTEL_STATUS_BWSS   = 1 << 2,
	_INTEL_STATUS_VPPS   = 1 << 3,
	_INTEL_STATUS_BWSLBS = 1 << 4,
	_INTEL_STATUS_ECLBS  = 1 << 5,
	_INTEL_STATUS_ESS    = 1 << 6,
	_INTEL_STATUS_WSMS   = 1 << 7
};

DriverError Intel28F016S5Driver::_flush(uint32_t offset, int timeout) {
	volatile uint16_t *ptr  = _region.getRawPtr(offset & ~1);

	int shift = (offset & 1) * 8;

	// Not required as all write/erase commands already put the chip into status
	// reading mode.
	//*ptr = _INTEL_GET_STATUS;

	for (; timeout > 0; timeout--) {
		uint8_t status = (*ptr >> shift) & 0xff;

		if (status & (_INTEL_STATUS_DPS | _INTEL_STATUS_VPPS)) {
			LOG("locked @ 0x%08x, stat=0x%02x", offset, status);

			*ptr = _INTEL_CLEAR_STATUS;
			return WRITE_PROTECTED;
		}
		if (status & (_INTEL_STATUS_BWSLBS | _INTEL_STATUS_ECLBS)) {
			LOG("error @ 0x%08x, stat=0x%02x", offset, status);

			*ptr = _INTEL_CLEAR_STATUS;
			return CHIP_ERROR;
		}
		if (status & _INTEL_STATUS_WSMS) {
			*ptr = _INTEL_CLEAR_STATUS;
			return NO_ERROR;
		}

		delayMicroseconds(10);
	}

	LOG("timeout @ 0x%08x, stat=0x%02x", offset, (*ptr >> shift) & 0xff);

	*ptr = _INTEL_CLEAR_STATUS;
	return CHIP_TIMEOUT;
}

void Intel28F016S5Driver::write(uint32_t offset, uint16_t value) {
	volatile uint16_t *ptr = _region.getRawPtr(offset);

	*ptr = _INTEL_RESET;
	*ptr = _INTEL_CLEAR_STATUS;
	*ptr = _INTEL_WRITE_BYTE;
	*ptr = value;
}

void Intel28F016S5Driver::eraseSector(uint32_t offset) {
	volatile uint16_t *ptr = _region.getRawPtr(offset);

	*ptr = _INTEL_RESET;
	*ptr = _INTEL_ERASE_SECTOR1;
	*ptr = _INTEL_ERASE_SECTOR2;
}

DriverError Intel28F016S5Driver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto error = _flush(offset, _FLASH_WRITE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, _FLASH_WRITE_TIMEOUT);
}

DriverError Intel28F016S5Driver::flushErase(uint32_t offset) {
	auto error = _flush(offset, _FLASH_ERASE_TIMEOUT);

	if (error)
		return error;

	return _flush(offset + 1, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &Intel28F016S5Driver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* Intel 28F640J5 driver */

static const ChipSize _28F640J5_CHIP_SIZE{
	.chipLength        = 0x800000,
	.eraseSectorLength = 0x20000
};

DriverError Intel28F640J5Driver::flushWrite(
	uint32_t offset, uint16_t value
) {
	return _flush(offset, _FLASH_WRITE_TIMEOUT);
}

DriverError Intel28F640J5Driver::flushErase(uint32_t offset) {
	return _flush(offset, _FLASH_ERASE_TIMEOUT);
}

const ChipSize &Intel28F640J5Driver::getChipSize(void) const {
	return _28F640J5_CHIP_SIZE;
}

/* BIOS ROM headers */

static const ShellInfo _SHELL_VERSIONS[]{
	{
		.name         = "700A01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerPtr    = reinterpret_cast<const uint8_t *>(DEV2_BASE | 0x40000),
		.headerHash   = 0x9c615f57
	}, {
		.name         = "700A01 (Gachagachamp)",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerPtr    = reinterpret_cast<const uint8_t *>(DEV2_BASE | 0x40000),
		.headerHash   = 0x7e31a844
	}, {
		.name         = "700B01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x61334),
		.headerPtr    = reinterpret_cast<const uint8_t *>(DEV2_BASE | 0x28000),
		.headerHash   = 0xb257d3b5
	}
};

bool SonyKernelHeader::validateMagic(void) const {
	return (
		util::hash(magic, sizeof(magic)) == "Sony Computer Entertainment Inc."_h
	);
}

bool OpenBIOSHeader::validateMagic(void) const {
	return (util::hash(magic, sizeof(magic)) == "OpenBIOS"_h);
}

bool ShellInfo::validateHash(void) const {
	return (util::hash(headerPtr, sizeof(util::ExecutableHeader)) == headerHash);
}

const ShellInfo *getShellInfo(void) {
	for (auto &shell : _SHELL_VERSIONS) {
		if (shell.validateHash())
			return &shell;
	}

	return nullptr;
}

}
