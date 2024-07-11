
#include <stddef.h>
#include <stdint.h>
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/romdrivers.hpp"
#include "common/util.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"

namespace rom {

/* ROM region dumpers */

// TODO: implement bounds checks in these

uint16_t *Region::getRawPtr(uint32_t offset, bool alignToChip) const {
#if 0
	if (bank >= 0)
		io::setFlashBank(bank);
#endif
	if (alignToChip)
		offset = 0;

	auto dest = reinterpret_cast<uint16_t *>(ptr + offset);

	util::assertAligned<uint16_t>(dest);

	return dest;
}

void Region::read(void *data, uint32_t offset, size_t length) const {
	auto source = reinterpret_cast<const uint32_t *>(ptr + offset);
	auto dest   = reinterpret_cast<uint32_t *>(data);

	// TODO: use memcpy() instead once an optimized implementation is added
	util::assertAligned<uint32_t>(source);
	util::assertAligned<uint32_t>(dest);

	for (; length >= 32; length -= 32, dest += 8, source += 8) {
		dest[0] = source[0];
		dest[1] = source[1];
		dest[2] = source[2];
		dest[3] = source[3];
		dest[4] = source[4];
		dest[5] = source[5];
		dest[6] = source[6];
		dest[7] = source[7];
	}

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
	for (; length >= 8; length -= 8, dest += 8, source += 8) {
		dest[0] = uint8_t(source[0]);
		dest[1] = uint8_t(source[1]);
		dest[2] = uint8_t(source[2]);
		dest[3] = uint8_t(source[3]);
		dest[4] = uint8_t(source[4]);
		dest[5] = uint8_t(source[5]);
		dest[6] = uint8_t(source[6]);
		dest[7] = uint8_t(source[7]);
	}

	for (; length; length--)
		*(dest++) = uint8_t(*(source++));
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
	if (!_inputs)
		return true;
	if (io::getJAMMAInputs() & _inputs)
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
	auto bankOffset = offset / FLASH_BANK_LENGTH;
	auto ptrOffset  = offset % FLASH_BANK_LENGTH;

	io::setFlashBank(bank + bankOffset);
	Region::read(data, ptrOffset, length);
}

uint32_t FlashRegion::zipCRC32(
	uint32_t offset, size_t length, uint32_t crc
) const {
	// FIXME: this implementation will not handle unaligned reads and reads that
	// cross bank boundaries properly
	auto bankOffset = offset / FLASH_BANK_LENGTH;
	auto ptrOffset  = offset % FLASH_BANK_LENGTH;

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

enum FlashIdentifier : uint16_t {
	_ID_AM29F016   = 0x01 | (0xad << 8),
	_ID_AM29F040   = 0x01 | (0xa4 << 8),
	_ID_MBM29F016A = 0x04 | (0xad << 8),
	_ID_MBM29F017A = 0x04 | (0x3d << 8),
	_ID_MBM29F040A = 0x04 | (0xa4 << 8),
	_ID_28F016S5   = 0x89 | (0xaa << 8),
	_ID_28F640J5   = 0x89 | (0x15 << 8)
};

const util::ExecutableHeader *FlashRegion::getBootExecutableHeader(void) const {
	// FIXME: this implementation will not detect executables that cross bank
	// boundaries (but it shouldn't matter as executables must be <4 MB anyway)
	auto data   = reinterpret_cast<const uint8_t *>(ptr + FLASH_EXECUTABLE_OFFSET);
	auto crcPtr = reinterpret_cast<const uint32_t *>(ptr + FLASH_CRC_OFFSET);
	auto table  = reinterpret_cast<const uint32_t *>(CACHE_BASE);

	io::setFlashBank(bank);

	auto header = reinterpret_cast<const util::ExecutableHeader *>(data);

	if (!header->validateMagic())
		return nullptr;

	// The integrity of the executable is verified by calculating the CRC32 of
	// its bytes whose offsets are powers of 2 (i.e. the bytes at indices 0, 1,
	// 2, 4, 8 and so on). Note that the actual size of the executable is
	// header.textLength + util::EXECUTABLE_BODY_OFFSET, as the CRC is also
	// calculated on the header, but Konami's shell ignores the last 2048 bytes
	// due to a bug.
	size_t   length = header->textLength;
	uint32_t crc    = ~0;

	crc = (crc >> 8) ^ table[(crc ^ *data) & 0xff];

	for (size_t i = 1; i < length; i <<= 1)
		crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xff];

	if (~crc != *crcPtr) {
		LOG_ROM("CRC32 mismatch");
		LOG_ROM("exp=0x%08x", ~crc);
		LOG_ROM("got=0x%08x", *crcPtr);
		return nullptr;
	}

	return header;
}

uint32_t FlashRegion::getJEDECID(void) const {
	io::setFlashBank(bank);

	auto _ptr = reinterpret_cast<volatile uint16_t *>(ptr);

	_ptr[0x000] = JEDEC_RESET;
	_ptr[0x000] = INTEL_RESET;
	_ptr[0x555] = JEDEC_HANDSHAKE1;
	_ptr[0x2aa] = JEDEC_HANDSHAKE2;
	_ptr[0x555] = JEDEC_GET_ID; // Same as INTEL_GET_ID

	uint32_t id1 = _ptr[0] | (_ptr[1] << 16);

	_ptr[0x000] = JEDEC_RESET;
	_ptr[0x000] = INTEL_RESET;

	uint32_t id2 = _ptr[0] | (_ptr[1] << 16);

	if (id1 == id2) {
		LOG_ROM("chip not responding to commands");
		return 0;
	}

	return id1;
}

size_t FlashRegion::getActualLength(void) const {
	if (!bank)
		return regionLength;

	// Issue a JEDEC ID command to the first chip, then keep resetting all other
	// chips until the first one is also reset, indicating that the address has
	// wrapped around.
	io::setFlashBank(bank);

	auto _ptr = reinterpret_cast<volatile uint16_t *>(ptr);

	_ptr[0x000] = JEDEC_RESET;
	_ptr[0x000] = INTEL_RESET;
	_ptr[0x555] = JEDEC_HANDSHAKE1;
	_ptr[0x2aa] = JEDEC_HANDSHAKE2;
	_ptr[0x555] = JEDEC_GET_ID; // Same as INTEL_GET_ID

	uint32_t id1 = _ptr[0] | (_ptr[1] << 16);

	int bankOffset = 1;
	int numBanks   = regionLength / FLASH_BANK_LENGTH;

	for (; bankOffset < numBanks; bankOffset++) {
		io::setFlashBank(bank + bankOffset);

		_ptr[0x000] = JEDEC_RESET;
		_ptr[0x000] = INTEL_RESET;

		io::setFlashBank(bank);

		uint32_t id2 = _ptr[0] | (_ptr[1] << 16);

		if (id1 != id2)
			break;
	}

	_ptr[0x000] = JEDEC_RESET;
	_ptr[0x000] = INTEL_RESET;

	uint32_t id3 = _ptr[0] | (_ptr[1] << 16);

	if (id1 == id3) {
		LOG_ROM("chip not responding to commands");
		return 0;
	}
	if (bankOffset == numBanks) {
		// There is at least one game that uses a "64 MB" card (actually two 32
		// MB cards in an adapter), but it's rare enough that forcing the user
		// to select the card size manually makes sense.
		LOG_ROM("no mirroring detected");
		return 0;
	}

	return bankOffset * FLASH_BANK_LENGTH;
}

Driver *FlashRegion::newDriver(void) const {
	if (!isPresent()) {
		LOG_ROM("card not present");
		return new Driver(*this);
	}

	uint32_t id   = getJEDECID();
	uint16_t low  = ((id >> 0) & 0xff) | ((id >>  8) & 0xff00);
	uint16_t high = ((id >> 8) & 0xff) | ((id >> 16) & 0xff00);

	LOG_ROM("low=0x%04x, high=0x%04x", low, high);

	if (low == high) {
		// Two 8-bit chips for each bank
		switch (low) {
			case _ID_AM29F016:
			case _ID_MBM29F016A:
			case _ID_MBM29F017A:
				// The MBM29F017A datasheet incorrectly lists the device ID as
				// 0xad rather than 0x3d in some places. The chip behaves pretty
				// much identically to the MBM29F016A.
				return new AM29F016Driver(*this);

			case _ID_AM29F040:
			case _ID_MBM29F040A:
				return new AM29F040Driver(*this);

			case _ID_28F016S5:
				// The chip used by Konami is actually the Sharp LH28F016S,
				// which uses the same ID and command set as the Intel 28F016S5.
				return new Intel28F016S5Driver(*this);
		}
#if 0
	} else if (!high || (high == 0xff)) {
#else
	} else {
#endif
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
const FlashRegion flash(0x1000000, SYS573_BANK_FLASH);
const FlashRegion pcmcia[2]{
	{ 0x4000000, SYS573_BANK_PCMCIA1, io::JAMMA_PCMCIA_CD1 },
	{ 0x4000000, SYS573_BANK_PCMCIA2, io::JAMMA_PCMCIA_CD2 }
};

/* BIOS ROM headers */

static const ShellInfo _KONAMI_SHELLS[]{
	{
		.name         = "700A01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerHash   = 0x9c615f57,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x40000
		)
	}, {
		.name         = "700A01 (Gachagachamp)",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x40890),
		.headerHash   = 0x7e31a844,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x40000
		)
	}, {
		.name         = "700B01",
		.bootFileName = reinterpret_cast<const char *>(DEV2_BASE | 0x61334),
		.headerHash   = 0xb257d3b5,
		.header       = reinterpret_cast<const util::ExecutableHeader *>(
			DEV2_BASE | 0x28000
		)
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
	return (util::hash(
		reinterpret_cast<const uint8_t *>(header), sizeof(util::ExecutableHeader)
	) == headerHash);
}

bool getShellInfo(ShellInfo &output) {
	for (auto &shell : _KONAMI_SHELLS) {
		if (!shell.validateHash())
			continue;

		__builtin_memcpy(&output, &shell, sizeof(ShellInfo));
		return true;
	}

	// If no official shell was found, fall back to searching the entire ROM for
	// a valid PS1 executable. Note that the executable has to be 32-byte
	// aligned for this to work.
	for (uintptr_t ptr = DEV2_BASE; ptr < (DEV2_BASE + 0x80000); ptr += 32) {
		auto header = reinterpret_cast<const util::ExecutableHeader *>(ptr);

		if (!header->validateMagic())
			continue;

		output.name         = header->getRegionString();
		output.bootFileName = nullptr;
#if 0
		output.headerHash   = util::hash(
			reinterpret_cast<const uint8_t *>(header),
			sizeof(util::ExecutableHeader)
		);
#endif
		output.header       = header;
		return true;
	}

	return false;
}

}
