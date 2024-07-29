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
#include <stdio.h>
#include "common/util/string.hpp"

namespace util {

/* String manipulation */

const char HEX_CHARSET[]{ "0123456789ABCDEF" };
const char BASE41_CHARSET[]{ "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:" };

size_t hexValueToString(char *output, uint32_t value, size_t numDigits) {
	output += numDigits;
	*output = 0;

	for (size_t i = numDigits; i; i--, value >>= 4)
		*(--output) = HEX_CHARSET[value & 0xf];

	return numDigits;
}

size_t hexToString(
	char *output, const uint8_t *input, size_t length, char separator
) {
	size_t outLength = 0;

	for (; length; length--) {
		uint8_t value = *(input++);

		*(output++) = HEX_CHARSET[value >> 4];
		*(output++) = HEX_CHARSET[value & 0xf];

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
size_t encodeBase41(char *output, const uint8_t *input, size_t length) {
	size_t outLength = 0;

	for (int i = length + 1; i > 0; i -= 2) {
		int value = *(input++) << 8;
		value    |= *(input++);

		*(output++) = BASE41_CHARSET[value % 41];
		*(output++) = BASE41_CHARSET[(value / 41) % 41];
		*(output++) = BASE41_CHARSET[value / 1681];
		outLength  += 3;
	}

	*output = 0;
	return outLength;
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

}
