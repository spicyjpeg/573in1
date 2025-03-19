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
#include "common/util/templates.hpp"
#include "common/gpu.hpp"
#include "ps1/gpucmd.h"
#include "ps1/registers.h"

namespace mdec {

static constexpr int    MACROBLOCK_SIZE         = 16;
static constexpr size_t MACROBLOCK_LENGTH_16BPP =
	MACROBLOCK_SIZE * MACROBLOCK_SIZE * 2;
static constexpr size_t MACROBLOCK_LENGTH_24BPP =
	MACROBLOCK_SIZE * MACROBLOCK_SIZE * 3;

/* Basic API */

void init(void);
size_t feed(const void *data, size_t length, bool wait = false);
size_t receive(void *data, size_t length, bool wait = false);

static inline bool isIdle(void) {
	return (
		!(DMA_CHCR(DMA_MDEC_IN) & DMA_CHCR_ENABLE) &&
		(MDEC1 & MDEC_STAT_BUSY)
	);
}

static inline size_t feedDecodedBS(
	const uint32_t *data,
	uint32_t       flags,
	bool           wait = false
) {
	size_t length = data[0] & MDEC_CMD_LENGTH_BITMASK;

	MDEC0 = MDEC_CMD_OP_DECODE | length | flags;
	return feed(&data[1], length, wait);
}

/* Asynchronous MDEC-to-VRAM image uploader */

class VRAMUploader {
private:
	util::Data _buffers[2];
	int        _currentBuffer;

	int16_t     _frameWidth, _boundaryX;
	gpu::RectWH _currentSlice;

public:
	VRAMUploader(
		int           width,
		int           height,
		GP1ColorDepth colorDepth = GP1_COLOR_16BPP
	);

	void newFrame(int x, int y);
	bool poll(void);
};

/* MDEC bitstream decompressor */

class BSHeader {
public:
	uint16_t outputLength;
	uint16_t mdecCommand;
	uint16_t quantScale;
	uint16_t version;

	inline size_t getUncompLength(void) const {
		// DMA feeds data to the MDEC in 32-word chunks so the uncompressed
		// length has to be rounded accordingly. Additionally, the decompressor
		// generates a 4-byte header containing the command to send to the MDEC.
		return (outputLength + 4 + 127) / 128;
	}
};

enum BSDecompressorError {
	NO_ERROR     = 0,
	PARTIAL_DATA = 1,
	DECODE_ERROR = 2
};

class BSDecompressor;

extern "C" BSDecompressorError _bsDecompressorStart(
	BSDecompressor *_this,
	uint32_t       *output,
	size_t         outputLength,
	const void     *input
);
extern "C" BSDecompressorError _bsDecompressorResume(
	BSDecompressor *_this,
	uint32_t       *output,
	size_t         outputLength
);

class BSDecompressor {
protected:
	const void *_input;

	uint32_t _bits, _nextBits;
	size_t   _remaining;

	uint8_t _isV3;
	int8_t  _bitOffset, _blockIndex, _coeffIndex;

	uint16_t _quantScale;
	int16_t  _lastY, _lastCr, _lastCb;

public:
	inline BSDecompressorError decompress(
		uint32_t   *output,
		const void *input,
		size_t     outputLength
	) {
		return _bsDecompressorStart(this, output, outputLength, input);
	}
	inline BSDecompressorError resume(uint32_t *output, size_t outputLength) {
		return _bsDecompressorResume(this, output, outputLength);
	}
};

void initBSHuffmanTable(void);

}
