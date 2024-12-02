/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartdata.hpp"

namespace cart {

/* Common data structures */

uint8_t IdentifierSet::getFlags(void) const {
	uint8_t flags = 0;

	if (!traceID.isEmpty())
		flags |= DATA_HAS_TRACE_ID;
	if (!cartID.isEmpty())
		flags |= DATA_HAS_CART_ID;
	if (!installID.isEmpty())
		flags |= DATA_HAS_INSTALL_ID;
	if (!systemID.isEmpty())
		flags |= DATA_HAS_SYSTEM_ID;

	return flags;
}

void IdentifierSet::setInstallID(uint8_t prefix) {
	util::clear(installID);

	installID.data[0] = prefix;
	installID.updateChecksum();
}

void IdentifierSet::updateTraceID(
	TraceIDType type, int param, const Identifier *_cartID
) {
	util::clear(traceID);

	const uint8_t *input   = _cartID ? &_cartID->data[1] : &cartID.data[1];
	uint16_t      checksum = 0;

	switch (type) {
		case TID_NONE:
			return;

		case TID_81:
			// This format seems to be an arbitrary unique identifier not tied
			// to anything in particular (maybe RTC RAM?), ignored by the game.
			traceID.data[0] = 0x81;
			traceID.data[2] = 5;
			traceID.data[5] = 7;
			traceID.data[6] = 3;

			LOG_CART_DATA("prefix=0x81");
			break;

		case TID_82_BIG_ENDIAN:
		case TID_82_LITTLE_ENDIAN:
			for (size_t i = 0; i < ((sizeof(cartID.data) - 2) * 8); i += 8) {
				uint8_t value = *(input++);

				for (size_t j = i; j < (i + 8); j++, value >>= 1) {
					if (value & 1)
						checksum ^= 1 << (j % param);
				}
			}

			traceID.data[0] = 0x82;
			if (type == TID_82_BIG_ENDIAN) {
				traceID.data[1] = checksum >> 8;
				traceID.data[2] = checksum & 0xff;
			} else {
				traceID.data[1] = checksum & 0xff;
				traceID.data[2] = checksum >> 8;
			}

			LOG_CART_DATA("prefix=0x82, checksum=0x%04x", checksum);
			break;
	}

	traceID.updateChecksum();
}

uint8_t PublicIdentifierSet::getFlags(void) const {
	uint8_t flags = 0;

	if (!installID.isEmpty())
		flags |= DATA_HAS_INSTALL_ID;
	if (!systemID.isEmpty())
		flags |= DATA_HAS_SYSTEM_ID;

	return flags;
}

void PublicIdentifierSet::setInstallID(uint8_t prefix) {
	util::clear(installID);

	installID.data[0] = prefix;
	installID.updateChecksum();
}

void BasicHeader::updateChecksum(bool invert) {
	auto    value = util::sum(reinterpret_cast<const uint8_t *>(this), 4);
	uint8_t mask  = invert ? 0xff : 0x00;

	checksum = uint8_t((value & 0xff) ^ mask);
}

bool BasicHeader::validateChecksum(bool invert) const {
	auto    value = util::sum(reinterpret_cast<const uint8_t *>(this), 4);
	uint8_t mask  = invert ? 0xff : 0x00;

	value = (value & 0xff) ^ mask;
	if (value != checksum) {
		LOG_CART_DATA("mismatch, exp=0x%02x, got=0x%02x", value, checksum);
		return false;
	}

	return true;
}

void ExtendedHeader::updateChecksum(bool invert) {
	auto     value = util::sum(reinterpret_cast<const uint16_t *>(this), 7);
	uint16_t mask  = invert ? 0xffff : 0x0000;

	checksum = uint16_t((value & 0xffff) ^ mask);
}

bool ExtendedHeader::validateChecksum(bool invert) const {
	auto     value = util::sum(reinterpret_cast<const uint16_t *>(this), 7);
	uint16_t mask  = invert ? 0xffff : 0x0000;

	value = (value & 0xffff) ^ mask;
	if (value != checksum) {
		LOG_CART_DATA("mismatch, exp=0x%04x, got=0x%04x", value, checksum);
		return false;
	}

	return true;
}

/* Cartridge data parsers/writers */

// The system and install IDs are excluded from validation as they may not be
// always present.
// TODO/FIXME: this will create ambiguity between two of the basic formats...
static constexpr uint8_t _IDENTIFIER_FLAG_MASK =
	DATA_HAS_TRACE_ID | DATA_HAS_CART_ID;

bool CartParser::validate(void) {
	char region[8];

	if (getRegion(region) < REGION_MIN_LENGTH) {
		LOG_CART_DATA("region is too short: %s", region);
		return false;
	}
	if (!isValidRegion(region)) {
		LOG_CART_DATA("invalid region: %s", region);
		return false;
	}

#if 0
	auto id = getIdentifiers();

	if (id) {
		uint8_t flags = (id->getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK;

		if (flags) {
			LOG_CART_DATA("flags do not match, value=%02x", flags);
			return false;
		}
	}
#endif

	return true;
}

size_t SimpleCartParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, sizeof(header->region));
	output[sizeof(header->region)] = 0;

