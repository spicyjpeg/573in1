
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/util.hpp"
#include "main/cart.hpp"

namespace cart {

/* Definitions */

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

// |                         | Simple (cart) | Basic (cart) | Extended (cart) | Extended (ROM) |
// | :---------------------- | :------------ | :----------- | :-------------- | :------------- |
// | DATA_HAS_CODE_PREFIX    |               | Optional     | Mandatory       | Mandatory      |
// | DATA_HAS_*_ID           |               | Optional     | Optional        |                |
// | DATA_HAS_SYSTEM_ID      |               | Optional     | Optional        | Optional       |
// | DATA_HAS_PUBLIC_SECTION | Mandatory     |              | Optional        |                |
// | DATA_GX706_WORKAROUND   |               |              | Optional        | Optional       |

// Note that DATA_HAS_SYSTEM_ID is used to indicate the presence of a signature
// (rather than a "raw" system ID) in an extended ROM header.
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

static constexpr size_t CODE_LENGTH        = 5;
static constexpr size_t CODE_PREFIX_LENGTH = 2;
static constexpr size_t REGION_MIN_LENGTH  = 2;
static constexpr size_t REGION_MAX_LENGTH  = 5;

/* Common data structures */

class [[gnu::packed]] IdentifierSet {
public:
	Identifier traceID, cartID, installID, systemID; // aka TID, SID, MID, XID

	inline void clear(void) {
		__builtin_memset(this, 0, sizeof(IdentifierSet));
	}

	uint8_t getFlags(void) const;
	void setInstallID(uint8_t prefix);
	void updateTraceID(
		TraceIDType type, int param, const Identifier *_cartID = nullptr
	);
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

/* Cartridge data parsers/writers */

class CartParser {
protected:
	CartDump &_dump;

	inline uint8_t *_getPublicData(void) const {
		if (flags & DATA_HAS_PUBLIC_SECTION)
			return &_dump.data[_dump.getChipSize().publicDataOffset];
		else
			return _dump.data;
	}

public:
	uint8_t flags;

	inline CartParser(CartDump &dump, uint8_t flags = 0)
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
	inline SimpleCartParser(CartDump &dump, uint8_t flags = 0)
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
	inline BasicCartParser(CartDump &dump, uint8_t flags = 0)
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
	inline ExtendedCartParser(CartDump &dump, uint8_t flags = 0)
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
CartParser *newCartParser(
	CartDump &dump, FormatType formatType, uint8_t flags = 0
);
CartParser *newCartParser(CartDump &dump);

/* Flash and RTC header parsers/writers */

class ROMHeaderParser {
protected:
	ROMHeaderDump &_dump;

public:
	uint8_t flags;

	inline ROMHeaderParser(ROMHeaderDump &dump, uint8_t flags = 0)
	: _dump(dump), flags(flags) {}

	virtual ~ROMHeaderParser(void) {}
	virtual size_t getCode(char *output) const { return 0; }
	virtual void setCode(const char *input) {}
	virtual size_t getRegion(char *output) const { return 0; }
	virtual void setRegion(const char *input) {}
	virtual uint16_t getYear(void) const { return 0; }
	virtual void setYear(uint16_t value) {}
	virtual void flush(void) {}
	virtual bool validate(void);
};

class ExtendedROMHeaderParser : public ROMHeaderParser {
private:
	inline ExtendedHeader *_getHeader(void) const {
		return reinterpret_cast<ExtendedHeader *>(_dump.data);
	}
	inline uint8_t *_getSignature(void) const {
		return &_dump.data[sizeof(ExtendedHeader)];
	}

	void _calculateSignature(uint8_t *output) const;

public:
	inline ExtendedROMHeaderParser(ROMHeaderDump &dump, uint8_t flags = 0)
	: ROMHeaderParser(dump, flags | DATA_HAS_CODE_PREFIX) {}

	size_t getCode(char *output) const;
	void setCode(const char *input);
	size_t getRegion(char *output) const;
	void setRegion(const char *input);
	uint16_t getYear(void) const;
	void setYear(uint16_t value);
	void flush(void);
	bool validate(void);
};

ROMHeaderParser *newROMHeaderParser(
	ROMHeaderDump &dump, FormatType formatType, uint8_t flags = 0
);
ROMHeaderParser *newROMHeaderParser(ROMHeaderDump &dump);

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

class [[gnu::packed]] ROMHeaderDBEntry {
public:
	FormatType formatType;
	uint8_t    flags;

	uint16_t year;
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

using CartDB      = DB<CartDBEntry>;
using ROMHeaderDB = DB<ROMHeaderDBEntry>;

}
