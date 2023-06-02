

#include <stddef.h>
#include <stdint.h>
#include "asset.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"

namespace cart {

/* Utilities */

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

/* Common data structures */

void BasicHeader::updateChecksum(void) {
	auto value = util::sum(reinterpret_cast<const uint8_t *>(this), 4);

	checksum = uint8_t((value & 0xff) ^ 0xff);
}

bool BasicHeader::validateChecksum(void) const {
	auto value = util::sum(reinterpret_cast<const uint8_t *>(this), 4);

	return (checksum == ((value & 0xff) ^ 0xff));
}

void ExtendedHeader::updateChecksum(void) {
	auto value = util::sum(reinterpret_cast<const uint16_t *>(this), 14);

	checksum = uint16_t(value & 0xffff);
}

bool ExtendedHeader::validateChecksum(void) const {
	auto value = util::sum(reinterpret_cast<const uint16_t *>(this), 14);

	return (checksum == (value & 0xffff));
}

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
	installID.clear();

	installID.data[0] = prefix;
	installID.updateChecksum();
}

void IdentifierSet::updateTraceID(uint8_t prefix, int param) {
	traceID.clear();

	uint8_t  *input   = &cartID.data[1];
	uint16_t checksum = 0;

	switch (prefix) {
		case 0x81:
			// TODO: reverse engineer this TID format
			traceID.data[2] = 0;
			traceID.data[5] = 0;
			traceID.data[6] = 0;

			LOG("prefix=0x81");
			break;

		case 0x82:
			for (size_t i = 0; i < (sizeof(cartID.data) - 2); i++) {
				uint8_t value = *(input++);

				for (int j = 0; j < 8; j++, value >>= 1) {
					if (value % 2)
						checksum ^= 1 << (i % param);
				}
			}

			traceID.data[1] = checksum >> 8;
			traceID.data[2] = checksum & 0xff;

			LOG("prefix=0x82, checksum=%04x", checksum);
			break;

		default:
			LOG("unknown prefix 0x%02x", prefix);
	}

	traceID.data[0] = prefix;
	traceID.updateChecksum();
}

/* Data formats */

bool CartData::validate(void) {
	char region[8];

	if (getRegion(region) < REGION_MIN_LENGTH)
		return false;
	if (!isValidRegion(region))
		return false;

	auto id = getIdentifiers();

	if (id) {
		uint8_t idFlags = flags & (
			DATA_HAS_TRACE_ID | DATA_HAS_CART_ID | DATA_HAS_INSTALL_ID |
			DATA_HAS_SYSTEM_ID
		);

		if (id->getFlags() != idFlags)
			return false;
	}

	return true;
}

size_t SimpleCartData::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, 4);
	output[4] = 0;

	return __builtin_strlen(output);
}

size_t BasicCartData::getRegion(char *output) const {
	auto header = _getHeader();

	output[0] = header->region[0];
	output[1] = header->region[1];
	output[2] = 0;

	return 2;
}

IdentifierSet *BasicCartData::getIdentifiers(void) {
	return reinterpret_cast<IdentifierSet *>(&_dump.data[sizeof(BasicHeader)]);
}

bool BasicCartData::validate(void) {
	return (CartData::validate() && _getHeader()->validateChecksum());
}

size_t ExtendedCartData::getRegion(char *output) const {
	auto header = _getHeader();

	__builtin_memcpy(output, header->region, 4);
	output[4] = 0;

	return __builtin_strlen(output);
}

IdentifierSet *ExtendedCartData::getIdentifiers(void) {
	if (!(flags & DATA_HAS_PUBLIC_SECTION))
		return nullptr;

	return reinterpret_cast<IdentifierSet *>(
		&_dump.data[sizeof(ExtendedHeader) + sizeof(PublicIdentifierSet)]
	);
}

void ExtendedCartData::flush(void) {
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

bool ExtendedCartData::validate(void) {
	return (CartData::validate() && _getHeader()->validateChecksum());
}

/* Data format identification */

struct KnownFormat {
public:
	FormatType formatType;
	uint8_t    flags;
};

static constexpr int _NUM_KNOWN_FORMATS = 8;

// More variants may exist.
static const KnownFormat _KNOWN_FORMATS[_NUM_KNOWN_FORMATS]{
	{ SIMPLE,   DATA_HAS_PUBLIC_SECTION },
	{ BASIC,    0 },
	{ BASIC,    DATA_HAS_TRACE_ID },
	{ BASIC,    DATA_HAS_TRACE_ID | DATA_HAS_CART_ID },
	{ BASIC,    DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID },
	{ BASIC,    DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
		| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID },
	{ EXTENDED, 0 },
	{ EXTENDED, DATA_HAS_CODE_PREFIX | DATA_HAS_TRACE_ID | DATA_HAS_CART_ID
		| DATA_HAS_INSTALL_ID | DATA_HAS_SYSTEM_ID | DATA_HAS_PUBLIC_SECTION },
};

CartData *createCartData(Dump &dump, FormatType formatType, uint8_t flags) {
	switch (formatType) {
		case SIMPLE:
			return new SimpleCartData(dump, flags);

		case BASIC:
			return new BasicCartData(dump, flags);

		case EXTENDED:
			return new ExtendedCartData(dump, flags);

		default:
			return new CartData(dump, flags);
	}
}

CartData *createCartData(Dump &dump) {
	for (int i = 0; i < _NUM_KNOWN_FORMATS; i++) {
		auto     &fmt  = _KNOWN_FORMATS[i];
		CartData *data = createCartData(dump, fmt.formatType, fmt.flags);

		if (data->validate()) {
			LOG("found known format, index=%d", i);
			return data;
		}

		delete data;
	}

	LOG("unrecognized dump format");
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