	return __builtin_strlen(output);
}

void SimpleCartParser::setRegion(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->region, input, sizeof(header->region));
}

void BasicCartParser::setCode(const char *input) {
	if (!(flags & DATA_HAS_CODE_PREFIX))
		return;

	auto header = _getHeader();

	header->codePrefix[0] = input[0];
	header->codePrefix[1] = input[1];
}

size_t BasicCartParser::getRegion(char *output) const {
	auto header = _getHeader();

	output[0] = header->region[0];
	output[1] = header->region[1];
	output[2] = 0;

	return 2;
}

void BasicCartParser::setRegion(const char *input) {
	auto header = _getHeader();

	header->region[0] = input[0];
	header->region[1] = input[1];
}

IdentifierSet *BasicCartParser::getIdentifiers(void) {
	return reinterpret_cast<IdentifierSet *>(&_dump.data[sizeof(BasicHeader)]);
}

void BasicCartParser::flush(void) {
	_getHeader()->updateChecksum(flags & DATA_CHECKSUM_INVERTED);
}

bool BasicCartParser::validate(void) {
	if (!CartParser::validate())
		return false;

	return _getHeader()->validateChecksum(flags & DATA_CHECKSUM_INVERTED);
}

size_t ExtendedCartParser::getCode(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->code, sizeof(header->code) - 1);
	output[sizeof(header->code) - 1] = 0;

	if (flags & DATA_GX706_WORKAROUND)
		output[1] = 'X';

	return __builtin_strlen(output);
}

void ExtendedCartParser::setCode(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->code, input, sizeof(header->code));

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'E';
}

size_t ExtendedCartParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, sizeof(header->region));
	output[sizeof(header->region)] = 0;

	return __builtin_strlen(output);
}

void ExtendedCartParser::setRegion(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->region, input, sizeof(header->region));
}

uint16_t ExtendedCartParser::getYear(void) const {
	return _getHeader()->year;
}

void ExtendedCartParser::setYear(uint16_t value) {
	_getHeader()->year = value;
}

IdentifierSet *ExtendedCartParser::getIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<IdentifierSet *>(
		&_dump.data[sizeof(ExtendedHeader) + sizeof(PublicIdentifierSet)]
	);
}

PublicIdentifierSet *ExtendedCartParser::getPublicIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<PublicIdentifierSet *>(
		&_getPublicData()[sizeof(ExtendedHeader)]
	);
}

void ExtendedCartParser::flush(void) {
	// Copy over the private identifiers to the public data area. On X76F041
	// carts this area is in the last sector, while on ZS01 carts it is placed
	// in the first 32 bytes.
	auto pri = getIdentifiers();
	auto pub = getPublicIdentifiers();

	// The private installation ID seems to always go unused and zeroed out...
#if 0
	pub->installID.copyFrom(pri->installID.data);
#endif
	pub->systemID.copyFrom(pri->systemID.data);

	auto header = _getHeader();
	char code   = header->code[1];

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'X';

	header->updateChecksum(flags & DATA_CHECKSUM_INVERTED);

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = code;
}

bool ExtendedCartParser::validate(void) {
	if (!CartParser::validate())
		return false;

	auto header = _getHeader();
	char code   = header->code[1];

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'X';

	bool valid = header->validateChecksum(flags & DATA_CHECKSUM_INVERTED);

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = code;

	return valid;
}

/* Flash and RTC header parsers/writers */

// Used alongside the system ID and the header itself to calculate the MD5 used
// as a header signature. Seems to be the same in all games.
static const uint8_t _SIGNATURE_SALT[]{
	0xc1, 0xa2, 0x03, 0xd6, 0xab, 0x70, 0x85, 0x5e
};

bool ROMHeaderParser::validate(void) {
	char region[8];

	if (getRegion(region) < REGION_MIN_LENGTH) {
		LOG_CART_DATA("region is too short: %s", region);
		return false;
	}
	if (!isValidRegion(region)) {
		LOG_CART_DATA("invalid region: %s", region);
		return false;
	}

	return true;
}

