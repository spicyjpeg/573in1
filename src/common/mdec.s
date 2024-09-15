# 573in1 - Copyright (C) 2022-2024 spicyjpeg
#
# 573in1 is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# 573in1. If not, see <https://www.gnu.org/licenses/>.

.set noreorder
.set noat

.set GTE_LZCS, $30 # Leading zero count input
.set GTE_LZCR, $31 # Leading zero count output

.set CACHE_BASE, 0x9f800000

## Structure definitions

.set BSHeader_outputLength, 0
.set BSHeader_mdecCommand,  BSHeader_outputLength + 2
.set BSHeader_quantScale,   BSHeader_mdecCommand  + 2
.set BSHeader_version,      BSHeader_quantScale   + 2
.set BSHeader_data0,        BSHeader_version      + 2
.set BSHeader_data1,        BSHeader_data0        + 4

.set BSDecompressor_input,      0
.set BSDecompressor_bits,       BSDecompressor_input      + 4
.set BSDecompressor_nextBits,   BSDecompressor_bits       + 4
.set BSDecompressor_remaining,  BSDecompressor_nextBits   + 4
.set BSDecompressor_isBSv3,     BSDecompressor_remaining  + 4
.set BSDecompressor_bitOffset,  BSDecompressor_isBSv3     + 1
.set BSDecompressor_blockIndex, BSDecompressor_bitOffset  + 1
.set BSDecompressor_coeffIndex, BSDecompressor_blockIndex + 1
.set BSDecompressor_quantScale, BSDecompressor_coeffIndex + 1
.set BSDecompressor_lastY,      BSDecompressor_quantScale + 2
.set BSDecompressor_lastCr,     BSDecompressor_lastY      + 2
.set BSDecompressor_lastCb,     BSDecompressor_lastCr     + 2

.set BSHuffmanTable_ac0,       0
.set BSHuffmanTable_ac2,       BSHuffmanTable_ac0      +   2 * 2
.set BSHuffmanTable_ac3,       BSHuffmanTable_ac2      +   8 * 4
.set BSHuffmanTable_ac4,       BSHuffmanTable_ac3      +  64 * 4
.set BSHuffmanTable_ac5,       BSHuffmanTable_ac4      +   8 * 2
.set BSHuffmanTable_ac7,       BSHuffmanTable_ac5      +   8 * 2
.set BSHuffmanTable_ac8,       BSHuffmanTable_ac7      +  16 * 2
.set BSHuffmanTable_ac9,       BSHuffmanTable_ac8      +  32 * 2
.set BSHuffmanTable_ac10,      BSHuffmanTable_ac9      +  32 * 2
.set BSHuffmanTable_ac11,      BSHuffmanTable_ac10     +  32 * 2
.set BSHuffmanTable_ac12,      BSHuffmanTable_ac11     +  32 * 2
.set BSHuffmanTable_dcValues,  BSHuffmanTable_ac12     +  32 * 2
.set BSHuffmanTable_dcLengths, BSHuffmanTable_dcValues + 128 * 1

## Constants

.set NO_ERROR,     0
.set PARTIAL_DATA, 1
.set DECODE_ERROR, 2

.set BLOCK_INDEX_Y3, 0
.set BLOCK_INDEX_Y2, 1
.set BLOCK_INDEX_Y1, 2
.set BLOCK_INDEX_Y0, 3
.set BLOCK_INDEX_CB, 4
.set BLOCK_INDEX_CR, 5

.set MDEC_COMMAND,  0x3800
.set MDEC_END_CODE, 0xfe00

.set BS_DC_END_CODE, 0x1ff
.set BS_DC_BITMASK,  0x3ff

.set BS_DC_V2_LENGTH,     10
.set BS_DC_V3_LENGTH,      7
.set BS_DC_V3_END_LENGTH,  9

.set MAX_PREFIX_LENGTH, 11
.set JUMP_CASE_LENGTH,   5 # (1 << 5) = 32 bytes = 8 instructions

## Local variables and macros

.set value,  $v0
.set length, $v1

.set this,         $a0
.set output,       $a1
.set outputLength, $a2
.set input,        $a3

.set temp,      $t0
.set bits,      $t1
.set nextBits,  $t2
.set remaining, $t3
.set isBSv3,    $t4
.set bitOffset, $t5

.set blockIndex, $t6
.set coeffIndex, $t7
.set quantScale, $s0

.set lastY,  $s1
.set lastCr, $s2
.set lastCb, $s3

