
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "common/util.hpp"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace util {

/* String hashing */

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

/* Date and time class */

bool Date::isValid(void) const {
	if ((hour > 23) || (minute > 59) || (second > 59))
		return false;
	if ((month < 1) || (month > 12))
		return false;
	if ((day < 1) || (day > getMonthDayCount()))
		return false;

	return true;
}

bool Date::isLeapYear(void) const {
	if (year % 4)
		return false;
	if (!(year % 100) && (year % 400))
		return false;

	return true;
}

int Date::getDayOfWeek(void) const {
	// See https://datatracker.ietf.org/doc/html/rfc3339#appendix-B
	int _year = year, _month = month - 2;

	if (_month <= 0) {
		_month += 12;
		_year--;
	}

	int century = _year / 100;
	_year      %= 100;

	int weekday = 0
		+ day
		+ (_month * 26 - 2) / 10
		+ _year
		+ _year / 4
		+ century / 4
		+ century * 5;

	return weekday % 7;
}

int Date::getMonthDayCount(void) const {
	switch (month) {
		case 2:
			return isLeapYear() ? 29 : 28;

		case 4:
		case 6:
		case 9:
		case 11:
			return 30;

		default:
			return 31;
	}
}

uint32_t Date::toDOSTime(void) const {
	int _year = year + 2000 - 1980;

	if ((_year < 0) || (_year > 127))
		return 0;

	return 0
		| (_year  << 25)
		| (month  << 21)
		| (day    << 16)
		| (hour   << 11)
		| (minute <<  5)
		| (second >>  1);
}

size_t Date::toString(char *output) const {
	return sprintf(
		output, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute,
		second
	);
}

/* Tween/animation classes */

template<typename T, typename E> void Tween<T, E>::setValue(
	int time, T start, T target, int duration
) {
	//assert(duration <= 0x800);

	_base  = start;
	_delta = target - start;

	_endTime   = time + duration;
	_timeScale = TWEEN_UNIT / duration;
}

template<typename T, typename E> void Tween<T, E>::setValue(T target) {
	_base  = target;
	_delta = static_cast<T>(0);

	_endTime = 0;
}

template<typename T, typename E> T Tween<T, E>::getValue(int time) const {
	int remaining = time - _endTime;

	if (remaining >= 0)
		return _base + _delta;

	return _base + (
		_delta * E::apply(remaining * _timeScale + TWEEN_UNIT)
	) / TWEEN_UNIT;
}

template class Tween<int, LinearEasing>;
template class Tween<int, QuadInEasing>;
template class Tween<int, QuadOutEasing>;
template class Tween<uint16_t, LinearEasing>;
template class Tween<uint16_t, QuadInEasing>;
template class Tween<uint16_t, QuadOutEasing>;
template class Tween<uint32_t, LinearEasing>;
template class Tween<uint32_t, QuadInEasing>;
template class Tween<uint32_t, QuadOutEasing>;

/* Logging framework */

// Global state, I know, but it's a necessary evil.
Logger logger;

void LogBuffer::clear(void) {
	for (auto line : _lines)
		line[0] = 0;
}

char *LogBuffer::allocateLine(void) {
	size_t tail = _tail;
	_tail       = (tail + 1) % MAX_LOG_LINES;

	return _lines[tail];
}

void Logger::setLogBuffer(LogBuffer *buffer) {
	auto enable = disableInterrupts();
	_buffer     = buffer;

	if (enable)
		enableInterrupts();
}

void Logger::setupSyslog(int baudRate) {
	auto enable = disableInterrupts();

	if (baudRate) {
		initSerialIO(baudRate);
		_enableSyslog = true;
	} else {
		_enableSyslog = false;
	}

	if (enable)
		enableInterrupts();
}

void Logger::log(const char *format, ...) {
	auto    enable = disableInterrupts();
	va_list ap;

	if (_buffer) {
		auto line = _buffer->allocateLine();

		va_start(ap, format);
		vsnprintf(line, MAX_LOG_LINE_LENGTH, format, ap);
		va_end(ap);

		if (_enableSyslog)
			puts(line);
	} else if (_enableSyslog) {
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);

		putchar('\n');
	}

	if (enable)
		enableInterrupts();
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

/* LZ4 decompressor */

void decompressLZ4(
	uint8_t *output, const uint8_t *input, size_t maxOutputLength,
	size_t inputLength
) {
	auto outputEnd = &output[maxOutputLength];
	auto inputEnd  = &input[inputLength];

	while (input < inputEnd) {
		uint8_t token = *(input++);

		// Copy literals from the input stream.
		int literalLength = token >> 4;

		if (literalLength == 0xf) {
			uint8_t addend;

			do {
				addend         = *(input++);
				literalLength += addend;
			} while (addend == 0xff);
		}

		for (; literalLength && (output < outputEnd); literalLength--)
			*(output++) = *(input++);
		if (input >= inputEnd)
			break;

		int offset = input[0] | (input[1] << 8);
		input     += 2;

		// Copy from previously decompressed data. Note that this *must* be done
		// one byte at a time, as the compressor relies on out-of-bounds copies
		// repeating the last byte.
		int copyLength = token & 0xf;

		if (copyLength == 0xf) {
			uint8_t addend;

			do {
				addend      = *(input++);
				copyLength += addend;
			} while (addend == 0xff);
		}

		auto copySource = output - offset;
		copyLength     += 4;

		for (; copyLength && (output < outputEnd); copyLength--)
			*(output++) = *(copySource++);
	}
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

/* String manipulation */

static const char _HEX_CHARSET[]{ "0123456789ABCDEF" };

size_t hexValueToString(char *output, uint32_t value, size_t numDigits) {
	output += numDigits;
	*output = 0;

	for (size_t i = numDigits; i; i--, value >>= 4)
		*(--output) = _HEX_CHARSET[value & 0xf];

	return numDigits;
}

size_t hexToString(
	char *output, const uint8_t *input, size_t length, char separator
) {
	size_t outLength = 0;

	for (; length; length--) {
		uint8_t value = *(input++);

		*(output++) = _HEX_CHARSET[value >> 4];
		*(output++) = _HEX_CHARSET[value & 0xf];

		if (separator && (length > 1)) {
			*(output++) = separator;
			outLength  += 3;
		} else {
			outLength  += 2;
		}
	}

	*output = 0;
	return outLength;
}

size_t serialNumberToString(char *output, const uint8_t *input) {
	uint32_t value =
		input[0] | (input[1] << 8) | (input[2] << 16) | (input[3] << 24);

	return sprintf(output, "%04d-%04d", (value / 10000) % 10000, value % 10000);
}

// This format is used by Konami's tools to display trace IDs in the TID_81
// format.
static const char _TRACE_ID_CHECKSUM_CHARSET[]{ "0X987654321" };

size_t traceIDToString(char *output, const uint8_t *input) {
	uint16_t high = (input[0] << 8) | input[1];
	uint32_t low  =
		(input[2] << 24) | (input[3] << 16) | (input[4] << 8) | input[5];

	size_t length = sprintf(&output[1], "%02d-%04d", high % 100, low % 10000);

	// The checksum is calculated in a very weird way:
	//   code     = AB-CDEF
	//   checksum = (A*7 + B*6 + C*5 + D*4 + E*3 + F*2) % 11
	int checksum = 0, multiplier = 7;

	for (const char *ptr = &output[1]; *ptr; ptr++) {
		if (*ptr != '-')
			checksum += (*ptr - '0') * (multiplier--);
	}

	output[0] = _TRACE_ID_CHECKSUM_CHARSET[checksum % 11];
	return length + 1;
}

// This encoding is similar to standard base45, but with some problematic
// characters (' ', '$', '%', '*') excluded.
static const char _BASE41_CHARSET[]{ "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:" };

size_t encodeBase41(char *output, const uint8_t *input, size_t length) {
	size_t outLength = 0;

	for (int i = length + 1; i > 0; i -= 2) {
		int value = *(input++) << 8;
		value    |= *(input++);

		*(output++) = _BASE41_CHARSET[value % 41];
		*(output++) = _BASE41_CHARSET[(value / 41) % 41];
		*(output++) = _BASE41_CHARSET[value / 1681];
		outLength  += 3;
	}

	*output = 0;
	return outLength;
}

/* PS1 executable loader */

bool ExecutableHeader::validateMagic(void) const {
	return (hash(magic, sizeof(magic)) == "PS-X EXE"_h);
}

ExecutableLoader::ExecutableLoader(
	const ExecutableHeader &header, void *defaultStackTop
) : _header(header), _argCount(0) {
	auto stackTop = header.getStackPtr();

	if (!stackTop)
		stackTop = defaultStackTop;

	_argListPtr      = reinterpret_cast<const char **>(uintptr_t(stackTop) & ~7)
		- MAX_EXECUTABLE_ARGS;
	_currentStackPtr = reinterpret_cast<char *>(_argListPtr);
}

void ExecutableLoader::copyArgument(const char *arg) {
	// Command-line arguments must be copied to the top of the new stack in
	// order to ensure the executable is going to be able to access them at any
	// time.
	size_t length     = __builtin_strlen(arg) + 1;
	_currentStackPtr -= (length + 7) & ~7;

	addArgument(_currentStackPtr);
	__builtin_memcpy(_currentStackPtr, arg, length);
	//assert(_argCount <= MAX_EXECUTABLE_ARGS);
}

[[noreturn]] void ExecutableLoader::run(
	int rawArgc, const char *const *rawArgv
) {
	disableInterrupts();
	flushCache();

	register int               a0  __asm__("a0") = rawArgc;
	register const char *const *a1 __asm__("a1") = rawArgv;
	register uintptr_t         gp  __asm__("gp") = _header.initialGP;

	// Changing the stack pointer and return address is not something that
	// should be done in a C++ function, but hopefully it's fine here since
	// we're jumping out right after setting it.
	__asm__ volatile(
		".set push\n"
		".set noreorder\n"
		"li    $ra, %0\n"
		"jr    %1\n"
		"addiu $sp, %2, -8\n"
		".set pop\n"
		:: "i"(DEV2_BASE), "r"(_header.entryPoint), "r"(_currentStackPtr),
		"r"(a0), "r"(a1), "r"(gp)
	);
	__builtin_unreachable();
}

}
