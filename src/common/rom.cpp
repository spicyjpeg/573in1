
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
	auto source = reinterpret_cast<const uint32_t *>(ptr + offset);
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

bool FlashRegion::isPresent(void) const {
	if (!inputs)
		return true;
	if (io::getJAMMAInputs() & inputs)
		return true;

	return false;
}

void FlashRegion::read(void *data, uint32_t offset, size_t length) const {
	// The internal flash and PCMCIA cards can only be accessed 4 MB at a time.
	// FIXME: this implementation will not handle reads that cross bank
	// boundaries properly
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
	// FIXME: this implementation will not handle reads that cross bank
	// boundaries properly
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

enum SharpCommand : uint16_t {
	_SHARP_RESET         = 0xffff,
	_SHARP_GET_ID        = 0x9090,
	_SHARP_WRITE_BYTE    = 0x4040,
	_SHARP_ERASE_SECTOR1 = 0x2020,
	_SHARP_ERASE_SECTOR2 = 0xd0d0,
	_SHARP_GET_STATUS    = 0x7070,
	_SHARP_CLEAR_STATUS  = 0x5050,
	_SHARP_SUSPEND       = 0xb0b0,
	_SHARP_RESUME        = 0xd0d0
};

bool FlashRegion::hasBootExecutable(void) const {
	// FIXME: this implementation will not detect executables that cross bank
	// boundaries (but it shouldn't matter as executables must be <4 MB anyway)
	auto data   = reinterpret_cast<const uint8_t *>(ptr + FLASH_EXE_OFFSET);
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
	auto ptr = getFlashPtr();

	ptr[0x000] = _SHARP_RESET;
	ptr[0x000] = _SHARP_RESET;
	ptr[0x555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	ptr[0x555] = _JEDEC_GET_ID; // Same as _SHARP_GET_ID

	return ptr[0] | (ptr[1] << 16);
}

const BIOSRegion  bios;
const RTCRegion   rtc;
const FlashRegion flash(SYS573_BANK_FLASH, 0x1000000);
const FlashRegion pcmcia[2]{
	{ SYS573_BANK_PCMCIA1, 0x4000000, io::JAMMA_PCMCIA_CD1 },
	{ SYS573_BANK_PCMCIA2, 0x4000000, io::JAMMA_PCMCIA_CD2 }
};

/* Data common to all flash chip drivers */

static constexpr int _FLASH_WRITE_TIMEOUT = 10000;
static constexpr int _FLASH_ERASE_TIMEOUT = 10000000;

const char *const FLASH_DRIVER_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"CHIP_TIMEOUT",
	"CHIP_ERROR",
	"VERIFY_MISMATCH",
	"WRITE_PROTECTED"
};

static const FlashChipSize _DUMMY_CHIP_SIZE{
	.chipLength        = 0,
	.eraseSectorLength = 0
};

// The onboard flash and all Konami-supplied flash cards use 2 MB chips with 64
// KB sectors.
static const FlashChipSize _STANDARD_CHIP_SIZE{
	.chipLength        = 0x200000,
	.eraseSectorLength = 0x10000
};

const FlashChipSize &FlashDriver::getChipSize(void) const {
	return _DUMMY_CHIP_SIZE;
}

/* Fujitsu MBM29F016A driver */

enum FujitsuStatusFlag : uint16_t {
	_FUJITSU_STATUS_ERASE_TOGGLE = 0x101 << 2,
	_FUJITSU_STATUS_ERASE_START  = 0x101 << 3,
	_FUJITSU_STATUS_ERROR        = 0x101 << 5,
	_FUJITSU_STATUS_TOGGLE       = 0x101 << 6,
	_FUJITSU_STATUS_POLL_BIT     = 0x101 << 7
};

FlashDriverError MBM29F016AFlashDriver::_flush(
	volatile uint16_t *ptr, uint16_t value, int timeout
) {
	for (; timeout > 0; timeout--) {
		auto status = *ptr;

		if (status == value)
			return NO_ERROR;

		if (!((status ^ value) & _FUJITSU_STATUS_POLL_BIT)) {
			LOG("mismatch, exp=0x%04x, got=0x%04x", value, status);
			return VERIFY_MISMATCH;
		}
		if (status & _FUJITSU_STATUS_ERROR) {
			LOG("MBM29F016A error, stat=0x%04x", status);
			return CHIP_ERROR;
		}

		delayMicroseconds(1);
	}

	LOG("MBM29F016A timeout, stat=0x%04x", *ptr);
	return CHIP_TIMEOUT;
}

void MBM29F016AFlashDriver::write(uint32_t offset, uint16_t value) {
	auto ptr = _region.getFlashPtr();

	ptr[0x000]  = _JEDEC_RESET;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[0x555]  = _JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void MBM29F016AFlashDriver::eraseSector(uint32_t offset) {
	auto ptr = _region.getFlashPtr();

	ptr[0x000]  = _JEDEC_RESET;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[0x555]  = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x555]  = _JEDEC_HANDSHAKE1;
	ptr[0x2aa]  = _JEDEC_HANDSHAKE2;
	ptr[offset] = _JEDEC_ERASE_SECTOR;
}

void MBM29F016AFlashDriver::eraseChip(uint32_t offset) {
	auto ptr = _region.getFlashPtr();

	ptr[0x000] = _JEDEC_RESET;
	ptr[0x555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	ptr[0x555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aa] = _JEDEC_HANDSHAKE2;
	ptr[0x555] = _JEDEC_ERASE_CHIP;
}

FlashDriverError MBM29F016AFlashDriver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto ptr = &(_region.getFlashPtr())[offset];

	return _flush(ptr, value, _FLASH_WRITE_TIMEOUT);
}

FlashDriverError MBM29F016AFlashDriver::flushErase(uint32_t offset) {
	auto ptr = &(_region.getFlashPtr())[offset];

	return _flush(ptr, 0xffff, _FLASH_ERASE_TIMEOUT);
}

const FlashChipSize &MBM29F016AFlashDriver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* Unknown Fujitsu chip driver */

// Konami's drivers handle this chip pretty much identically to the MBM29F016A,
// but using 0x5555/0x2aaa as command addresses instead of 0x555/0x2aa. This
// could actually be a >2 MB chip.

void FujitsuUnknownFlashDriver::write(uint32_t offset, uint16_t value) {
	auto ptr = _region.getFlashPtr();

	ptr[0x0000] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_WRITE_BYTE;
	ptr[offset] = value;
}

void FujitsuUnknownFlashDriver::eraseSector(uint32_t offset) {
	auto ptr = _region.getFlashPtr();

	ptr[0x0000] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[offset] = _JEDEC_ERASE_SECTOR;
}

void FujitsuUnknownFlashDriver::eraseChip(uint32_t offset) {
	auto ptr = _region.getFlashPtr();

	ptr[0x0005] = _JEDEC_RESET;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_HANDSHAKE;
	ptr[0x5555] = _JEDEC_HANDSHAKE1;
	ptr[0x2aaa] = _JEDEC_HANDSHAKE2;
	ptr[0x5555] = _JEDEC_ERASE_CHIP;
}

/* Sharp LH28F016S driver */

enum SharpStatusFlag : uint16_t {
	_SHARP_STATUS_DPS    = 0x101 << 1,
	_SHARP_STATUS_BWSS   = 0x101 << 2,
	_SHARP_STATUS_VPPS   = 0x101 << 3,
	_SHARP_STATUS_BWSLBS = 0x101 << 4,
	_SHARP_STATUS_ECLBS  = 0x101 << 5,
	_SHARP_STATUS_ESS    = 0x101 << 6,
	_SHARP_STATUS_WSMS   = 0x101 << 7
};

FlashDriverError LH28F016SFlashDriver::_flush(
	volatile uint16_t *ptr, int timeout
) {
	// Not required as all write/erase commands already put the chip into status
	// reading mode.
	//*ptr = _SHARP_GET_STATUS;

	for (; timeout > 0; timeout--) {
		auto status = *ptr;

		if (status & (_SHARP_STATUS_DPS | _SHARP_STATUS_VPPS)) {
			*ptr = _SHARP_CLEAR_STATUS;

			LOG("LH28F016S locked, stat=0x%04x", status);
			return WRITE_PROTECTED;
		}
		if (status & (_SHARP_STATUS_BWSLBS | _SHARP_STATUS_ECLBS)) {
			*ptr = _SHARP_CLEAR_STATUS;

			LOG("LH28F016S error, stat=0x%04x", status);
			return CHIP_ERROR;
		}

		if (status & _SHARP_STATUS_WSMS)
			return NO_ERROR;

		delayMicroseconds(1);
	}

	LOG("LH28F016S timeout, stat=0x%04x", *ptr);
	return CHIP_TIMEOUT;
}

void LH28F016SFlashDriver::write(uint32_t offset, uint16_t value) {
	auto ptr = &(_region.getFlashPtr())[offset];

	*ptr = _SHARP_RESET;
	*ptr = _SHARP_CLEAR_STATUS;
	*ptr = _SHARP_WRITE_BYTE;
	*ptr = value;
}

void LH28F016SFlashDriver::eraseSector(uint32_t offset) {
	auto ptr = &(_region.getFlashPtr())[offset];

	*ptr = _SHARP_RESET;
	*ptr = _SHARP_ERASE_SECTOR1;
	*ptr = _SHARP_ERASE_SECTOR2;
}

FlashDriverError LH28F016SFlashDriver::flushWrite(
	uint32_t offset, uint16_t value
) {
	auto ptr = &(_region.getFlashPtr())[offset];

	return _flush(ptr, _FLASH_WRITE_TIMEOUT);
}

FlashDriverError LH28F016SFlashDriver::flushErase(uint32_t offset) {
	auto ptr = &(_region.getFlashPtr())[offset];

	return _flush(ptr, _FLASH_ERASE_TIMEOUT);
}

const FlashChipSize &LH28F016SFlashDriver::getChipSize(void) const {
	return _STANDARD_CHIP_SIZE;
}

/* Flash chip identification */

enum FlashIdentifier : uint16_t {
	// NOTE: the MBM29F017A datasheet incorrectly lists the device ID as 0xad in
	// some places. The chip behaves pretty much identically to the MBM29F016A.
	_ID_MBM29F016A      = 0x04 | (0xad << 8),
	_ID_MBM29F017A      = 0x04 | (0x3d << 8),
	_ID_FUJITSU_UNKNOWN = 0x04 | (0xa4 << 8),
	_ID_LH28F016S       = 0x89 | (0xaa << 8)
};

FlashDriver *newFlashDriver(FlashRegion &region) {
	if (!region.isPresent()) {
		LOG("card not present");
		return new FlashDriver(region);
	}

	uint32_t id   = region.getJEDECID();
	uint16_t low  = ((id >> 0) & 0xff) | ((id >>  8) & 0xff00);
	uint16_t high = ((id >> 8) & 0xff) | ((id >> 16) & 0xff00);
	LOG("low=0x%04x, high=0x%04x", low, high);

	if (low != high) {
		// TODO: implement single-chip (16-bit) flash support
		return new FlashDriver(region);
	} else {
		switch (low) {
			case _ID_MBM29F016A:
			case _ID_MBM29F017A:
				return new MBM29F016AFlashDriver(region);

			case _ID_FUJITSU_UNKNOWN:
				return new FujitsuUnknownFlashDriver(region);

			case _ID_LH28F016S:
				return new LH28F016SFlashDriver(region);

			default:
				return new FlashDriver(region);
		}
	}
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