.set huffmanTable, $t8
.set acJumpPtr,    $t9

.macro swapHalfwords reg
	# reg = uint32_t(reg << 16) | uint32_t(reg >> 16);
	srl   temp, \reg, 16
	sll   \reg, 16
	or    \reg, temp
.endm

.macro decodeBSv3DC lastValueReg, jumpTo
	# bits     <<= temp;
	# bitOffset -= temp;
	sllv  bits, bits, temp
	beqz  length, 3f
	subu  bitOffset, temp

0: # if (length) {
	# value = bits >> (32 - length);
	subu  $at, length
	srlv  value, bits, $at

	# ((1 << length) - 1) below is computed as (0xffffffff >> (32 - length)) as
	# it takes fewer instructions.
	bltz  bits, 2f
	li    temp, -1

1:
	# if (!(bits >> 31)) value -= (1 << length) - 1;
	srlv  temp, temp, $at
	subu  value, temp

2:
	# lastValueReg += value * 4;
	# lastValueReg &= BS_DC_BITMASK;
	sll   value, 2
	addu  \lastValueReg, value
	andi  \lastValueReg, BS_DC_BITMASK

3: # } else {
	# *output = lastY | quantScale;
	or    temp, \lastValueReg, quantScale
	b     \jumpTo
	sh    temp, 0(output)
.endm # }

.macro decodeFixedAC prefixLength, codeLength, discarded, tableImm, jumpTo
	# bitmask = (1 << codeLength) - 1;
	# value   = (bits >> (32 - totalLength)) & bitmask;
	srl   value, bits, 32 - (\prefixLength + \codeLength + 1)
	andi  value, ((1 << \codeLength) - 1) << 1

	# An extra shift (to multiply the code by the size of each table entry, 2
	# bytes) is avoided here by keeping one more bit from the sliding window
	# above, then masking it off.
	addu  value, huffmanTable
	lhu   value, \tableImm(value)

	# bits     <<= totalLength;
	# bitOffset -= totalLength + discarded;
	sll   bits, \prefixLength + \codeLength
	addiu bitOffset, -(\prefixLength + \codeLength + \discarded)

	# *output = tableImm[value];
	b     \jumpTo
	sh    value, 0(output)
.endm

.macro decodeVariableAC prefixLength, codeLength, tableImm, jumpTo
	# Lookup tables with 32-bit entries are used to decode variable length
	# codes, with the upper 16 bits holding the length of the code.

	# bitmask = (1 << codeLength) - 1;
	# value   = (bits >> (32 - totalLength)) & bitmask;
	srl   value, bits, 32 - (\prefixLength + \codeLength + 2)
	andi  value, ((1 << \codeLength) - 1) << 2

	# An extra shift (to multiply the code by the size of each table entry, 4
	# bytes) is avoided here by keeping two more bits from the sliding window
	# above, then masking it off.
	addu  value, huffmanTable
	lw    value, \tableImm(value)

	# *output = tableImm[index] & 0xffff;
	# length  = tableImm[index] >> 16;
	b     \jumpTo
	sh    value, 0(output)
	nop
	nop
.endm

.macro decodeACEscape prefixLength, codeLength, discarded, jumpTo
	# shift = 32 - (prefixLength + codeLength);
	# value = bits >> shift;
	srl   value, bits, 32 - (\prefixLength + \codeLength)

	# bits     <<= totalLength;
	# bitOffset -= totalLength + discarded;
	sll   bits, \prefixLength + \codeLength
	addiu bitOffset, -(\prefixLength + \codeLength + \discarded)

	# *output = value & 0xffff;
	b     \jumpTo
	sh    value, 0(output)
	nop
	nop
	nop
.endm

## MDEC bitstream decompressor

.section .text._bsDecompressorStart, "ax", @progbits
.global _bsDecompressorStart
.type _bsDecompressorStart, @function

