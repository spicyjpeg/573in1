/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"

namespace util {

/* String hashing (http://www.cse.yorku.ca/~oz/hash.html) */

using Hash = uint32_t;

Hash hash(const char *str, char terminator = 0, Hash value = 0);
Hash hash(const uint8_t *data, size_t length, Hash value = 0);

/* Hash table parser */

template<typename T> static inline const T *getHashTableEntry(
	const T *table,
	size_t  numBuckets,
	Hash    id
) {
#if 0
	auto index = id % NB;
#else
	auto index = id & (numBuckets - 1);
#endif

	do {
		auto entry = &table[index];
		index      = entry->getChained();

		if (entry->getHash() == id)
			return entry;
	} while (index);

	return nullptr;
}

/* CRC calculation */

// This CRC32 implementation uses a lookup table cached in the scratchpad area
// in order to improve performance.
class ZIPCRC32 {
private:
	uint32_t _table[256];

public:
	[[gnu::always_inline]] inline uint32_t update(
		uint8_t  value,
		uint32_t crc
	) const {
		return (crc >> 8) ^ _table[(crc ^ value) & 0xff];
	}

	void init(void);
	uint32_t update(const uint8_t *data, size_t length, uint32_t crc = 0) const;
};

static auto &zipCRC32 = *reinterpret_cast<ZIPCRC32 *>(CACHE_BASE);

uint8_t dsCRC8(const uint8_t *data, size_t length);
uint16_t zsCRC16(const uint8_t *data, size_t length);

/* MD5 hash */

class MD5 {
private:
	uint32_t _state[4];
	uint8_t  _blockBuffer[64];
	size_t   _blockCount, _bufferLength;

	void _flushBlock(const void *data);

public:
	MD5(void);
	void update(const uint8_t *data, size_t length);
	void digest(uint8_t *output);
};

}

/* String hashing operator */

static consteval inline util::Hash operator""_h(
	const char *literal,
	size_t     length
) {
	util::Hash value = 0;

	for (; length > 0; length--)
		value = util::Hash(*(literal++)) + (value << 6) + (value << 16) - value;

	return value;
}
