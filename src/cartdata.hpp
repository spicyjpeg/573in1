
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "cart.hpp"
#include "util.hpp"

namespace cart {

/* Cartridge data parsers */

static constexpr size_t CODE_LENGTH       = 5;
static constexpr size_t REGION_MIN_LENGTH = 2;
static constexpr size_t REGION_MAX_LENGTH = 5;

class Parser {
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

	inline Parser(Dump &dump, uint8_t flags = 0)
	: _dump(dump), flags(flags) {}

	virtual ~Parser(void) {}
	virtual size_t getCode(char *output) const { return 0; }
	virtual size_t getRegion(char *output) const { return 0; }
	virtual IdentifierSet *getIdentifiers(void) { return nullptr; }
	virtual void flush(void) {}
	virtual bool validate(void);
};

class SimpleParser : public Parser {
private:
	inline SimpleHeader *_getHeader(void) const {
		return reinterpret_cast<SimpleHeader *>(_getPublicData());
	}

public:
	inline SimpleParser(Dump &dump, uint8_t flags = 0)
	: Parser(dump, flags | DATA_HAS_PUBLIC_SECTION) {}

	size_t getRegion(char *output) const;
};

class BasicParser : public Parser {
private:
	inline BasicHeader *_getHeader(void) const {
		return reinterpret_cast<BasicHeader *>(_getPublicData());
	}

public:
	inline BasicParser(Dump &dump, uint8_t flags = 0)
	: Parser(dump, flags) {}

	size_t getRegion(char *output) const;
	IdentifierSet *getIdentifiers(void);
	bool validate(void);
};

class ExtendedParser : public Parser {
private:
	inline ExtendedHeader *_getHeader(void) const {
		return reinterpret_cast<ExtendedHeader *>(_getPublicData());
	}

public:
	inline ExtendedParser(Dump &dump, uint8_t flags = 0)
	: Parser(dump, flags | DATA_HAS_CODE_PREFIX) {}

	size_t getCode(char *output) const;
	size_t getRegion(char *output) const;
	IdentifierSet *getIdentifiers(void);
	void flush(void);
	bool validate(void);
};

bool isValidRegion(const char *region);
Parser *newCartParser(Dump &dump, FormatType formatType, uint8_t flags = 0);
Parser *newCartParser(Dump &dump);

/* Cartridge database */

class [[gnu::packed]] DBEntry {
public:
	ChipType    chipType;
	FormatType  formatType;
	TraceIDType traceIDType;
	uint8_t     flags;

	uint8_t  traceIDParam, installIDPrefix;
	uint16_t year;
	uint8_t  dataKey[8];
	char     code[8], region[8], name[64];

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

class CartDB : public util::Data {
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

	const DBEntry *lookup(const char *code, const char *region) const;
};

}