_bsDecompressorStart:
	addiu $sp, -16
	sw    $s0,  0($sp)
	sw    $s1,  4($sp)
	sw    $s2,  8($sp)
	sw    $s3, 12($sp)

	# header = (const BSHeader *) input;
	# bits   = swapHalfwords(header->data[0]);
	# lastY  = 0;
	lw    bits,  BSHeader_data0(input)
	li    lastY, 0
	swapHalfwords bits

	# nextBits = swapHalfwords(header->data[1]);
	# lastCr   = 0;
	lw    nextBits, BSHeader_data1(input)
	li    lastCr, 0
	swapHalfwords nextBits

	# remaining = header->outputLength * 2;
	# lastCb    = 0;
	lhu   remaining, BSHeader_outputLength(input)
	li    lastCb, 0
	sll   remaining, 1

	# quantScale = (header->quantScale & 63) << 10;
	# bitOffset  = 32;
	lw    temp, BSHeader_quantScale(input)
	li    bitOffset, 32
	andi  quantScale, temp, 63
	sll   quantScale, 10

	# isBSv3     = !(header->version < 3);
	# blockIndex = BLOCK_INDEX_CR;
	# coeffIndex = 0;
	srl   temp, 16
	sltiu isBSv3, temp, 3
	xori  isBSv3, 1
	li    blockIndex, BLOCK_INDEX_CR
	li    coeffIndex, 0

	# input = &(header->data[2]);
	j     _bsDecompressorSkipContextLoad
	addiu input, 16

.section .text._bsDecompressorResume, "ax", @progbits
.global _bsDecompressorResume
.type _bsDecompressorResume, @function

_bsDecompressorResume:
	addiu $sp, -16
	sw    $s0,  0($sp)
	sw    $s1,  4($sp)
	sw    $s2,  8($sp)
	sw    $s3, 12($sp)

	lw    input,      BSDecompressor_input     (this)
	lw    bits,       BSDecompressor_bits      (this)
	lw    nextBits,   BSDecompressor_nextBits  (this)
	lw    remaining,  BSDecompressor_remaining (this)
	lb    isBSv3,     BSDecompressor_isBSv3    (this)
	lb    bitOffset,  BSDecompressor_bitOffset (this)
	lb    blockIndex, BSDecompressor_blockIndex(this)
	lb    coeffIndex, BSDecompressor_coeffIndex(this)
	lhu   quantScale, BSDecompressor_quantScale(this)
	lh    lastY,      BSDecompressor_lastY     (this)
	lh    lastCr,     BSDecompressor_lastCr    (this)
	lh    lastCb,     BSDecompressor_lastCb    (this)

_bsDecompressorSkipContextLoad:
	# if (outputLength <= 0) outputLength = 0x3fff0000;
	bgtz  outputLength, .LoutputLengthValid
	addiu outputLength, -1

.LdefaultOutputLength:
	lui   outputLength, 0x3fff

.LoutputLengthValid:
	# outputLength = min((outputLength - 1) * 2, remaining);
	# remaining   -= outputLength;
	sll   outputLength, 1
	subu  remaining, outputLength
	bgez  remaining, .LremainingValid
	lui   temp, MDEC_COMMAND

.LadjustRemaining:
	addu  outputLength, remaining
	li    remaining, 0

.LremainingValid:
	# *(output++) = outputLength / 2;
	# *(output++) = MDEC_COMMAND;
	srl   value, outputLength, 1
	or    value, temp
	sw    value, 0(output)

	# huffmanTable = (const BSHuffmanTable *) CACHE_BASE;
	# acJumpPtr    = &LacJumpArea;
	la    huffmanTable, CACHE_BASE
	la    acJumpPtr, .LacJumpArea

	beqz  outputLength, .Lbreak
	addiu output, 4

.LdecompressLoop: # while (outputLength) {
	# The first step to decompress each code is to determine whether it is a DC
	# or AC coefficient. At the same time the GTE is given the task of counting
	# the number of leading zeroes/ones in the code.
	mtc2  bits, GTE_LZCS

	bnez  coeffIndex, .LisACCoeff
	addiu coeffIndex, 1

	bnez  isBSv3, .LisBSv3DCCoeff
	li    temp, BS_DC_END_CODE

.LisBSv2DCCoeff: # if (!coeffIndex && !isBSv3) {
	# The DC coefficient in v2 frames is a raw 10-bit value. Value 0x1ff is used
	# to signal the end of the bitstream.

	# value = bits >> (32 - BS_DC_V2_LENGTH);
	# if (value == BS_DC_END_CODE) break;
	srl   value, bits, 32 - BS_DC_V2_LENGTH
	beq   value, temp, .Lbreak
	or    value, quantScale

	# bits     <<= BS_DC_V2_LENGTH;
	# bitOffset -= BS_DC_V2_LENGTH;
	sll   bits, BS_DC_V2_LENGTH
	addiu bitOffset, -BS_DC_V2_LENGTH

	# *output = value | quantScale;
	b     .Ldone
	sh    value, 0(output)

