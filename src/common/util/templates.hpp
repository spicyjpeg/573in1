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

template<typename T> static constexpr inline uint32_t sum(
	const T *data,
	size_t  length
) {
	uint32_t value = 0;

	for (; length > 0; length--)
		value += uint32_t(*(data++));

	return value;
}

template<typename T> static constexpr inline T bitwiseXOR(
	const T *data,
	size_t  length
) {
	T value = 0;

	for (; length > 0; length--)
		value ^= *(data++);

	return value;
}

template<typename T> static constexpr inline bool isEmpty(
	const T *data,
	size_t  length,
	T       value = 0
) {
	for (; length > 0; length--) {
		if (*(data++) != value)
			return false;
	}

	return true;
}

template<typename T> static constexpr inline T min(T a, T b) {
	return (a < b) ? a : b;
}

template<typename T> static constexpr inline T max(T a, T b) {
	return (a > b) ? a : b;
}

template<typename T> static constexpr inline T clamp(
	T value,
	T minValue,
	T maxValue
) {
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;

	return value;
}

template<typename T> static constexpr inline T rotateLeft(T value, int amount) {
	return T((value << amount) | (value >> (sizeof(T) * 8 - amount)));
}

template<typename T> static constexpr inline T rotateRight(T value, int amount) {
	return T((value >> amount) | (value << (sizeof(T) * 8 - amount)));
}

// These shall only be used with unsigned types.
template<typename T> static constexpr inline T truncateToMultiple(
	T value,
	T length
) {
	return value - (value % length);
}

template<typename T> static constexpr inline T roundUpToMultiple(
	T value,
	T length
) {
	T diff = value % length;
	return diff ? (value - diff + length) : value;
}

template<typename T, typename X> static inline void assertAligned(X *ptr) {
	assert(!(uintptr_t(ptr) % alignof(T)));
}

template<typename T> static inline void clear(T &obj, uint8_t value = 0) {
	__builtin_memset(&obj, value, sizeof(obj));
}

template<typename T> static inline void copy(T &dest, const T &source) {
	__builtin_memcpy(&dest, &source, sizeof(source));
}

template<typename T> static constexpr inline size_t countOf(T &array) {
	return sizeof(array) / sizeof(array[0]);
}

template<typename T> static constexpr inline auto *endOf(T &array) {
	return &array[countOf(array)];
}

/* Concatenation and BCD conversion */

template<typename T, typename V, typename... A>
static constexpr inline T concat(V value, A... next) {
	constexpr int remaining = sizeof...(next);

	if constexpr (remaining)
		return 0
			| T(value)
			| (concat<T>(next...) << (remaining ? (sizeof(V) * 8) : 0));

	return T(value);
}

template<typename T, typename V, int N = sizeof(T) / sizeof(V)>
static constexpr inline T mirror(V value) {
	constexpr int remaining = max(N - 1, 0);

	if constexpr (remaining)
		return 0
			| T(value)
			| (mirror<T, V, remaining>(value) << (sizeof(V) * 8));

	return T(value);
}

static constexpr inline uint16_t concat2(uint8_t low, uint8_t high) {
	return concat<uint16_t>(low, high);
}
static constexpr inline uint16_t mirror2(uint8_t value) {
	return mirror<uint16_t>(value);
}

static constexpr inline uint32_t concat4(uint16_t low, uint16_t high) {
	return concat<uint32_t>(low, high);
}
static constexpr inline uint32_t mirror4(uint16_t value) {
	return mirror<uint32_t>(value);
}

static constexpr inline uint32_t concat4(
	uint8_t a, uint8_t b, uint8_t c, uint8_t d
) {
	return concat<uint32_t>(a, b, c, d);
}
static constexpr inline uint32_t mirror4(uint8_t value) {
	return mirror<uint32_t>(value);
}

static constexpr inline uint8_t encodeBCD(uint8_t value) {
	// output = units + tens * 16
	//        = units + tens * 10 + tens * 6
	//        = value             + tens * 6
	return value + (value / 10) * 6;
}

static constexpr inline uint8_t decodeBCD(uint8_t value) {
	// output = low + high * 10
	//        = low + high * 16 - high * 6
	//        = value           - high * 6
	return value - (value >> 4) * 6;
}

/* Simple "smart" pointer */

class Data {
public:
	void   *ptr;
	size_t length;
	bool   destructible;

	inline Data(void)
	: ptr(nullptr), length(0), destructible(false) {}
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
		if (ptr && destructible)
			delete[] as<uint8_t>();

		ptr          = _length ? (new uint8_t[_length]) : nullptr;
		length       = _length;
		destructible = true;

		return ptr;
	}
	inline void destroy(void) {
		if (ptr) {
			if (destructible)
				delete[] as<uint8_t>();

			ptr          = nullptr;
			length       = 0;
			destructible = false;
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

	inline T *pushItem(void) {
		if (length >= N)
			return nullptr;

		size_t i = _tail;
		_tail    = (i + 1) % N;
		length++;

		return &_items[i];
	}
	inline T *popItem(void) {
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

/* Character concatenation operator */

static consteval inline uint32_t operator""_c(
	const char *const literal, size_t length
) {
	return util::concat4(
		(length > 0) ? literal[0] : 0,
		(length > 1) ? literal[1] : 0,
		(length > 2) ? literal[2] : 0,
		(length > 3) ? literal[3] : 0
	);
}
