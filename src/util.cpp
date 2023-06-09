
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ps1/registers.h"
#include "ps1/system.h"
#include "util.hpp"

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

/* Logger */

// Global state, I know, but it's a necessary evil.
Logger logger;

Logger::Logger(void)
: _tail(0), enableSyslog(false) {
	clear();
}

void Logger::clear(void) {
	for (int i = 0; i < MAX_LOG_LINES; i++)
		_lines[i][0] = 0;
}

void Logger::log(const char *format, ...) {
	auto mask = setInterruptMask(0);

	size_t  tail = _tail;
	va_list ap;

	_tail = size_t(tail + 1) % MAX_LOG_LINES;

	va_start(ap, format);
	vsnprintf(_lines[tail], MAX_LOG_LINE_LENGTH, format, ap);
	va_end(ap);

	if (enableSyslog)
		puts(_lines[tail]);
	if (mask)
		setInterruptMask(mask);
}

/* CRC calculation */

static constexpr uint16_t _CRC8_POLY  = 0x8c;
static constexpr uint16_t _CRC16_POLY = 0x1021;

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
			if (temp & 0x8000)
				crc ^= _CRC16_POLY;
		}
	}

	return (crc ^ 0xffff) & 0xffff;
}

/* String manipulation */

static const char _HEX_CHARSET[] = "0123456789ABCDEF";

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
	uint32_t value = input[0] | (input[1] << 8) | (input[2] << 16) | (input[3] << 24);

	return sprintf(output, "%04d-%04d", (value / 10000) % 10000, value % 10000);
}

// This format is used by Konami's tools to display trace IDs in the TID_81
// format.
static const char _TRACE_ID_CHECKSUM_CHARSET[] = "0X987654321";

size_t traceIDToString(char *output, const uint8_t *input) {
	uint16_t high = (input[0] << 8) | input[1];
	uint32_t low  = (input[2] << 24) | (input[3] << 16) | (input[4] << 8) | input[5];

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
static const char _BASE41_CHARSET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:";

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

}
