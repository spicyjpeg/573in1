
#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "ps1/system.h"

namespace util {

/* Misc. template utilities */

template<typename T> static inline uint32_t sum(const T *data, size_t length) {
	uint32_t value = 0;

	for (; length; length--)
		value += uint32_t(*(data++));

	return value;
}

template<typename T> static inline T min(T a, T b) {
	return (a < b) ? a : b;
}

template<typename T> static inline T max(T a, T b) {
	return (a > b) ? a : b;
}

template<typename T> static inline T clamp(T value, T minValue, T maxValue) {
	return (value < minValue) ? minValue :
		((value > maxValue) ? maxValue : value);
}

// These shall only be used with unsigned types.
template<typename T> static inline T truncateToMultiple(T value, T length) {
	return value - (value % length);
}

template<typename T> static inline T roundUpToMultiple(T value, T length) {
	T diff = value % length;
	return diff ? (value - diff + length) : value;
}

template<typename T, typename X> static inline void assertAligned(X *ptr) {
	assert(!(reinterpret_cast<uintptr_t>(ptr) % alignof(T)));
}

template<typename T, typename X> static constexpr inline T forcedCast(X item) {
	return reinterpret_cast<T>(reinterpret_cast<void *>(item));
}

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

	inline void *allocate(size_t _length) {
		if (ptr)
			free(ptr);

		ptr    = malloc(_length);
		length = _length;

		return ptr;
	}
	inline void destroy(void) {
		if (ptr) {
			free(ptr);
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

/* Tween/animation classes */

static constexpr int TWEEN_UNIT = 1 << 12;

class LinearEasing {
public:
	template<typename T> static inline T apply(T value) {
		return value;
	}
};

class QuadInEasing {
public:
	template<typename T> static inline T apply(T value) {
		return (value * value) / TWEEN_UNIT;
	}
};

class QuadOutEasing {
public:
	template<typename T> static inline T apply(T value) {
		return (value * 2) - ((value * value) / TWEEN_UNIT);
	}
};

template<typename T, typename E> class Tween {
private:
	T   _base, _delta;
	int _endTime, _timeScale;

public:
	inline Tween(void) {
		setValue(static_cast<T>(0));
	}
	inline Tween(T start) {
		setValue(start);
	}

	inline T getValue(int time) const {
		int remaining = time - _endTime;
		if (remaining >= 0)
			return _base + _delta;

		return _base + (
			_delta * E::apply(remaining * _timeScale + TWEEN_UNIT)
		) / TWEEN_UNIT;
	}
	inline T getTargetValue(void) const {
		return _base + _delta;
	}
	inline bool isDone(int time) const {
		return time >= _endTime;
	}

	inline void setValue(int time, T start, T target, int duration) {
		//assert(duration <= 0x800);

		_base  = start;
		_delta = target - start;

		_endTime   = time + duration;
		_timeScale = TWEEN_UNIT / duration;
	}
	inline void setValue(int time, T target, int duration) {
		setValue(time, getValue(time), target, duration);
	}
	inline void setValue(T target) {
		_base    = target;
		_delta   = static_cast<T>(0);
		_endTime = 0;
	}
};

/* Logger (basically a ring buffer of lines) */

static constexpr int MAX_LOG_LINE_LENGTH = 128;
static constexpr int MAX_LOG_LINES       = 32;

class Logger {
private:
	char _lines[MAX_LOG_LINES][MAX_LOG_LINE_LENGTH];
	int  _tail;

public:
	bool enableSyslog;

	// 0 = last line, 1 = second to last, etc.
	inline const char *getLine(int line) const {
		return _lines[size_t(_tail - (line + 1)) % MAX_LOG_LINES];
	}

	Logger(void);
	void clear(void);
	void log(const char *format, ...);
};

extern Logger logger;

/* Other APIs */

uint8_t dsCRC8(const uint8_t *data, size_t length);
uint16_t zsCRC16(const uint8_t *data, size_t length);
uint32_t zipCRC32(const uint8_t *data, size_t length, uint32_t crc = 0);
void initZipCRC32(void);

size_t hexToString(char *output, const uint8_t *input, size_t length, char sep = 0);
size_t serialNumberToString(char *output, const uint8_t *input);
size_t traceIDToString(char *output, const uint8_t *input);
size_t encodeBase41(char *output, const uint8_t *input, size_t length);

/* Error strings */

extern const char *const CART_DRIVER_ERROR_NAMES[];
extern const char *const IDE_DEVICE_ERROR_NAMES[];
extern const char *const FATFS_ERROR_NAMES[];
extern const char *const MINIZ_ERROR_NAMES[];
extern const char *const MINIZ_ZIP_ERROR_NAMES[];

}

//#define LOG(...) util::logger.log(__VA_ARGS__)
#define LOG(fmt, ...) \
	util::logger.log("%s(%d): " fmt, __func__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

static constexpr inline util::Hash operator""_h(
	const char *const literal, size_t length
) {
	return util::hash(literal, length);
}
