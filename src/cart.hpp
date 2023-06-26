
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "util.hpp"

namespace cart {

/* Definitions */

enum ChipType : uint8_t {
	NONE    = 0,
	X76F041 = 1,
	X76F100 = 2,
	ZS01    = 3
};

enum FormatType : uint8_t {
	BLANK    = 0,
	SIMPLE   = 1,
	BASIC    = 2,
	EXTENDED = 3
};

enum TraceIDType : uint8_t {
	TID_NONE             = 0,
	TID_81               = 1,
	TID_82_BIG_ENDIAN    = 2,
	TID_82_LITTLE_ENDIAN = 3
};

enum DumpFlag : uint8_t {
	DUMP_HAS_SYSTEM_ID   = 1 << 0,
	DUMP_HAS_CART_ID     = 1 << 1,
	DUMP_CONFIG_OK       = 1 << 2,
	DUMP_SYSTEM_ID_OK    = 1 << 3,
	DUMP_CART_ID_OK      = 1 << 4,
	DUMP_ZS_ID_OK        = 1 << 5,
	DUMP_PUBLIC_DATA_OK  = 1 << 6,
	DUMP_PRIVATE_DATA_OK = 1 << 7
};

// |                         | Simple    | Basic    | Extended  |
// | :---------------------- | :-------- | :------- | :-------- |
// | DATA_HAS_CODE_PREFIX    |           | Optional | Mandatory |
// | DATA_HAS_*_ID           |           | Optional | Optional  |
// | DATA_HAS_PUBLIC_SECTION | Mandatory |          | Optional  |
// | DATA_GX706_WORKAROUND   |           |          | Optional  |

enum DataFlag : uint8_t {
	DATA_HAS_CODE_PREFIX     = 1 << 0,
	DATA_HAS_TRACE_ID        = 1 << 1,
	DATA_HAS_CART_ID         = 1 << 2,
	DATA_HAS_INSTALL_ID      = 1 << 3,
	DATA_HAS_SYSTEM_ID       = 1 << 4,
	DATA_HAS_PUBLIC_SECTION  = 1 << 5,
	DATA_CHECKSUM_INVERTED   = 1 << 6,
	DATA_GX706_WORKAROUND    = 1 << 7
};

static constexpr int    NUM_CHIP_TYPES       = 4;
static constexpr size_t MAX_QR_STRING_LENGTH = 0x600;

/* Common data structures */

class [[gnu::packed]] Identifier {
public:
	uint8_t data[8];

	inline void copyFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, sizeof(data));
	}
	inline void copyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, sizeof(data));
	}
	inline void clear(void) {
		__builtin_memset(data, 0, sizeof(data));
	}
	inline bool isEmpty(void) const {
		return (util::sum(data, sizeof(data)) == 0);
	}

	inline size_t toString(char *output) const {
		return util::hexToString(output, data, sizeof(data), '-');
	}
	inline size_t toSerialNumber(char *output) const {
		return util::serialNumberToString(output, &data[1]);
	}

	void updateChecksum(void);
	bool validateChecksum(void) const;
	void updateDSCRC(void);
	bool validateDSCRC(void) const;
};

class [[gnu::packed]] IdentifierSet {
public:
	Identifier traceID, cartID, installID, systemID; // aka TID, SID, MID, XID

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(IdentifierSet));
	}

	uint8_t getFlags(void) const;
	void setInstallID(uint8_t prefix);
	void updateTraceID(TraceIDType type, int param);
};

class [[gnu::packed]] PublicIdentifierSet {
public:
	Identifier installID, systemID; // aka MID, XID

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(PublicIdentifierSet));
	}

	uint8_t getFlags(void) const;
	void setInstallID(uint8_t prefix);
};

class [[gnu::packed]] SimpleHeader {
public:
	char region[4];
};

class [[gnu::packed]] BasicHeader {
public:
	char    region[2], codePrefix[2];
	uint8_t checksum, _pad[3];

	void updateChecksum(bool invert = false);
	bool validateChecksum(bool invert = false) const;
};

class [[gnu::packed]] ExtendedHeader {
public:
	char     code[8];
	uint16_t year;      // BCD, can be little endian, big endian or zero
	char     region[4];
	uint16_t checksum;

	void updateChecksum(bool invert = false);
	bool validateChecksum(bool invert = false) const;
};

/* Cartridge dump structure */

struct ChipSize {
public:
	size_t dataLength, publicDataOffset, publicDataLength;
};

extern const ChipSize CHIP_SIZES[NUM_CHIP_TYPES];

class [[gnu::packed]] Dump {
public:
	ChipType chipType;
	uint8_t  flags;
	uint8_t  _reserved[2];

	Identifier systemID, cartID, zsID;

	uint8_t dataKey[8], config[8];
	uint8_t data[512];

	inline const ChipSize &getChipSize(void) const {
		return CHIP_SIZES[chipType];
	}
	inline size_t getDumpLength(void) const {
		return (sizeof(Dump) - sizeof(data)) + getChipSize().dataLength;
	}
	inline void clearIdentifiers(void) {
		systemID.clear();
		cartID.clear();
		zsID.clear();
	}
	inline void copyDataFrom(const uint8_t *source) {
		__builtin_memcpy(data, source, getChipSize().dataLength);
	}
	inline void copyDataTo(uint8_t *dest) const {
		__builtin_memcpy(dest, data, getChipSize().dataLength);
	}
	inline void clearData(void) {
		__builtin_memset(data, 0, sizeof(data));
	}
	inline void copyKeyFrom(const uint8_t *source) {
		__builtin_memcpy(dataKey, source, sizeof(dataKey));
	}
	inline void copyKeyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, dataKey, sizeof(dataKey));
	}
	inline void clearKey(void) {
		__builtin_memset(dataKey, 0, sizeof(dataKey));
	}
	inline void copyConfigFrom(const uint8_t *source) {
		__builtin_memcpy(config, source, sizeof(config));
	}
	inline void copyConfigTo(uint8_t *dest) const {
		__builtin_memcpy(dest, config, sizeof(config));
	}
	inline void clearConfig(void) {
		__builtin_memset(config, 0, sizeof(config));
	}

	void initConfig(uint8_t maxAttempts, bool hasPublicSection = false);
	bool isPublicDataEmpty(void) const;
	bool isDataEmpty(void) const;
	bool isReadableDataEmpty(void) const;
	size_t toQRString(char *output) const;
};

}
