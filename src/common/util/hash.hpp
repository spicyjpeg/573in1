/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
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

namespace util {

/* String hashing (http://www.cse.yorku.ca/~oz/hash.html) */

using Hash = uint32_t;

template<typename T> static constexpr inline Hash hash(
	const T *const data, size_t length = -1, Hash value = 0
) {
	if (*data && length)
		return hash(
			&data[1], length - 1,
			Hash(*data) + (value << 6) + (value << 16) - value
		);

	return value;
}

Hash hash(const char *str, char terminator = 0);
Hash hash(const uint8_t *data, size_t length);

/* Hash table parser */

template<typename T> static inline const T *getHashTableEntry(
	const T *table, size_t numBuckets, Hash id
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

uint8_t dsCRC8(const uint8_t *data, size_t length);
uint16_t zsCRC16(const uint8_t *data, size_t length);
uint32_t zipCRC32(const uint8_t *data, size_t length, uint32_t crc = 0);
void initZipCRC32(void);

/* MD5 hash */

class MD5 {
private:
	uint32_t _state[4];
	uint8_t  _blockBuffer[64];
	size_t   _blockCount, _bufferLength;

	static inline int _indexF(int index) {
		return index;
	}
	static inline uint32_t _addF(uint32_t x, uint32_t y, uint32_t z) {
		return z ^ (x & (y ^ z)); // (x & y) | ((~x) & z)
	}
	static inline int _indexG(int index) {
		return ((index * 5) + 1) % 16;
	}
	static inline uint32_t _addG(uint32_t x, uint32_t y, uint32_t z) {
		return y ^ (z & (x ^ y)); // (x & z) | (y & (~z))
	}
	static inline int _indexH(int index) {
		return ((index * 3) + 5) % 16;
	}
	static inline uint32_t _addH(uint32_t x, uint32_t y, uint32_t z) {
		return x ^ y ^ z;
	}
	static inline int _indexI(int index) {
		return (index * 7) % 16;
	}
	static inline uint32_t _addI(uint32_t x, uint32_t y, uint32_t z) {
		return (y ^ (x | (~z)));
	}

	void _flushBlock(const void *data);

public:
	MD5(void);
	void update(const uint8_t *data, size_t length);
	void digest(uint8_t *output);
};

}

/* String hashing operator */

static constexpr inline util::Hash operator""_h(
	const char *const literal, size_t length
) {
	return util::hash(literal, length);
}
