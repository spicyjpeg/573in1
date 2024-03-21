
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

size_t hexToString(char *output, const uint8_t *input, size_t length, char sep) {
	size_t outLength = 0;

	for (; length; length--) {
		uint8_t value = *(input++);

		*(output++) = _HEX_CHARSET[value >> 4];
		*(output++) = _HEX_CHARSET[value & 0xf];

		if (sep && (length > 1)) {
			*(output++) = sep;
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

	_argListPtr      = reinterpret_cast<char **>(uintptr_t(stackTop) & ~7)
		- MAX_EXECUTABLE_ARGS;
	_currentStackPtr = reinterpret_cast<char *>(_argListPtr);
}

void ExecutableLoader::addArgument(const char *arg) {
	// Command-line arguments must be copied to the top of the new stack in
	// order to ensure the executable is going to be able to access them at any
	// time.
	size_t length  = __builtin_strlen(arg) + 1;
	size_t aligned = (length + 7) & ~7;

	_currentStackPtr        -= aligned;
	_argListPtr[_argCount++] = _currentStackPtr;

	__builtin_memcpy(_currentStackPtr, arg, length);
	//assert(_argCount <= MAX_EXECUTABLE_ARGS);
}

[[noreturn]] void ExecutableLoader::run(void) {
	disableInterrupts();
	flushCache();

	register int       a0   __asm__("a0") = _argCount;
	register char      **a1 __asm__("a1") = _argListPtr;
	register uintptr_t gp   __asm__("gp") = _header.initialGP;

	// Changing the stack pointer and return address is not something that
	// should be done in a C++ function, but hopefully it's fine here since
	// we're jumping out right after setting it.
	__asm__ volatile(
		".set noreorder\n"
		"li    $ra, %0\n"
		"jr    %1\n"
		"addiu $sp, %2, -8\n"
		".set reorder\n"
		:: "i"(DEV2_BASE), "r"(_header.entryPoint), "r"(_currentStackPtr),
		"r"(a0), "r"(a1), "r"(gp)
	);
	__builtin_unreachable();
}

}
