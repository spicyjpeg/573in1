
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
		LOG("checksum mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
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
		LOG("CRC mismatch, exp=0x%02x, got=0x%02x", value, data[7]);
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

/* Dump structure and utilities */

const ChipSize CHIP_SIZES[NUM_CHIP_TYPES]{
	{ .dataLength =   0, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 512, .publicDataOffset = 384, .publicDataLength = 128 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =   0 },
	{ .dataLength = 112, .publicDataOffset =   0, .publicDataLength =  32 }
};

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

	compLength = util::encodeBase45(&output[5], compressed, compLength);
	__builtin_memcpy(&output[0], "573::", 5);
	__builtin_memcpy(&output[compLength + 5], "::", 3);

	return compLength + 7;
}

}
