
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "asset.hpp"
#include "cartio.hpp"

namespace cart {

/* Definitions */

enum FormatType : uint8_t {
	BLANK    = 0,
	SIMPLE   = 1,
	BASIC    = 2,
	EXTENDED = 3
};

// |                         | Simple    | Basic    | Extended  |
// | :---------------------- | :-------- | :------- | :-------- |
// | DATA_HAS_CODE_PREFIX    |           | Optional | Mandatory |
// | DATA_HAS_*_ID           |           | Optional | Optional  |
// | DATA_HAS_PUBLIC_SECTION | Mandatory |          | Optional  |

enum FormatFlags : uint8_t {
	DATA_HAS_CODE_PREFIX    = 1 << 0,
	DATA_HAS_TRACE_ID       = 1 << 1,
	DATA_HAS_CART_ID        = 1 << 2,
	DATA_HAS_INSTALL_ID     = 1 << 3,
	DATA_HAS_SYSTEM_ID      = 1 << 4,
	DATA_HAS_PUBLIC_SECTION = 1 << 5
};

static constexpr size_t CODE_LENGTH       = 5;
static constexpr size_t REGION_MIN_LENGTH = 2;
static constexpr size_t REGION_MAX_LENGTH = 5;

bool isValidRegion(const char *region);

/* Common data structures */

class [[gnu::packed]] SimpleHeader {
public:
	char region[4];
};

class [[gnu::packed]] BasicHeader {
public:
	char    region[2], codePrefix[2];
	uint8_t checksum, _pad[3];

	void updateChecksum(void);
	bool validateChecksum(void) const;
};

class [[gnu::packed]] ExtendedHeader {
public:
	char     code[8];
	uint16_t year;      // BCD, can be little endian, big endian or zero
	char     region[4];
	uint16_t checksum;

	void updateChecksum(void);
	bool validateChecksum(void) const;
};

class [[gnu::packed]] IdentifierSet {
public:
	Identifier traceID, cartID, installID, systemID; // aka TID, SID, MID, XID

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(IdentifierSet));
	}

	uint8_t getFlags(void) const;
	void setInstallID(uint8_t prefix);
	void updateTraceID(uint8_t prefix, int param);
};

class [[gnu::packed]] PublicIdentifierSet {
public:
	Identifier traceID, cartID; // aka TID, SID

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(PublicIdentifierSet));
	}
};

/* Data formats */

class CartData {
protected:
	Dump &_dump;

	inline uint8_t *_getPublicData(void) const {
		if (flags & DATA_HAS_PUBLIC_SECTION)
			return &_dump.data[_dump.getChipSize().publicDataOffset];
		else
			return _dump.data;
	}

public:
	uint8_t flags;

	inline CartData(Dump &dump, uint8_t flags = 0)
	: _dump(dump), flags(flags) {}

	virtual size_t getRegion(char *output) const { return 0; }
	virtual IdentifierSet *getIdentifiers(void) { return nullptr; }
	virtual void flush(void) {}
	virtual bool validate(void);
};

class SimpleCartData : public CartData {
private:
	inline SimpleHeader *_getHeader(void) const {
		return reinterpret_cast<SimpleHeader *>(_getPublicData());
	}

public:
	inline SimpleCartData(Dump &dump, uint8_t flags = 0)
	: CartData(dump, flags | DATA_HAS_PUBLIC_SECTION) {}

	size_t getRegion(char *output) const;
};

class BasicCartData : public CartData {
private:
	inline BasicHeader *_getHeader(void) const {
		return reinterpret_cast<BasicHeader *>(_getPublicData());
	}

public:
	inline BasicCartData(Dump &dump, uint8_t flags = 0)
	: CartData(dump, flags) {}

	size_t getRegion(char *output) const;
	IdentifierSet *getIdentifiers(void);
	bool validate(void);
};

class ExtendedCartData : public CartData {
private:
	inline ExtendedHeader *_getHeader(void) const {
		return reinterpret_cast<ExtendedHeader *>(_getPublicData());
	}

public:
	inline ExtendedCartData(Dump &dump, uint8_t flags = 0)
	: CartData(dump, flags | DATA_HAS_CODE_PREFIX) {}

	size_t getRegion(char *output) const;
	IdentifierSet *getIdentifiers(void);
	void flush(void);
	bool validate(void);
};

CartData *createCartData(Dump &dump, FormatType formatType, uint8_t flags = 0);
CartData *createCartData(Dump &dump);

/* Cartridge database */

class [[gnu::packed]] DBEntry {
public:
	ChipType   chipType;
	FormatType formatType;
	uint8_t    flags;

	uint8_t traceIDPrefix, traceIDParam, installIDPrefix, _reserved[2];
	uint8_t dataKey[8];
	char    code[8], region[8], name[64];

	inline int compare(const char *_code, const char *_region) const {
		int diff = __builtin_strncmp(code, _code, CODE_LENGTH + 1);
		if (diff)
			return diff;

		// If the provided region string is longer than this entry's region but
		// the first few characters match, return 0. Do not however match
		// strings shorter than this entry's region.
		return __builtin_strncmp(region, _region, __builtin_strlen(region));
	}
	inline int getDisplayName(char *output, size_t length) const {
		return snprintf(output, length, "%s %s\t%s", code, region, name);
	}
};

class CartDB : public asset::Asset {
public:
	inline const DBEntry *operator[](int index) const {
		return get(index);
	}

	inline const DBEntry *get(int index) const {
		auto entries = reinterpret_cast<const DBEntry *>(ptr);

		if (!entries)
			return nullptr;
		if ((index * sizeof(DBEntry)) >= length)
			return nullptr;

		return &entries[index];
	}
	inline size_t getNumEntries(void) const {
		return length / sizeof(DBEntry);
	}

	const DBEntry *lookupEntry(const char *code, const char *region) const;
};

}
