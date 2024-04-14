
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/util.hpp"
#include "main/cart.hpp"

namespace cart {

/* Cartridge data parsers */

static constexpr size_t CODE_LENGTH        = 5;
static constexpr size_t CODE_PREFIX_LENGTH = 2;
static constexpr size_t REGION_MIN_LENGTH  = 2;
static constexpr size_t REGION_MAX_LENGTH  = 5;

class CartParser {
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

	inline CartParser(Dump &dump, uint8_t flags = 0)
	: _dump(dump), flags(flags) {}

	virtual ~CartParser(void) {}
	virtual size_t getCode(char *output) const { return 0; }
	virtual void setCode(const char *input) {}
	virtual size_t getRegion(char *output) const { return 0; }
	virtual void setRegion(const char *input) {}
	virtual uint16_t getYear(void) const { return 0; }
	virtual void setYear(uint16_t value) {}
	virtual IdentifierSet *getIdentifiers(void) { return nullptr; }
	virtual PublicIdentifierSet *getPublicIdentifiers(void) { return nullptr; }
	virtual void flush(void) {}
	virtual bool validate(void);
};

class SimpleCartParser : public CartParser {
private:
	inline SimpleHeader *_getHeader(void) const {
		return reinterpret_cast<SimpleHeader *>(_getPublicData());
	}

public:
	inline SimpleCartParser(Dump &dump, uint8_t flags = 0)
	: CartParser(dump, flags | DATA_HAS_PUBLIC_SECTION) {}

	size_t getRegion(char *output) const;
	void setRegion(const char *input);
};

class BasicCartParser : public CartParser {
private:
	inline BasicHeader *_getHeader(void) const {
		return reinterpret_cast<BasicHeader *>(_getPublicData());
	}

public:
	inline BasicCartParser(Dump &dump, uint8_t flags = 0)
	: CartParser(dump, flags) {}

	void setCode(const char *input);
	size_t getRegion(char *output) const;
	void setRegion(const char *input);
	IdentifierSet *getIdentifiers(void);
	void flush(void);
	bool validate(void);
};

class ExtendedCartParser : public CartParser {
private:
	inline ExtendedHeader *_getHeader(void) const {
		return reinterpret_cast<ExtendedHeader *>(_getPublicData());
	}

public:
	inline ExtendedCartParser(Dump &dump, uint8_t flags = 0)
	: CartParser(dump, flags | DATA_HAS_CODE_PREFIX) {}

	size_t getCode(char *output) const;
	void setCode(const char *input);
	size_t getRegion(char *output) const;
	void setRegion(const char *input);
	uint16_t getYear(void) const;
	void setYear(uint16_t value);
	IdentifierSet *getIdentifiers(void);
	PublicIdentifierSet *getPublicIdentifiers(void);
	void flush(void);
	bool validate(void);
};

bool isValidRegion(const char *region);
bool isValidUpgradeRegion(const char *region);
CartParser *newCartParser(Dump &dump, FormatType formatType, uint8_t flags = 0);
CartParser *newCartParser(Dump &dump);

/* Cartridge and flash header database */

class [[gnu::packed]] CartDBEntry {
public:
	ChipType    chipType;
	FormatType  formatType;
	TraceIDType traceIDType;
	uint8_t     flags;

	uint8_t  traceIDParam, installIDPrefix;
	uint16_t year;
	uint8_t  dataKey[8];
	char     code[8], region[8], name[96];

	inline int compare(const char *_code, const char *_region) const {
		int diff = __builtin_strncmp(
			&code[CODE_PREFIX_LENGTH], &_code[CODE_PREFIX_LENGTH],
			CODE_LENGTH - CODE_PREFIX_LENGTH + 1
		);
		if (diff)
			return diff;

		diff = __builtin_strncmp(code, _code, CODE_PREFIX_LENGTH);
		if (diff)
			return diff;

		return __builtin_strncmp(region, _region, REGION_MAX_LENGTH);
	}
	inline int getDisplayName(char *output, size_t length) const {
		return snprintf(output, length, "%s %s\t%s", code, region, name);
	}
	inline bool requiresCartID(void) const {
		if (flags & DATA_HAS_CART_ID)
			return true;
		if ((flags & DATA_HAS_TRACE_ID) && (traceIDType >= TID_82_BIG_ENDIAN))
			return true;

		return false;
	}
	inline void copyKeyTo(uint8_t *dest) const {
		__builtin_memcpy(dest, dataKey, sizeof(dataKey));
	}
};

class [[gnu::packed]] FlashDBEntry {
public:
	// TODO
	uint8_t flags;

	uint16_t year;
	uint8_t  hashSalt[8];
	char     code[8], region[8], name[96];

	inline int compare(const char *_code, const char *_region) const {
		int diff = __builtin_strncmp(
			&code[CODE_PREFIX_LENGTH], &_code[CODE_PREFIX_LENGTH],
			CODE_LENGTH - CODE_PREFIX_LENGTH + 1
		);
		if (diff)
			return diff;

		diff = __builtin_strncmp(code, _code, CODE_PREFIX_LENGTH);
		if (diff)
			return diff;

		return __builtin_strncmp(region, _region, REGION_MAX_LENGTH);
	}
};

template<typename T> class DB : public util::Data {
public:
	inline const T *operator[](int index) const {
		return get(index);
	}
	inline size_t getNumEntries(void) const {
		return length / sizeof(T);
	}

	const T *get(int index) const;
	const T *lookup(const char *code, const char *region) const;
};

using CartDB  = DB<CartDBEntry>;
using FlashDB = DB<FlashDBEntry>;

}
