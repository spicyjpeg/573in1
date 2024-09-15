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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "common/util/templates.hpp"
#include "common/mdec.hpp"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace mdec {

/* IDCT matrix and quantization table */

#define SF0 0x5a82 // cos(0/16 * pi) * sqrt(2)
#define SF1 0x7d8a // cos(1/16 * pi) * 2
#define SF2 0x7641 // cos(2/16 * pi) * 2
#define SF3 0x6a6d // cos(3/16 * pi) * 2
#define SF4 0x5a82 // cos(4/16 * pi) * 2
#define SF5 0x471c // cos(5/16 * pi) * 2
#define SF6 0x30fb // cos(6/16 * pi) * 2
#define SF7 0x18f8 // cos(7/16 * pi) * 2

static const int16_t _IDCT_TABLE[]{
	SF0,  SF0,  SF0,  SF0,  SF0,  SF0,  SF0,  SF0,
	SF1,  SF3,  SF5,  SF7, -SF7, -SF5, -SF3, -SF1,
	SF2,  SF6, -SF6, -SF2, -SF2, -SF6,  SF6,  SF2,
	SF3, -SF7, -SF1, -SF5,  SF5,  SF1,  SF7, -SF3,
	SF4, -SF4, -SF4,  SF4,  SF4, -SF4, -SF4,  SF4,
	SF5, -SF1,  SF7,  SF3, -SF3, -SF7,  SF1, -SF5,
	SF6, -SF2,  SF2, -SF6, -SF6,  SF2, -SF2,  SF6,
	SF7, -SF5,  SF3, -SF1,  SF1, -SF3,  SF5, -SF7
};

// The BS v2/v3 quantization table is based on the MPEG-1 table, with the only
// difference being the DC coefficient (2 instead of 8). Quantization tables are
// stored in zigzag order, rather than row- or column-major.
static const uint8_t _BS_QUANT_TABLE[]{
	 2, 16, 16, 19, 16, 19, 22, 22,
	22, 22, 22, 22, 26, 24, 26, 27,
	27, 27, 26, 26, 26, 26, 27, 27,
	27, 29, 29, 29, 34, 34, 34, 29,
	29, 29, 27, 27, 29, 29, 32, 32,
	34, 34, 37, 38, 37, 35, 35, 34,
	35, 38, 38, 40, 40, 40, 48, 48,
	46, 46, 56, 56, 58, 69, 69, 83
};

/* Basic API */

static constexpr int _DMA_CHUNK_SIZE = 32;
static constexpr int _DMA_TIMEOUT    = 100000;

void init(void) {
	MDEC1 = MDEC_CTRL_RESET;
	MDEC1 = MDEC_CTRL_DMA_OUT | MDEC_CTRL_DMA_IN;

	MDEC0 = MDEC_CMD_SET_IDCT_TABLE;
	feed(_IDCT_TABLE, sizeof(_IDCT_TABLE), true);

	MDEC0 = MDEC_CMD_SET_QUANT_TABLE | MDEC_CMD_FLAG_USE_CHROMA;
	feed(_BS_QUANT_TABLE, sizeof(_BS_QUANT_TABLE), true);
	feed(_BS_QUANT_TABLE, sizeof(_BS_QUANT_TABLE), true);
}

