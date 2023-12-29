
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
	for (auto line : _lines)
		line[0] = 0;
}

void Logger::log(const char *format, ...) {
	auto enable = disableInterrupts();

	size_t  tail = _tail;
	va_list ap;

	_tail = size_t(tail + 1) % MAX_LOG_LINES;

	va_start(ap, format);
	vsnprintf(_lines[tail], MAX_LOG_LINE_LENGTH, format, ap);
	va_end(ap);

	if (enableSyslog)
		puts(_lines[tail]);
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

/* PS1 executable header */

bool ExecutableHeader::validateMagic(void) const {
	return (hash(magic, sizeof(magic)) == "PS-X EXE"_h);
}

/* Error strings */

const char *const CART_DRIVER_ERROR_NAMES[]{
	"NO_ERROR", // = 0
	"UNSUPPORTED_OP",
	"DS2401_NO_RESP",
	"DS2401_ID_ERROR",
	"X76_NACK",
	"X76_POLL_FAIL",
	"X76_VERIFY_FAIL",
	"ZS01_NACK",
	"ZS01_ERROR",
	"ZS01_CRC_MISMATCH"
};

const char *const IDE_DEVICE_ERROR_NAMES[]{
	"NO_ERROR", // = 0
	"UNSUPPORTED_OP",
	"STATUS_TIMEOUT",
	"DRIVE_ERROR",
	"INCOMPLETE_DATA",
	"CHECKSUM_MISMATCH"
};

const char *const FATFS_ERROR_NAMES[]{
	"OK", // = 0
	"DISK_ERR",
	"INT_ERR",
	"NOT_READY",
	"NO_FILE",
	"NO_PATH",
	"INVALID_NAME",
	"DENIED",
	"EXIST",
	"INVALID_OBJECT",
	"WRITE_PROTECTED",
	"INVALID_DRIVE",
	"NOT_ENABLED",
	"NO_FILESYSTEM",
	"MKFS_ABORTED",
	"TIMEOUT",
	"LOCKED",
	"NOT_ENOUGH_CORE",
	"TOO_MANY_OPEN_FILES",
	"INVALID_PARAMETER"
};

const char *const MINIZ_ERROR_NAMES[]{
	"VERSION_ERROR",
	"BUF_ERROR",
	"MEM_ERROR",
	"DATA_ERROR",
	"STREAM_ERROR",
	"ERRNO",
	"OK", // = 0
	"STREAM_END",
	"NEED_DICT"
};

const char *const MINIZ_ZIP_ERROR_NAMES[]{
	"NO_ERROR", // = 0
	"UNDEFINED_ERROR",
	"TOO_MANY_FILES",
	"FILE_TOO_LARGE",
	"UNSUPPORTED_METHOD",
	"UNSUPPORTED_ENCRYPTION",
	"UNSUPPORTED_FEATURE",
	"FAILED_FINDING_CENTRAL_DIR",
	"NOT_AN_ARCHIVE",
	"INVALID_HEADER_OR_CORRUPTED",
	"UNSUPPORTED_MULTIDISK",
	"DECOMPRESSION_FAILED",
	"COMPRESSION_FAILED",
	"UNEXPECTED_DECOMPRESSED_SIZE",
	"CRC_CHECK_FAILED",
	"UNSUPPORTED_CDIR_SIZE",
	"ALLOC_FAILED",
	"FILE_OPEN_FAILED",
	"FILE_CREATE_FAILED",
	"FILE_WRITE_FAILED",
	"FILE_READ_FAILED",
	"FILE_CLOSE_FAILED",
	"FILE_SEEK_FAILED",
	"FILE_STAT_FAILED",
	"INVALID_PARAMETER",
	"INVALID_FILENAME",
	"BUF_TOO_SMALL",
	"INTERNAL_ERROR",
	"FILE_NOT_FOUND",
	"ARCHIVE_TOO_LARGE",
	"VALIDATION_FAILED",
	"WRITE_CALLBACK_FAILED"
};

}