.LisBSv3DCCoeff: # } else if (!coeffIndex && isBSv3) {
	# v3 DC coefficients are variable-length deltas prefixed with a Huffman code
	# indicating their length. Since the prefix code is up to 7 bits long, it
	# makes sense to decode it with a simple 128-byte lookup table rather than
	# using the GTE. The codes are different for luma and chroma blocks, so each
	# table entry contains the decoded length for both block types (packed as
	# two nibbles). Prefix 111111111 is used to signal the end of the bitstream.

	# length = bits >> (32 - BS_DC_V3_END_LENGTH);
	# if (length == BS_DC_END_CODE) break;
	srl   length, bits, 32 - BS_DC_V3_END_LENGTH
	beq   length, temp, .Lbreak
	srl   length, BS_DC_V3_END_LENGTH - BS_DC_V3_LENGTH

	# length >>= BS_DC_V3_END_LENGTH - BS_DC_V3_LENGTH;
	# length   = huffmanTable->dcValues[length];
	addu  length, huffmanTable

	addiu $at, blockIndex, -BLOCK_INDEX_CB
	bltz  $at, .LisY
	lbu   length, BSHuffmanTable_dcValues(length)

	# if (blockIndex >= BLOCK_INDEX_CB)
	beqz  $at, .LisCb
	andi  length, 15

.LisCr: # if (blockIndex > BLOCK_INDEX_CB) {
	# length &= 15;
	# temp    = huffmanTable->dcLengths[length] & 15;
	addu  temp, length, huffmanTable
	lbu   temp, BSHuffmanTable_dcLengths(temp)
	li    $at, 32

	# decodeBSv3DC(lastCb);
	# bits     <<= length;
	# bitOffset -= length;
	andi  temp, 15
	decodeBSv3DC lastCr, .LconsumeBits

.LisCb: # } else if (block_index == BLOCK_INDEX_CB) {
	# length &= 15;
	# temp    = huffmanTable->dcLengths[length] & 15;
	addu  temp, length, huffmanTable
	lbu   temp, BSHuffmanTable_dcLengths(temp)
	li    $at, 32

	# decodeBSv3DC(lastCb);
	# bits     <<= length;
	# bitOffset -= length;
	andi  temp, 15
	decodeBSv3DC lastCb, .LconsumeBits

.LisY: # } else {
	# length >>= 4;
	# temp     = huffmanTable->dcLengths[length] >> 4;
	nop
	srl   length, 4

	addu  temp, length, huffmanTable
	lbu   temp, BSHuffmanTable_dcLengths(temp)
	li    $at, 32

	# decodeBSv3DC(lastCb);
	# bits     <<= length;
	# bitOffset -= length;
	srl   temp, 4
	decodeBSv3DC lastY, .LconsumeBits

.LisACCoeff: # } } else {
	# Check whether the code starts with 10 or 11; if not, retrieve the leading
	# zero count from the GTE, validate it and use it as an index into the jump
	# area. Each case is 8 instructions long and handles decoding a specific
	# Huffman prefix.

	# temp = countLeadingZeroes(temp);
	mfc2  temp, GTE_LZCR

	bltz  bits, .Lac1
	addiu $at, temp, -MAX_PREFIX_LENGTH

	bgtz  $at, .LacInvalid
	sll   temp, JUMP_CASE_LENGTH

.Lac0: # if ((prefix <= MAX_PREFIX_LENGTH) && !(bits >> 31)) {
	# goto &acJumpArea[prefix << JUMP_CASE_LENGTH];
	addu  temp, acJumpPtr
	jr    temp
	nop

.LacInvalid: # } else if (prefix > MAX_PREFIX_LENGTH) {
	# return DECODE_ERROR;
	b     .Lreturn
	li    $v0, DECODE_ERROR

.Lac1: # } else {
	# bits <<= 1;
	# if (bits >> 31) goto &acJumpArea[0];
	sll   bits, 1
	bltz  bits, .LacJumpArea
	li    temp, MDEC_END_CODE

.Lac10: # else {
	# Prefix 10 marks the end of a block. Note that the 10/11 prefix check above
	# shifts the window by one bit *without* updating the bit offset.

	# bits     <<= 1;
	# bitOffset -= 2;
	sll   bits, 1
	addiu bitOffset, -2

	# *output = MDEC_END_CODE;
	sh    temp, 0(output)

	# coeffIndex = 0;
	# if (--blockIndex < BLOCK_INDEX_Y3) blockIndex = BLOCK_INDEX_CR;
	addiu blockIndex, -1
	bgez  blockIndex, .Ldone
	li    coeffIndex, 0

