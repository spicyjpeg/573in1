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
#include "ps1/registers.h"

namespace mdec {

/* Basic API */

void init(void);
size_t feed(const void *data, size_t length, bool wait);

static inline bool isIdle(void) {
	return !(MDEC1 & MDEC_STAT_BUSY);
}

static inline size_t feedBS(const uint32_t *data, uint32_t flags, bool wait) {
	size_t length = data[0] & MDEC_CMD_FLAG_LENGTH_BITMASK;

	MDEC0 = MDEC_CMD_DECODE | length | flags;
	return feed(&data[1], length, wait);
}

/* MDEC bitstream decompressor */

struct BSHeader {
public:
	uint16_t outputLength;
	uint16_t mdecCommand;
	uint16_t quantScale;
	uint16_t version;
};

enum BSDecompressorError {
	NO_ERROR     = 0,
	PARTIAL_DATA = 1,
	DECODE_ERROR = 2
};

extern "C" BSDecompressorError _bsDecompressorStart(
	void *_this, uint32_t *output, size_t outputLength, const uint32_t *input
);
extern "C" BSDecompressorError _bsDecompressorResume(
	void *_this, uint32_t *output, size_t outputLength
);

class BSDecompressor {
protected:
	const uint32_t *_input;

	uint32_t _bits, _nextBits;
	size_t   _remaining;

	uint8_t _isV3;
	int8_t  _bitOffset, _blockIndex, _coeffIndex;

	uint16_t _quantScale;
	int16_t  _lastY, _lastCr, _lastCb;

public:
	inline BSDecompressorError decompress(
		uint32_t *output, const uint32_t *input, size_t outputLength
	) {
		return _bsDecompressorStart(this, output, outputLength, input);
	}
	inline BSDecompressorError resume(uint32_t *output, size_t outputLength) {
		return _bsDecompressorResume(this, output, outputLength);
	}
};

void initBSHuffmanTable(void);

}
