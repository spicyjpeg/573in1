

#include <stddef.h>
#include <stdint.h>
#include "asset.hpp"
#include "cart.hpp"
#include "cartdata.hpp"

namespace cart {

/* Cartridge data parsers */

bool Parser::validate(void) {
	char region[8];

	if (getRegion(region) < REGION_MIN_LENGTH)
		return false;
	if (!isValidRegion(region))
		return false;

	auto id = getIdentifiers();

	if (id) {
		// The system ID is excluded from validation as it is going to be
		// missing if the game hasn't yet been installed.
		uint8_t idFlags = flags & (
			DATA_HAS_TRACE_ID | DATA_HAS_CART_ID | DATA_HAS_INSTALL_ID
		);

		if (id->getFlags() != idFlags)
			return false;
	}

	return true;
}

size_t SimpleParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, 4);
	output[4] = 0;

	return __builtin_strlen(output);
}

size_t BasicParser::getRegion(char *output) const {
	auto header = _getHeader();

	output[0] = header->region[0];
	output[1] = header->region[1];
	output[2] = 0;

	return 2;
}

IdentifierSet *BasicParser::getIdentifiers(void) {
	return reinterpret_cast<IdentifierSet *>(&_dump.data[sizeof(BasicHeader)]);
}

bool BasicParser::validate(void) {
	return (Parser::validate() && _getHeader()->validateChecksum());
}

size_t ExtendedParser::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, 4);
	output[4] = 0;

	return __builtin_strlen(output);
}

IdentifierSet *ExtendedParser::getIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<IdentifierSet *>(
		&_dump.data[sizeof(ExtendedHeader) + sizeof(PublicIdentifierSet)]
	);
}

void ExtendedParser::flush(void) {
	// Copy over the private identifiers to the public data area. On X76F041
	// carts this area is in the last sector, while on ZS01 carts it is placed
	// in the first 32 bytes.
	auto pri = getIdentifiers();
	auto pub = reinterpret_cast<PublicIdentifierSet *>(
		&_getPublicData()[sizeof(ExtendedHeader)]
	);

	pub->traceID.copyFrom(pri->traceID.data);
	pub->cartID.copyFrom(pri->cartID.data);
}

bool ExtendedParser::validate(void) {
	return (Parser::validate() && _getHeader()->validateChecksum());
}

/* Data format identification */

struct KnownFormat {
public:
	const char *name;
	FormatType format;
	uint8_t    flags;
};

static constexpr int _NUM_KNOWN_FORMATS = 8;

static const KnownFormat _KNOWN_FORMATS[_NUM_KNOWN_FORMATS]{
	{
		// Used by GCB48 (and possibly other games?)
		.name   = "region string only",
		.format = SIMPLE,
		.flags  = DATA_HAS_PUBLIC_SECTION
	}, {
		.name   = "basic with no IDs",
		.format = BASIC,
		.flags  = 0
	}, {
		.name   = "basic with TID",
		.format = BASIC,
		.flags  = DATA_HAS_TRACE_ID
	}, {
		.name   = "basic with TID, SID",
		.format = BASIC,
		.flags  = DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
	}, {
		.name   = "basic with code prefix, TID, SID",
		.format = BASIC,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
	}, {
		// Used by most pre-ZS01 Bemani games
		.name   = "basic with code prefix, all IDs",
		.format = BASIC,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
			| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID
	}, {
		// Used by early (pre-digital-I/O) Bemani games
		.name   = "extended with no IDs",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX
	}, {
		// Used by GE936/GK936 and all ZS01 Bemani games
		.name   = "extended with all IDs",
		.format = EXTENDED,
		.flags  = DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
			| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID | DATA_HAS_PUBLIC_SECTION
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

Parser *newCartParser(Dump &dump, FormatType formatType, uint8_t flags) {
	switch (formatType) {
		case SIMPLE:
			return new SimpleParser(dump, flags);

		case BASIC:
			return new BasicParser(dump, flags);

		case EXTENDED:
			return new ExtendedParser(dump, flags);

		default:
			return new Parser(dump, flags);
	}
}

Parser *newCartParser(Dump &dump) {
	// Try all formats from the most complex one to the simplest.
	for (int i = _NUM_KNOWN_FORMATS - 1; i >= 0; i++) {
		auto   &format = _KNOWN_FORMATS[i];
		Parser *parser = newCartParser(dump, format.format, format.flags);

		if (parser->validate()) {
			LOG("found known data format");
			LOG("%s, index=%d", format.name, i);
			return parser;
		}

		delete parser;
	}

	LOG("unrecognized data format");
	return nullptr;
}

/* Cartridge database */

const DBEntry *CartDB::lookupEntry(const char *code, const char *region) const {
	// Perform a binary search. This assumes all entries in the DB are sorted by
	// their code and region.
	auto offset = reinterpret_cast<const DBEntry *>(ptr);

	for (size_t step = getNumEntries() / 2; step; step /= 2) {
		auto entry = &offset[step];
		int  diff  = entry->compare(code, region);

		if (!diff)
			return entry;
		else if (diff > 0) // TODO: could be diff < 0
			offset = entry;
	}

	LOG("%s %s not found", code, region);
	return nullptr;
}

}
