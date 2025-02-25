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
#include <stdlib.h>

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

template<typename T> static inline T rotateLeft(T value, int amount) {
	return T((value << amount) | (value >> (sizeof(T) * 8 - amount)));
}

template<typename T> static inline T rotateRight(T value, int amount) {
	return T((value >> amount) | (value << (sizeof(T) * 8 - amount)));
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

static constexpr inline uint16_t concatenate(uint8_t a, uint8_t b) {
	return a | (b << 8);
}

static constexpr inline uint32_t concatenate(
	uint8_t a, uint8_t b, uint8_t c, uint8_t d
) {
	return a | (b << 8) | (c << 16) | (d << 24);
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

/* Date and time class */

class Date {
public:
	uint16_t year;
	uint8_t  month, day, hour, minute, second;

	inline void reset(void) {
		year   = 2024;
		month  = 1;
		day    = 1;
		hour   = 0;
		minute = 0;
		second = 0;
	}

	bool isValid(void) const;
	bool isLeapYear(void) const;
	int getDayOfWeek(void) const;
	int getMonthDayCount(void) const;
	uint32_t toDOSTime(void) const;
	size_t toString(char *output) const;
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

	inline T getTargetValue(void) const {
		return _base + _delta;
	}
	inline bool isDone(int time) const {
		return time >= _endTime;
	}
	inline void setValue(int time, T target, int duration) {
		setValue(time, getValue(time), target, duration);
	}

	void setValue(int time, T start, T target, int duration);
	void setValue(T target);
	T getValue(int time) const;
};

/* Logging framework */

static constexpr int MAX_LOG_LINE_LENGTH = 128;
static constexpr int MAX_LOG_LINES       = 64;

class LogBuffer {
private:
	char _lines[MAX_LOG_LINES][MAX_LOG_LINE_LENGTH];
	int  _tail;

public:
	inline LogBuffer(void)
	: _tail(0) {
		clear();
	}

	// 0 = last line, 1 = second to last, etc.
	inline const char *getLine(int line) const {
		return _lines[size_t(_tail - (line + 1)) % MAX_LOG_LINES];
	}

	void clear(void);
	char *allocateLine(void);
};

class Logger {
private:
	LogBuffer *_buffer;
	bool      _enableSyslog;

public:
	inline Logger(void)
	: _buffer(nullptr), _enableSyslog(false) {}

	void setLogBuffer(LogBuffer *buffer);
	void setupSyslog(int baudRate);
	void log(const char *format, ...);
};

extern Logger logger;

/* PS1 executable loader */

static constexpr size_t EXECUTABLE_BODY_OFFSET = 2048;
static constexpr size_t MAX_EXECUTABLE_ARGS    = 32;

class ExecutableHeader {
public:
	uint32_t magic[4];

	uint32_t entryPoint, initialGP;
	uint32_t textOffset, textLength;
	uint32_t dataOffset, dataLength;
	uint32_t bssOffset, bssLength;
	uint32_t stackOffset, stackLength;
	uint32_t _reserved[5];

	inline void *getEntryPoint(void) const {
		return reinterpret_cast<void *>(entryPoint);
	}
	inline void *getInitialGP(void) const {
		return reinterpret_cast<void *>(initialGP);
	}
	inline void *getTextPtr(void) const {
		return reinterpret_cast<void *>(textOffset);
	}
	inline void *getStackPtr(void) const {
		return reinterpret_cast<void *>(stackOffset + stackLength);
	}
	inline const char *getRegionString(void) const {
		return reinterpret_cast<const char *>(this + 1);
	}
	inline void relocateText(const void *source) const {
		__builtin_memcpy(getTextPtr(), source, textLength);
	}

	bool validateMagic(void) const;
};

class ExecutableLoader {
private:
	void *_entryPoint, *_initialGP;

	size_t     _numArgs;
	const char **_argListPtr;
	char       *_currentStackPtr;

public:
	inline bool copyArgument(const char *arg) {
		return copyArgument(arg, __builtin_strlen(arg));
	}
	[[noreturn]] inline void run(void) {
		run(_numArgs, _argListPtr);
	}

	ExecutableLoader(void *entryPoint, void *initialGP, void *stackTop);
	bool addArgument(const char *arg);
	bool copyArgument(const char *arg, size_t length);
	bool formatArgument(const char *format, ...);
	[[noreturn]] void run(int rawArgc, const char *const *rawArgv);
};

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

/* Other APIs */

static inline size_t getLZ4InPlaceMargin(size_t inputLength) {
	return (inputLength >> 8) + 32;
}

void decompressLZ4(
	uint8_t *output, const uint8_t *input, size_t maxOutputLength,
	size_t inputLength
);

uint8_t dsCRC8(const uint8_t *data, size_t length);
uint16_t zsCRC16(const uint8_t *data, size_t length);
uint32_t zipCRC32(const uint8_t *data, size_t length, uint32_t crc = 0);
void initZipCRC32(void);

extern const char HEX_CHARSET[], BASE41_CHARSET[];

size_t hexValueToString(char *output, uint32_t value, size_t numDigits = 8);
size_t hexToString(
	char *output, const uint8_t *input, size_t length, char separator = 0
);
size_t serialNumberToString(char *output, const uint8_t *input);
size_t traceIDToString(char *output, const uint8_t *input);
size_t encodeBase41(char *output, const uint8_t *input, size_t length);

}

static constexpr inline util::Hash operator""_h(
	const char *const literal, size_t length
) {
	return util::hash(literal, length);
}

/* Logging macros */

#define LOG(type, fmt, ...) \
	util::logger.log( \
		type ",%s(%d): " fmt, __func__, __LINE__ __VA_OPT__(,) __VA_ARGS__ \
	)

#ifdef ENABLE_APP_LOGGING
#define LOG_APP(fmt, ...) LOG("app", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_APP(fmt, ...)
#endif

#ifdef ENABLE_CART_IO_LOGGING
#define LOG_CART_IO(fmt, ...) LOG("cart", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_CART_IO(fmt, ...)
#endif

#ifdef ENABLE_CART_DATA_LOGGING
#define LOG_CART_DATA(fmt, ...) LOG("data", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_CART_DATA(fmt, ...)
#endif

#ifdef ENABLE_ROM_LOGGING
#define LOG_ROM(fmt, ...) LOG("rom", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_ROM(fmt, ...)
#endif

#ifdef ENABLE_IDE_LOGGING
#define LOG_IDE(fmt, ...) LOG("ide", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_IDE(fmt, ...)
#endif

#ifdef ENABLE_FS_LOGGING
#define LOG_FS(fmt, ...) LOG("fs", fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define LOG_FS(fmt, ...)
#endif
