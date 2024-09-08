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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

namespace util {

/* Misc. template utilities */

template<typename T> static inline uint32_t sum(const T *data, size_t length) {
	uint32_t value = 0;

	for (; length; length--)
		value += uint32_t(*(data++));

	return value;
}

template<typename T> static constexpr inline T min(T a, T b) {
	return (a < b) ? a : b;
}

template<typename T> static constexpr inline T max(T a, T b) {
	return (a > b) ? a : b;
}

template<typename T> static constexpr inline T clamp(
	T value, T minValue, T maxValue
) {
	return (value < minValue) ? minValue :
		((value > maxValue) ? maxValue : value);
}

template<typename T> static constexpr inline T rotateLeft(T value, int amount) {
	return T((value << amount) | (value >> (sizeof(T) * 8 - amount)));
}

template<typename T> static constexpr inline T rotateRight(T value, int amount) {
	return T((value >> amount) | (value << (sizeof(T) * 8 - amount)));
}

// These shall only be used with unsigned types.
template<typename T> static constexpr inline T truncateToMultiple(
	T value, T length
) {
	return value - (value % length);
}

template<typename T> static constexpr inline T roundUpToMultiple(
	T value, T length
) {
	T diff = value % length;
	return diff ? (value - diff + length) : value;
}

template<typename T, typename X> static inline void assertAligned(X *ptr) {
	//assert(!(reinterpret_cast<uintptr_t>(ptr) % alignof(T)));
}

template<typename T> static inline void clear(T &obj, uint8_t value = 0) {
	__builtin_memset(&obj, value, sizeof(obj));
}

template<typename T> static constexpr inline size_t countOf(T &array) {
	return sizeof(array) / sizeof(array[0]);
}

template<typename T, typename X> static inline T forcedCast(X item) {
	return reinterpret_cast<T>(reinterpret_cast<void *>(item));
}

static constexpr inline uint16_t concat2(uint8_t low, uint8_t high) {
	return low | (high << 8);
}

static constexpr inline uint32_t concat4(uint16_t low, uint16_t high) {
	return low | (high << 16);
}

static constexpr inline uint32_t concat4(
	uint8_t a, uint8_t b, uint8_t c, uint8_t d
) {
	return a | (b << 8) | (c << 16) | (d << 24);
}

static constexpr inline uint64_t concat8(uint32_t low, uint32_t high) {
	return uint64_t(low) | (uint64_t(high) << 32);
}

static constexpr inline uint64_t concat8(
	uint16_t a, uint16_t b, uint16_t c, uint16_t d
) {
	return 0
		| (uint64_t(a) <<  0)
		| (uint64_t(b) << 16)
		| (uint64_t(c) << 32)
		| (uint64_t(d) << 48);
}

static constexpr inline uint64_t concat8(
	uint8_t a, uint8_t b, uint8_t c, uint8_t d,
	uint8_t e, uint8_t f, uint8_t g, uint8_t h
) {
	return 0
		| (uint64_t(a) <<  0) | (uint64_t(b) <<  8)
		| (uint64_t(c) << 16) | (uint64_t(d) << 24)
		| (uint64_t(e) << 32) | (uint64_t(f) << 40)
		| (uint64_t(g) << 48) | (uint64_t(h) << 56);
}

/* Simple "smart" pointer */

class Data {
public:
	void   *ptr;
	size_t length;

	inline Data(void)
	: ptr(nullptr), length(0) {}
	inline ~Data(void) {
		destroy();
	}

	template<typename T> inline T *as(void) {
		return reinterpret_cast<T *>(ptr);
	}
	template<typename T> inline const T *as(void) const {
		return reinterpret_cast<const T *>(ptr);
	}
	template<typename T> inline T *allocate(size_t count = 1) {
		return reinterpret_cast<T *>(allocate(sizeof(T) * count));
	}

	inline void *allocate(size_t _length) {
		if (ptr)
			delete[] as<uint8_t>();

		ptr    = _length ? (new uint8_t[_length]) : nullptr;
		length = _length;

		return ptr;
	}
	inline void destroy(void) {
		if (ptr) {
			delete[] as<uint8_t>();
			ptr = nullptr;
		}
	}
};

/* Simple ring buffer */

template<typename T, size_t N> class RingBuffer {
private:
	T      _items[N];
	size_t _head, _tail;

public:
	size_t length;

	inline RingBuffer(void)
	: _head(0), _tail(0), length(0) {}

	inline T *pushItem(void) volatile {
		if (length >= N)
			return nullptr;

		size_t i = _tail;
		_tail    = (i + 1) % N;
		length++;

		return &_items[i];
	}
	inline T *popItem(void) volatile {
		if (!length)
			return nullptr;

		size_t i = _head;
		_head    = (i + 1) % N;
		length--;

		return &_items[i];
	}
	inline T *peekItem(void) const {
		if (!length)
			return nullptr;

		return &_items[_head];
	}
};

}
