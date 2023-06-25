
#include <stddef.h>
#include <stdint.h>
#include "vendor/miniz.h"
#include "cart.hpp"
#include "util.hpp"

namespace cart {

/* Common data structures */

void Identifier::updateChecksum(void) {
	data[7] = (util::sum(data, 7) & 0xff) ^ 0xff;
}

bool Identifier::validateChecksum(void) const {
	uint8_t value = (util::sum(data, 7) & 0xff) ^ 0xff;

	if (value != data[7]) {
		LOG("mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
		return false;
	}

	return true;
}

void Identifier::updateDSCRC(void) {
	data[7] = util::dsCRC8(data, 7);
}

bool Identifier::validateDSCRC(void) const {
	uint8_t value = util::dsCRC8(data, 7);

	if (value != data[7]) {
		LOG("mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
		return false;
	}

	return true;
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

void IdentifierSet::updateTraceID(TraceIDType type, int param) {
	traceID.clear();

	uint8_t  *input   = &cartID.data[1];
	uint16_t checksum = 0;

	if (type == TID_81) {
		// This format seems to be an arbitrary unique identifier not tied to
		// anything in particular (perhaps RTC RAM?), ignored by the game.
		traceID.data[0] = 0x81;
		traceID.data[2] = 5;
		traceID.data[5] = 7;
		traceID.data[6] = 3;

		LOG("prefix=0x81");
		goto _done;
	}

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
	} else if (type == TID_82_LITTLE_ENDIAN) {
		traceID.data[1] = checksum & 0xff;
		traceID.data[2] = checksum >> 8;
	}

	LOG("prefix=0x82, checksum=%04x", checksum);

_done:
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
	installID.clear();

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
		LOG("mismatch, exp=0x%02x, got=0x%02x", value, checksum);
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
		LOG("mismatch, exp=0x%04x, got=0x%04x", value, checksum);
		return false;
	}

	return true;
}

/* Dump structure and utilities */

const ChipSize CHIP_SIZES[NUM_CHIP_TYPES]{
	{ .dataLength =   0, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 512, .publicDataOffset = 384, .publicDataLength = 128 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =  32 }
};

bool Dump::isPublicDataEmpty(void) const {
	if (!(flags & DUMP_PUBLIC_DATA_OK))
		return false;

	auto size = getChipSize();
	auto sum  = util::sum(&data[size.publicDataOffset], size.publicDataLength);

	return (!sum || (sum == (0xff * size.publicDataLength)));
}

bool Dump::isDataEmpty(void) const {
	if (!(flags & DUMP_PUBLIC_DATA_OK) || !(flags & DUMP_PRIVATE_DATA_OK))
		return false;

	size_t length = getChipSize().dataLength;
	auto   sum    = util::sum(data, length);

	return (!sum || (sum == (0xff * length)));
}

bool Dump::isReadableDataEmpty(void) const {
	// This is more or less a hack. The "right" way to tell if this chip has any
	// public data would be to use getChipSize().publicDataLength, but many
	// X76F041 carts don't actually have a public data area.
	if (chipType == ZS01)
		return isPublicDataEmpty();
	else
		return isDataEmpty();
}

size_t Dump::toQRString(char *output) const {
	uint8_t compressed[MAX_QR_STRING_LENGTH];
	size_t  uncompLength = getDumpLength();
	size_t  compLength   = MAX_QR_STRING_LENGTH;

	int error = mz_compress2(
		compressed, reinterpret_cast<mz_ulong *>(&compLength),
		reinterpret_cast<const uint8_t *>(this), uncompLength,
		MZ_BEST_COMPRESSION
	);

	if (error != MZ_OK) {
		LOG("compression error, code=%d", error);
		return 0;
	}
	LOG(
		"dump compressed, size=%d, ratio=%d%%", compLength,
		compLength * 100 / uncompLength
	);

	compLength = util::encodeBase41(&output[5], compressed, compLength);
	__builtin_memcpy(&output[0], "573::", 5);
	__builtin_memcpy(&output[compLength + 5], "::", 3);

	return compLength + 7;
}

}