void ExtendedROMHeaderParser::_calculateSignature(uint8_t *output) const {
	util::MD5 md5;
	uint8_t   buffer[16];

	md5.update(_dump.systemID.data, sizeof(_dump.systemID.data));
	md5.update(
		reinterpret_cast<const uint8_t *>(_getHeader()), sizeof(ExtendedHeader)
	);
	md5.update(_SIGNATURE_SALT, sizeof(_SIGNATURE_SALT));
	md5.digest(buffer);

	for (int i = 0; i < 8; i++)
		output[i] = buffer[i] ^ buffer[i + 8];
}

size_t ExtendedROMHeaderParser::getCode(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->code, sizeof(header->code) - 1);
	output[sizeof(header->code) - 1] = 0;

	if (flags & DATA_GX706_WORKAROUND)
		output[1] = 'X';

	return __builtin_strlen(output);
}

void ExtendedROMHeaderParser::setCode(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->code, input, sizeof(header->code));

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'E';
}

size_t ExtendedROMHeaderParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, sizeof(header->region));
	output[sizeof(header->region)] = 0;

	return __builtin_strlen(output);
}

void ExtendedROMHeaderParser::setRegion(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->region, input, sizeof(header->region));
}

uint16_t ExtendedROMHeaderParser::getYear(void) const {
	return _getHeader()->year;
}

void ExtendedROMHeaderParser::setYear(uint16_t value) {
	_getHeader()->year = value;
}

void ExtendedROMHeaderParser::flush(void) {
	auto header = _getHeader();
	char code   = header->code[1];

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'X';

	header->updateChecksum(flags & DATA_CHECKSUM_INVERTED);

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = code;
	if (flags & DATA_HAS_SYSTEM_ID)
		_calculateSignature(_getSignature());
}

bool ExtendedROMHeaderParser::validate(void) {
	if (!ROMHeaderParser::validate())
		return false;

	auto header = _getHeader();
	char code   = header->code[1];

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'X';

	bool valid = header->validateChecksum(flags & DATA_CHECKSUM_INVERTED);

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = code;

	if (!valid)
		return false;

	if (flags & DATA_HAS_SYSTEM_ID) {
		uint8_t signature[8];

		_calculateSignature(signature);

		if (__builtin_memcmp(signature, _getSignature(), sizeof(signature))) {
			LOG_CART_DATA("signature mismatch");
			return false;
		}
	}

	return true;
}

/* Data format identification */

struct KnownFormat {
public:
	const char *name;
	FormatType format;
	uint8_t    flags;
};

static const KnownFormat _KNOWN_CART_FORMATS[]{
	{
		// Used by GCB48 (and possibly other games?)
		.name   = "region only",
		.format = SIMPLE,
		.flags  = DATA_HAS_PUBLIC_SECTION
	}, {
		.name   = "basic (no IDs)",
		.format = BASIC,
		.flags  = DATA_CHECKSUM_INVERTED
	}, {
		.name   = "basic + TID",
		.format = BASIC,
		.flags  = DATA_HAS_TRACE_ID | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "basic + SID",
		.format = BASIC,
		.flags  = DATA_HAS_CART_ID | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "basic + TID, SID",
		.format = BASIC,
		.flags  = DATA_HAS_TRACE_ID | DATA_HAS_CART_ID | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "basic + prefix, TID, SID",
		.format = BASIC,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
			| DATA_CHECKSUM_INVERTED
	}, {
		// Used by most pre-ZS01 Bemani games
		.name   = "basic + prefix, all IDs",
		.format = BASIC,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
			| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "extended (no IDs)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "extended (no IDs, alt)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX
	}, {
		// Used by GX706
		.name   = "extended (no IDs, GX706)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_GX706_WORKAROUND
	}, {
		// Used by GE936/GK936 and all ZS01 Bemani games
		.name   = "extended + all IDs",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
			| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID | DATA_HAS_PUBLIC_SECTION
			| DATA_CHECKSUM_INVERTED
	}
};

static const KnownFormat _KNOWN_ROM_HEADER_FORMATS[]{
	{
		.name   = "extended (no MD5)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_CHECKSUM_INVERTED
	}, {
		.name   = "extended (no MD5, alt)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX
	}, {
		// Used by GX706
		.name   = "extended (no MD5, GX706)",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_GX706_WORKAROUND
	}, {
		.name   = "extended + MD5",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_SYSTEM_ID
			| DATA_CHECKSUM_INVERTED
	}
};

