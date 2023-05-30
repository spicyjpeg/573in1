
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "asset.hpp"
#include "cart.hpp"

namespace cartdb {

enum IdentifyResult {
	UNIDENTIFIED = 0,
	IDENTIFIED   = 1,
	BLANK        = 2
};

enum EntryFlag : uint8_t {
	HAS_SYSTEM_ID = 1 << 0,
	HAS_CART_ID   = 1 << 1,
	HAS_ZS_ID     = 1 << 2,
	HAS_CHECKSUM  = 1 << 3
};

static constexpr int ENTRY_VERSION = 1;

class [[gnu::packed]] Entry {
public:
	uint8_t        version;
	cart::ChipType chipType;

	uint8_t flags, _reserved;
	uint8_t systemIDOffset, cartIDOffset, zsIDOffset, checksumOffset;
	char    code[8], region[8], name[64];
	uint8_t dataKey[8], config[8];

	inline int getDisplayName(char *output, size_t length) const {
		return snprintf(output, length, "%s %s\t%s", code, region, name);
	}
};

class [[gnu::packed]] X76F041Entry : public Entry {
public:
	uint8_t data[512];
};

class [[gnu::packed]] X76F100Entry : public Entry {
public:
	uint8_t data[112];
};

class [[gnu::packed]] ZS01Entry : public Entry {
public:
	uint8_t data[112];
};

class CartDB {
private:
	cart::ChipType _chipType;
	size_t         _entryLength;

public:
	asset::Asset data;
	size_t       numEntries;

	inline CartDB(void)
	: _entryLength(0), numEntries(0) {}
	inline const Entry &getEntry(int index) const {
		auto _data = reinterpret_cast<const uint8_t *>(data.ptr);
		//assert(data);

		return *reinterpret_cast<const Entry *>(&_data[index * _entryLength]);
	}

	bool init(void);
	IdentifyResult identifyCart(cart::Cart &cart) const;
};

}