size_t feed(const void *data, size_t length, bool wait) {
	length /= 4;

	util::assertAligned<uint32_t>(data);
#if 0
	assert(!(length % _DMA_CHUNK_SIZE));
#else
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;
#endif

	if (!waitForDMATransfer(DMA_MDEC_IN, _DMA_TIMEOUT))
		return 0;

	DMA_MADR(DMA_MDEC_IN) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_MDEC_IN) = util::concat4(_DMA_CHUNK_SIZE, length);
	DMA_CHCR(DMA_MDEC_IN) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_SLICE
		| DMA_CHCR_ENABLE;

	if (wait)
		waitForDMATransfer(DMA_MDEC_IN, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

/* MDEC bitstream decompressor */

static constexpr inline uint8_t dc(int luma, int chroma) {
	return (chroma & 15) | (luma << 4);
}
static constexpr inline uint16_t ac(int rl, int coeff) {
	return (coeff & 0x3ff) | (rl << 10);
}
static constexpr inline uint32_t acl(int rl, int coeff, int length) {
	return (coeff & 0x3ff) | (rl << 10) | (length << 16);
}

#define DC2(luma, chroma)  dc  (luma, chroma), dc  (luma, chroma)
#define DC4(luma, chroma)  DC2 (luma, chroma), DC2 (luma, chroma)
#define DC8(luma, chroma)  DC4 (luma, chroma), DC4 (luma, chroma)
#define DC16(luma, chroma) DC8 (luma, chroma), DC8 (luma, chroma)
#define DC32(luma, chroma) DC16(luma, chroma), DC16(luma, chroma)

#define AC2(rl, coeff)   ac (rl, coeff), ac (rl, coeff)
#define AC4(rl, coeff)   AC2(rl, coeff), AC2(rl, coeff)
#define AC8(rl, coeff)   AC4(rl, coeff), AC4(rl, coeff)
#define PAIR(rl, coeff)  ac (rl, coeff), ac (rl, -(coeff))
#define PAIR2(rl, coeff) AC2(rl, coeff), AC2(rl, -(coeff))
#define PAIR4(rl, coeff) AC4(rl, coeff), AC4(rl, -(coeff))
#define PAIR8(rl, coeff) AC8(rl, coeff), AC8(rl, -(coeff))

#define ACL2(rl, coeff, length)   acl (rl, coeff, length), acl (rl, coeff, length)
#define ACL4(rl, coeff, length)   ACL2(rl, coeff, length), ACL2(rl, coeff, length)
#define ACL8(rl, coeff, length)   ACL4(rl, coeff, length), ACL4(rl, coeff, length)
#define PAIRL(rl, coeff, length)  acl (rl, coeff, length), acl (rl, -(coeff), length)
#define PAIRL2(rl, coeff, length) ACL2(rl, coeff, length), ACL2(rl, -(coeff), length)
#define PAIRL4(rl, coeff, length) ACL4(rl, coeff, length), ACL4(rl, -(coeff), length)
#define PAIRL8(rl, coeff, length) ACL8(rl, coeff, length), ACL8(rl, -(coeff), length)

struct BSHuffmanTable {
public:
	uint16_t ac0[2];
	uint32_t ac2[8],  ac3 [64];
	uint16_t ac4[8],  ac5 [8],  ac7 [16], ac8 [32];
	uint16_t ac9[32], ac10[32], ac11[32], ac12[32];

	uint8_t dcValues[128], dcLengths[9];
};

static const BSHuffmanTable _HUFFMAN_TABLE{
	.ac0 = {
		PAIR( 0,  1) // 11x
	},
	.ac2 = {
		PAIRL ( 0, 2, 5), PAIRL ( 2, 1, 5), // 010xx
		PAIRL2( 1, 1, 4)                    // 011x-
	},
	.ac3 = {
		// 00100xxxx
		PAIRL (13, 1, 9), PAIRL ( 0, 6, 9), PAIRL (12, 1, 9), PAIRL (11, 1, 9),
		PAIRL ( 3, 2, 9), PAIRL ( 1, 3, 9), PAIRL ( 0, 5, 9), PAIRL (10, 1, 9),
		// 001xxx---
		PAIRL8( 0, 3, 6), PAIRL8( 4, 1, 6), PAIRL8( 3, 1, 6)
	},
	.ac4 = {
		// 0001xxx
		PAIR( 7,  1), PAIR( 6,  1), PAIR( 1,  2), PAIR( 5,  1)
	},
	.ac5 = {
		// 00001xxx
		PAIR( 2,  2), PAIR( 9,  1), PAIR( 0,  4), PAIR( 8,  1)
	},
	.ac7 = {
		// 0000001xxxx
		PAIR(16,  1), PAIR( 5,  2), PAIR( 0,  7), PAIR( 2,  3),
		PAIR( 1,  4), PAIR(15,  1), PAIR(14,  1), PAIR( 4,  2)
	},
	.ac8 = {
		// 00000001xxxxx
		PAIR( 0, 11), PAIR( 8,  2), PAIR( 4,  3), PAIR( 0, 10),
		PAIR( 2,  4), PAIR( 7,  2), PAIR(21,  1), PAIR(20,  1),
		PAIR( 0,  9), PAIR(19,  1), PAIR(18,  1), PAIR( 1,  5),
		PAIR( 3,  3), PAIR( 0,  8), PAIR( 6,  2), PAIR(17,  1)
	},
	.ac9 = {
		// 000000001xxxxx
		PAIR(10,  2), PAIR( 9,  2), PAIR( 5,  3), PAIR( 3,  4),
		PAIR( 2,  5), PAIR( 1,  7), PAIR( 1,  6), PAIR( 0, 15),
		PAIR( 0, 14), PAIR( 0, 13), PAIR( 0, 12), PAIR(26,  1),
		PAIR(25,  1), PAIR(24,  1), PAIR(23,  1), PAIR(22,  1)
	},
	.ac10 = {
		// 0000000001xxxxx
		PAIR( 0, 31), PAIR( 0, 30), PAIR( 0, 29), PAIR( 0, 28),
		PAIR( 0, 27), PAIR( 0, 26), PAIR( 0, 25), PAIR( 0, 24),
		PAIR( 0, 23), PAIR( 0, 22), PAIR( 0, 21), PAIR( 0, 20),
		PAIR( 0, 19), PAIR( 0, 18), PAIR( 0, 17), PAIR( 0, 16)
	},
	.ac11 = {
		// 00000000001xxxxx
		PAIR( 0, 40), PAIR( 0, 39), PAIR( 0, 38), PAIR( 0, 37),
		PAIR( 0, 36), PAIR( 0, 35), PAIR( 0, 34), PAIR( 0, 33),
		PAIR( 0, 32), PAIR( 1, 14), PAIR( 1, 13), PAIR( 1, 12),
		PAIR( 1, 11), PAIR( 1, 10), PAIR( 1,  9), PAIR( 1,  8)
	},
	.ac12 = {
		// 000000000001xxxxx
		PAIR( 1, 18), PAIR( 1, 17), PAIR( 1, 16), PAIR( 1, 15),
		PAIR( 6,  3), PAIR(16,  2), PAIR(15,  2), PAIR(14,  2),
		PAIR(13,  2), PAIR(12,  2), PAIR(11,  2), PAIR(31,  1),
		PAIR(30,  1), PAIR(29,  1), PAIR(28,  1), PAIR(27,  1)
	},
	.dcValues = {
		DC32(1, 0), // 00-----
		DC32(2, 1), // 01-----
		DC16(0, 2), // 100----
		DC16(3, 2), // 101----
		DC16(4, 3), // 110----
		DC8 (5, 4), // 1110---
		DC4 (6, 5), // 11110--
		DC2 (7, 6), // 111110-
		dc  (8, 7), // 1111110
		dc  (0, 8)  // 1111111(0)
	},
	.dcLengths = {
		dc(3, 2),
		dc(2, 2),
		dc(2, 2),
		dc(3, 3),
		dc(3, 4),
		dc(4, 5),
		dc(5, 6),
		dc(6, 7),
		dc(7, 8)
	}
};

void initBSHuffmanTable(void) {
	auto table = reinterpret_cast<BSHuffmanTable *>(CACHE_BASE);

	__builtin_memcpy(table, &_HUFFMAN_TABLE, sizeof(BSHuffmanTable));
}

}