bool isValidRegion(const char *region) {
	// Character 0:    region code
	//                 (A=Asia, E=Europe, J=Japan, K=Korea, S=Singapore?, U=US)
	// Character 1:    major version code (A-F=regular, R-W=e-Amusement, X-Z=?)
	// Characters 2-4: minor version code (A-D or Z00-Z99, optional)
	if (!region[0] || !__builtin_strchr("AEJKSU", region[0]))
		return false;
	if (!region[1] || !__builtin_strchr("ABCDEFRSTUVWXYZ", region[1]))
		return false;

	if (region[2]) {
		if (!__builtin_strchr("ABCDZ", region[2]))
			return false;

		if (region[2] == 'Z') {
			if (!__builtin_isdigit(region[3]) || !__builtin_isdigit(region[4]))
				return false;

			region += 2;
		}
		if (region[3])
			return false;
	}

	return true;
}

bool isValidUpgradeRegion(const char *region) {
	if (!region[0] || !__builtin_strchr("aejksu", region[0]))
		return false;
	if (!region[1] || !__builtin_strchr("abcdefrstuvwxyz", region[1]))
		return false;

	if (region[2]) {
		if (!__builtin_strchr("abcdz", region[2]))
			return false;

		if (region[2] == 'z') {
			if (!__builtin_isdigit(region[3]) || !__builtin_isdigit(region[4]))
				return false;

			region += 2;
		}
		if (region[3])
			return false;
	}

	return true;
}

CartParser *newCartParser(
	CartDump &dump, FormatType formatType, uint8_t flags
) {
	switch (formatType) {
		case SIMPLE:
			return new SimpleCartParser(dump, flags);

		case BASIC:
			return new BasicCartParser(dump, flags);

		case EXTENDED:
			return new ExtendedCartParser(dump, flags);

		default:
			return nullptr;
	}
}

CartParser *newCartParser(CartDump &dump) {
	// Try all formats from the most complex one to the simplest.
#if 0
	for (auto &format : _KNOWN_CART_FORMATS) {
#else
	for (int i = util::countOf(_KNOWN_CART_FORMATS) - 1; i >= 0; i--) {
		auto &format = _KNOWN_CART_FORMATS[i];
#endif
		auto parser  = newCartParser(dump, format.format, format.flags);

		LOG_CART_DATA("trying as %s", format.name);
		if (parser->validate())
			return parser;

		delete parser;
	}

	LOG_CART_DATA("unrecognized data format");
	return nullptr;
}

ROMHeaderParser *newROMHeaderParser(
	ROMHeaderDump &dump, FormatType formatType, uint8_t flags
) {
	switch (formatType) {
		case EXTENDED:
			return new ExtendedROMHeaderParser(dump, flags);

		default:
			return nullptr;
	}
}

ROMHeaderParser *newROMHeaderParser(ROMHeaderDump &dump) {
#if 0
	for (auto &format : _KNOWN_ROM_HEADER_FORMATS) {
#else
	for (int i = util::countOf(_KNOWN_ROM_HEADER_FORMATS) - 1; i >= 0; i--) {
		auto &format = _KNOWN_ROM_HEADER_FORMATS[i];
#endif
		auto parser  = newROMHeaderParser(dump, format.format, format.flags);

		LOG_CART_DATA("trying as %s", format.name);
		if (parser->validate())
			return parser;

		delete parser;
	}

	LOG_CART_DATA("unrecognized data format");
	return nullptr;
}

/* Cartridge and flash header database */

template<typename T> const T *DB<T>::get(int index) const {
	auto entries = reinterpret_cast<const T *>(ptr);

	if (!entries)
		return nullptr;
	if ((index * sizeof(T)) >= length)
		return nullptr;

	return &entries[index];
}

template<typename T> const T *DB<T>::lookup(
	const char *code, const char *region
) const {
	// Perform a binary search. This assumes all entries in the DB are sorted by
	// their code and region.
	auto low  = reinterpret_cast<const T *>(ptr);
	auto high = &low[getNumEntries() - 1];

	while (low <= high) {
		auto entry = &low[(high - low) / 2];
		int  diff  = entry->compare(code, region);

		if (!diff) {
			LOG_CART_DATA("%s %s found", code, region);
			return entry;
		}

		if (diff < 0)
			low = &entry[1];
		else
			high = &entry[-1];
	}

	LOG_CART_DATA("%s %s not found", code, region);
	return nullptr;
}

template class DB<CartDBEntry>;
template class DB<ROMHeaderDBEntry>;

}
