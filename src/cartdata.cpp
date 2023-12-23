
#include <stddef.h>
#include <stdint.h>
#include "cart.hpp"
#include "cartdata.hpp"
#include "util.hpp"

namespace cart {

/* Cartridge data parsers */

// The system and install IDs are excluded from validation as they may not be
// always present.
// TODO/FIXME: this will create ambiguity between two of the basic formats...
static constexpr uint8_t _IDENTIFIER_FLAG_MASK = 
	DATA_HAS_TRACE_ID | DATA_HAS_CART_ID;

bool Parser::validate(void) {
	char region[8];

	if (getRegion(region) < REGION_MIN_LENGTH) {
		LOG("region is too short: %s", region);
		return false;
	}
	if (!isValidRegion(region)) {
		LOG("invalid region: %s", region);
		return false;
	}

#if 0
	auto id = getIdentifiers();

	if (id) {
		uint8_t flags = (id->getFlags() ^ flags) & _IDENTIFIER_FLAG_MASK;

		if (flags) {
			LOG("flags do not match, value=%02x", flags);
			return false;
		}
	}
#endif

	return true;
}

size_t SimpleParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, sizeof(header->region));
	output[sizeof(header->region)] = 0;

	return __builtin_strlen(output);
}

void SimpleParser::setRegion(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->region, input, sizeof(header->region));
}

void BasicParser::setCode(const char *input) {
	if (!(flags & DATA_HAS_CODE_PREFIX))
		return;

	auto header = _getHeader();

	header->region[2] = input[0];
	header->region[3] = input[1];
}

size_t BasicParser::getRegion(char *output) const {
	auto header = _getHeader();

	output[0] = header->region[0];
	output[1] = header->region[1];
	output[2] = 0;

	return 2;
}

void BasicParser::setRegion(const char *input) {
	auto header = _getHeader();

	header->region[0] = input[0];
	header->region[1] = input[1];
}

IdentifierSet *BasicParser::getIdentifiers(void) {
	return reinterpret_cast<IdentifierSet *>(&_dump.data[sizeof(BasicHeader)]);
}

void BasicParser::flush(void) {
	_getHeader()->updateChecksum(flags & DATA_CHECKSUM_INVERTED);
}

bool BasicParser::validate(void) {
	if (!Parser::validate())
		return false;

	return _getHeader()->validateChecksum(flags & DATA_CHECKSUM_INVERTED);
}

size_t ExtendedParser::getCode(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->code, sizeof(header->code) - 1);
	output[sizeof(header->code) - 1] = 0;

	if (flags & DATA_GX706_WORKAROUND)
		output[1] = 'X';

	return __builtin_strlen(output);
}

void ExtendedParser::setCode(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->code, input, sizeof(header->code));

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'E';
}

size_t ExtendedParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, sizeof(header->region));
	output[sizeof(header->region)] = 0;

	return __builtin_strlen(output);
}

void ExtendedParser::setRegion(const char *input) {
	auto header = _getHeader();

	__builtin_strncpy(header->region, input, sizeof(header->region));
}

uint16_t ExtendedParser::getYear(void) const {
	return _getHeader()->year;
}

void ExtendedParser::setYear(uint16_t value) {
	_getHeader()->year = value;
}

IdentifierSet *ExtendedParser::getIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<IdentifierSet *>(
		&_dump.data[sizeof(ExtendedHeader) + sizeof(PublicIdentifierSet)]
	);
}

PublicIdentifierSet *ExtendedParser::getPublicIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<PublicIdentifierSet *>(
		&_getPublicData()[sizeof(ExtendedHeader)]
	);
}

void ExtendedParser::flush(void) {
	// Copy over the private identifiers to the public data area. On X76F041
	// carts this area is in the last sector, while on ZS01 carts it is placed
	// in the first 32 bytes.
	auto pri = getIdentifiers();
	auto pub = getPublicIdentifiers();

	// The private installation ID seems to always go unused and zeroed out...
	//pub->installID.copyFrom(pri->installID.data);
	pub->systemID.copyFrom(pri->systemID.data);

	auto header = _getHeader();
	char code   = header->code[1];

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = 'X';

	header->updateChecksum(flags & DATA_CHECKSUM_INVERTED);

	if (flags & DATA_GX706_WORKAROUND)
		header->code[1] = code;
}

bool ExtendedParser::validate(void) {
	if (!Parser::validate())
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

/* Data format identification */

struct KnownFormat {
public:
	const char *name;
	FormatType format;
	uint8_t    flags;
};

static const KnownFormat _KNOWN_FORMATS[]{
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

bool isValidRegion(const char *region) {
	// Character 0:    region (A=Asia?, E=Europe, J=Japan, K=Korea, S=?, U=US)
	// Character 1:    type/variant (A-F=regular, R-W=e-Amusement, X-Z=?)
	// Characters 2-4: game revision (A-D or Z00-Z99, optional)
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

Parser *newCartParser(Dump &dump, FormatType formatType, uint8_t flags) {
	switch (formatType) {
		case SIMPLE:
			return new SimpleParser(dump, flags);

		case BASIC:
			return new BasicParser(dump, flags);

		case EXTENDED:
			return new ExtendedParser(dump, flags);

		default:
			//return new Parser(dump, flags);
			return nullptr;
	}
}

Parser *newCartParser(Dump &dump) {
	// Try all formats from the most complex one to the simplest.
	for (int i = util::countOf(_KNOWN_FORMATS) - 1; i >= 0; i--) {
		auto   &format = _KNOWN_FORMATS[i];
		Parser *parser = newCartParser(dump, format.format, format.flags);

		LOG("trying as %s", format.name);
		if (parser->validate())
			return parser;

		delete parser;
	}

	LOG("unrecognized data format");
	return nullptr;
}

/* Cartridge database */

const DBEntry *CartDB::lookup(const char *code, const char *region) const {
	// Perform a binary search. This assumes all entries in the DB are sorted by
	// their code and region.
	auto low  = reinterpret_cast<const DBEntry *>(ptr);
	auto high = &low[getNumEntries() - 1];

	while (low <= high) {
		auto entry = &low[(high - low) / 2];
		int  diff  = entry->compare(code, region);

		if (!diff) {
			LOG("%s %s found, entry=0x%08x", code, region, entry);
			return entry;
		}

		if (diff < 0)
			low = &entry[1];
		else
			high = &entry[-1];
	}

	LOG("%s %s not found", code, region);
	return nullptr;
}

}
