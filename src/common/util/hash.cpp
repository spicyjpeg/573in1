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

#include <stddef.h>
#include <stdint.h>
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "ps1/registers.h"

namespace util {

/* String hashing (http://www.cse.yorku.ca/~oz/hash.html) */

Hash hash(const char *str, char terminator) {
	auto _str  = reinterpret_cast<const uint8_t *>(str);
	Hash value = 0;

	while (*_str && (*_str != terminator))
		value = Hash(*(_str++)) + (value << 6) + (value << 16) - value;

	return value;
}

Hash hash(const uint8_t *data, size_t length) {
	Hash value = 0;

	for (; length; length--)
		value = Hash(*(data++)) + (value << 6) + (value << 16) - value;

	return value;
}

/* CRC calculation */

static constexpr uint8_t  _CRC8_POLY  = 0x8c;
static constexpr uint16_t _CRC16_POLY = 0x1021;
static constexpr uint32_t _CRC32_POLY = 0xedb88320;

uint8_t dsCRC8(const uint8_t *data, size_t length) {
	uint8_t crc = 0;

	for (; length; length--) {
		uint8_t value = *(data++);

		for (int bit = 8; bit; bit--) {
			uint8_t temp = crc ^ value;

			value >>= 1;
			crc   >>= 1;
			if (temp & 1)
				crc ^= _CRC8_POLY;
		}
	}

	return crc & 0xff;
}

uint16_t zsCRC16(const uint8_t *data, size_t length) {
	uint16_t crc = 0xffff;

	for (; length; length--) {
		crc ^= *(data++) << 8;

		for (int bit = 8; bit; bit--) {
			uint16_t temp = crc;

			crc <<= 1;
			if (temp & (1 << 15))
				crc ^= _CRC16_POLY;
		}
	}

	return (crc ^ 0xffff) & 0xffff;
}

uint32_t zipCRC32(const uint8_t *data, size_t length, uint32_t crc) {
	// This CRC32 implementation uses a lookup table cached in the scratchpad
	// area in order to improve performance.
	auto table = reinterpret_cast<const uint32_t *>(CACHE_BASE);
	crc        = ~crc;

	for (; length; length--)
		crc = (crc >> 8) ^ table[(crc ^ *(data++)) & 0xff];

	return ~crc;
}

void initZipCRC32(void) {
	auto table = reinterpret_cast<uint32_t *>(CACHE_BASE);

	for (int i = 0; i < 256; i++) {
		uint32_t crc = i;

		for (int bit = 8; bit; bit--) {
			uint32_t temp = crc;

			crc >>= 1;
			if (temp & 1)
				crc ^= _CRC32_POLY;
		}

		*(table++) = crc;
	}
}

extern "C" uint32_t mz_crc32(uint32_t crc, const uint8_t *data, size_t length) {
	return zipCRC32(data, length, crc);
}

/* MD5 hash */

static const uint32_t _MD5_SEED[]{
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476
};

static const uint32_t _MD5_ADD[][16]{
	{
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821
	}, {
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a
	}, {
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665
	}, {
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	}
};

static const uint8_t _MD5_SHIFT[][4]{
	{ 7, 12, 17, 22 },
	{ 5,  9, 14, 20 },
	{ 4, 11, 16, 23 },
	{ 6, 10, 15, 21 }
};

MD5::MD5(void)
: _blockCount(0), _bufferLength(0) {
	__builtin_memcpy(_state, _MD5_SEED, sizeof(_state));
}

void MD5::_flushBlock(const void *data) {
	auto input = reinterpret_cast<const uint32_t *>(data);

	assertAligned<uint32_t>(data);

	auto a = _state[0], b = _state[1], c = _state[2], d = _state[3];

	for (int i = 0; i < 16; i++) {
		auto _d = d;
		auto _e = a + _addF(b, c, d) + input[_indexF(i)] + _MD5_ADD[0][i];

		d  = c;
		c  = b;
		b += rotateLeft(_e, _MD5_SHIFT[0][i % 4]);
		a  = _d;
	}
	for (int i = 0; i < 16; i++) {
		auto _d = d;
		auto _e = a + _addG(b, c, d) + input[_indexG(i)] + _MD5_ADD[1][i];

		d  = c;
		c  = b;
		b += rotateLeft(_e, _MD5_SHIFT[1][i % 4]);
		a  = _d;
	}
	for (int i = 0; i < 16; i++) {
		auto _d = d;
		auto _e = a + _addH(b, c, d) + input[_indexH(i)] + _MD5_ADD[2][i];

		d  = c;
		c  = b;
		b += rotateLeft(_e, _MD5_SHIFT[2][i % 4]);
		a  = _d;
	}
	for (int i = 0; i < 16; i++) {
		auto _d = d;
		auto _e = a + _addI(b, c, d) + input[_indexI(i)] + _MD5_ADD[3][i];

		d  = c;
		c  = b;
		b += rotateLeft(_e, _MD5_SHIFT[3][i % 4]);
		a  = _d;
	}

	_state[0] += a;
	_state[1] += b;
	_state[2] += c;
	_state[3] += d;
	_blockCount++;
}

void MD5::update(const uint8_t *data, size_t length) {
	if (_bufferLength > 0) {
		auto ptr       = &_blockBuffer[_bufferLength];
		auto freeSpace = sizeof(_blockBuffer) - _bufferLength;

		if (length >= freeSpace) {
			__builtin_memcpy(ptr, data, freeSpace);
			_flushBlock(_blockBuffer);

			data         += freeSpace;
			length       -= freeSpace;
			_bufferLength = 0;
		} else {
			__builtin_memcpy(ptr, data, length);

			_bufferLength += length;
			return;
		}
	}

	// Avoid copying data to the intermediate block buffer whenever possible.
	for (;
		length >= sizeof(_blockBuffer);
		length -= sizeof(_blockBuffer), data += sizeof(_blockBuffer)
	)
		_flushBlock(data);

	if (length > 0) {
		__builtin_memcpy(_blockBuffer, data, length);
		_bufferLength = length;
	}
}

void MD5::digest(uint8_t *output) {
	uint64_t length = (
		uint64_t(_blockCount) * sizeof(_blockBuffer) + uint64_t(_bufferLength)
	) * 8;

	_blockBuffer[_bufferLength++] = 1 << 7;

	while (_bufferLength != (sizeof(_blockBuffer) - 8)) {
		if (_bufferLength == sizeof(_blockBuffer)) {
			_flushBlock(_blockBuffer);
			_bufferLength = 0;
		}

		_blockBuffer[_bufferLength++] = 0;
	}

	for (int i = 8; i > 0; i--, length >>= 8)
		_blockBuffer[_bufferLength++] = uint8_t(length & 0xff);

	_flushBlock(_blockBuffer);
	__builtin_memcpy(output, _state, sizeof(_state));
}

}
