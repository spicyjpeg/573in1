
#include "ps1/registers.h"
#include "io.hpp"
#include "rom.hpp"
#include "util.hpp"

namespace rom {

/* ROM region dumpers */

// TODO: implement bounds checks in these

static constexpr size_t   _FLASH_BANK_LENGTH = 0x400000;
static constexpr uint32_t _FLASH_CRC_OFFSET  = 0x20;
static constexpr uint32_t _FLASH_EXE_OFFSET  = 0x24;

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
	int bankOffset = offset / _FLASH_BANK_LENGTH;
	int ptrOffset  = offset % _FLASH_BANK_LENGTH;

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
	int bankOffset = offset / _FLASH_BANK_LENGTH;
	int ptrOffset  = offset % _FLASH_BANK_LENGTH;

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

bool FlashRegion::hasBootExecutable(void) const {
	auto data   = reinterpret_cast<const uint8_t *>(ptr + _FLASH_EXE_OFFSET);
	auto crcPtr = reinterpret_cast<const uint32_t *>(ptr + _FLASH_CRC_OFFSET);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);

	io::setFlashBank(bank);

	auto &header = *reinterpret_cast<const util::ExecutableHeader *>(data);

	if (!header.validateMagic())
		return false;

	// The integrity of the executable is verified by calculating the CRC32 of
	// its bytes whose offsets are powers of 2 (i.e. the bytes at indices 0, 1,
	// 2, 4, 8 and so on).
	size_t   length = header.textLength + 2048;
	uint32_t crc    = ~0;

	crc = (crc >> 8) ^ table[(crc ^ *data) & 0xff];

	for (size_t i = 1; i < length; i <<= 1)
		crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xff];

	return (~crc == *crcPtr);
}

const BIOSRegion  bios;
const RTCRegion   rtc;
const FlashRegion flash(SYS573_BANK_FLASH, 0x1000000);
const FlashRegion pcmcia[2]{
	{ SYS573_BANK_PCMCIA1, 0x4000000, io::JAMMA_PCMCIA_CD1 },
	{ SYS573_BANK_PCMCIA2, 0x4000000, io::JAMMA_PCMCIA_CD2 }
};

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
