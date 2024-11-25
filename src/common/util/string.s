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

.set GTE_LZCS, $30 # Leading zero count input
.set GTE_LZCR, $31 # Leading zero count output

## UTF-8 parser

.set codePoint, $v0
.set length,    $v1
.set ch,        $a0
.set startMask, $a1
.set contMask,  $a2
.set contByte,  $a3
.set temp,      $t0
.set i,         $t1

.section .text._parseUTF8Character, "ax", @progbits
.global _parseUTF8Character
.type _parseUTF8Character, @function

_parseUTF8Character:
	# 1-byte character: 0xxxxxxx
	# 2-byte character: 110xxxxx 10xxxxxx
	# 3-byte character: 1110xxxx 10xxxxxx 10xxxxxx
	# 4-byte character: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

	# uint8_t codePoint = *(ch++);
	lb    codePoint, 0(ch)
	addiu ch, 1
	bltz  codePoint, .LmsbSet
	andi  codePoint, 0xff

.LmsbNotSet: # if (signExtend(codePoint) >= 0) {
	# return { codePoint, 1 };
	jr    $ra
	li    length, 1

.LmsbSet: # } else {
	# size_t length = countLeadingOnes(codePoint << 24);
	sll   temp, codePoint, 24
	mtc2  temp, GTE_LZCS
	li    contMask, (1 << 7)
	nop
	mfc2  length, GTE_LZCR
	li    startMask, (1 << 7) - 1

	# codePoint &= (1 << (7 - length)) - 1;
	srlv  startMask, startMask, length
	and   codePoint, startMask

	# int i = length - 1;
	addiu i, length, -2

.LcontinuationLoop: # do {
	# uint8_t contByte = *(ch++);
	lbu   contByte, 0(ch)
	addiu ch, 1

	# if ((contByte & 0xc0) != 0x80) return { codePoint, 0 };
	andi  temp, contByte, (3 << 6)
	bne   temp, contMask, .LreturnInvalid
	andi  contByte, (1 << 6) - 1

	# codePoint <<= 6;
	# codePoint  |= contByte & 0x3f;
	sll   codePoint, 6
	or    codePoint, contByte

	bgtz  i, .LcontinuationLoop
	addiu i, -1

.LreturnValid: # } while ((--i) > 0);
	# return { codePoint, length };
	jr    $ra
	nop

.LreturnInvalid: # }
	jr    $ra
	li    length, 0