.LresetBlockIndex:
	b     .Ldone
	li    blockIndex, BLOCK_INDEX_CR

.LacJumpArea: # } }
	decodeFixedAC     1,  1, 1, BSHuffmanTable_ac0,  .Ldone      # (1)1x
	decodeVariableAC  2,  3,    BSHuffmanTable_ac2,  .LconsumeAC # 01xxx
	decodeVariableAC  3,  6,    BSHuffmanTable_ac3,  .LconsumeAC # 001xxxxxx
	decodeFixedAC     4,  3, 0, BSHuffmanTable_ac4,  .Ldone      # 0001xxx
	decodeFixedAC     5,  3, 0, BSHuffmanTable_ac5,  .Ldone      # 00001xxx
	decodeACEscape    6, 16, 0,                      .Ldone      # 000001xxxxxxxxxxxxxxxx
	decodeFixedAC     7,  4, 0, BSHuffmanTable_ac7,  .Ldone      # 0000001xxxx
	decodeFixedAC     8,  5, 0, BSHuffmanTable_ac8,  .Ldone      # 00000001xxxxx
	decodeFixedAC     9,  5, 0, BSHuffmanTable_ac9,  .Ldone      # 000000001xxxxx
	decodeFixedAC    10,  5, 0, BSHuffmanTable_ac10, .Ldone      # 0000000001xxxxx
	decodeFixedAC    11,  5, 0, BSHuffmanTable_ac11, .Ldone      # 00000000001xxxxx
	decodeFixedAC    12,  5, 0, BSHuffmanTable_ac12, .Ldone      # 000000000001xxxxx

.LconsumeAC:
	srl   length, value, 16

.LconsumeBits:
	sllv  bits, bits, length
	subu  bitOffset, length

.Ldone: # }
	# Update the bits. This makes sure the next iteration of the loop will be
	# able to read up to 32 bits from the bitstream.

	# coeffIndex++;
	# outputLength--;
	bgez  bitOffset, .LskipFeeding
	addiu outputLength, -1

.LfeedBitstream: # if (bitOffset < 0) {
	# bits = nextBits << (-bitOffset);
	subu  temp, $0, bitOffset
	sllv  bits, nextBits, temp

	# nextBits   = swapHalfwords(*(input++));
	# bitOffset += 32;
	lw    nextBits, 0(input)
	addiu bitOffset, 32
	swapHalfwords nextBits
	addiu input, 4

.LskipFeeding: # }
	# bits |= nextBits >> bitOffset;
	# output++;
	srlv  temp, nextBits, bitOffset
	or    bits, temp
	bnez  outputLength, .LdecompressLoop
	addiu output, 2

.Lbreak: # }
	beqz  remaining, .LpadOutput
	li    temp, MDEC_END_CODE

.LsaveContext: # if (remaining) {
	sw    input,      BSDecompressor_input     (this)
	sw    bits,       BSDecompressor_bits      (this)
	sw    nextBits,   BSDecompressor_nextBits  (this)
	sw    remaining,  BSDecompressor_remaining (this)
	sb    bitOffset,  BSDecompressor_bitOffset (this)
	sb    blockIndex, BSDecompressor_blockIndex(this)
	sb    coeffIndex, BSDecompressor_coeffIndex(this)
	sh    lastY,      BSDecompressor_lastY     (this)
	sh    lastCr,     BSDecompressor_lastCr    (this)
	sh    lastCb,     BSDecompressor_lastCb    (this)

	# return PARTIAL_DATA;
	b     .Lreturn
	li    $v0, PARTIAL_DATA

.LpadOutput: # } else {
	# for (; outputLength; outputLength--) *(output++) = MDEC_END_CODE;
	# return NO_ERROR;
	beqz  outputLength, .Lreturn
	li    $v0, NO_ERROR

.LpadOutputLoop:
	sh    temp, 0(output)
	addiu outputLength, -1
	bnez  outputLength, .LpadOutputLoop
	addiu output, 2

.Lreturn: # }
	lw    $s0,  0($sp)
	lw    $s1,  4($sp)
	lw    $s2,  8($sp)
	lw    $s3, 12($sp)

	jr    $ra
	addiu $sp, 16
