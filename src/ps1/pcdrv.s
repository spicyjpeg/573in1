# ps1-bare-metal - (C) 2023 spicyjpeg
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

.set noreorder

## Standard PCDRV API

.section .text.pcdrvInit, "ax", @progbits
.global pcdrvInit
.type pcdrvInit, @function

pcdrvInit:
	break 0, 0x101 # (_) -> error

	jr    $ra
	nop

.section .text.pcdrvCreate, "ax", @progbits
.global pcdrvCreate
.type pcdrvCreate, @function

pcdrvCreate:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x102 # (_, path, attributes) -> error, fd

	bgez  $v0, .LcreateOK # if (error < 0) fd = error
	nop
	move  $v1, $v0
.LcreateOK:
	jr    $ra # return fd
	move  $v0, $v1

.section .text.pcdrvOpen, "ax", @progbits
.global pcdrvOpen
.type pcdrvOpen, @function

pcdrvOpen:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x103 # (_, path, mode) -> error, fd

	bgez  $v0, .LopenOK # if (error < 0) fd = error
	nop
	move  $v1, $v0
.LopenOK:
	jr    $ra # return fd
	move  $v0, $v1

.section .text.pcdrvClose, "ax", @progbits
.global pcdrvClose
.type pcdrvClose, @function

pcdrvClose:
	move  $a1, $a0
	break 0, 0x104 # (_, fd) -> error

	jr    $ra
	nop

.section .text.pcdrvRead, "ax", @progbits
.global pcdrvRead
.type pcdrvRead, @function

pcdrvRead:
	move  $a3, $a1
	move  $a1, $a0
	break 0, 0x105 # (_, fd, length, data) -> error, length

	bgez  $v0, .LreadOK # if (error < 0) length = error
	nop
	move  $v1, $v0
.LreadOK:
	jr    $ra # return length
	move  $v0, $v1

.section .text.pcdrvWrite, "ax", @progbits
.global pcdrvWrite
.type pcdrvWrite, @function

pcdrvWrite:
	move  $a3, $a1
	move  $a1, $a0
	break 0, 0x106 # (_, fd, length, data) -> error, length

	bgez  $v0, .LwriteOK # if (error < 0) length = error
	nop
	move  $v1, $v0
.LwriteOK:
	jr    $ra # return length
	move  $v0, $v1

.section .text.pcdrvSeek, "ax", @progbits
.global pcdrvSeek
.type pcdrvSeek, @function

pcdrvSeek:
	move  $a3, $a2
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x107 # (_, fd, offset, mode) -> error, offset

	bgez  $v0, .LseekOK # if (error < 0) offset = error
	nop
	move  $v1, $v0
.LseekOK:
	jr    $ra # return offset
	move  $v0, $v1

## Extended PCDRV API

.section .text.pcdrvCreateDir, "ax", @progbits
.global pcdrvCreateDir
.type pcdrvCreateDir, @function

pcdrvCreateDir:
	move  $a1, $a0
	break 0, 0x108 # (_, path) -> error

	jr    $ra
	nop

.section .text.pcdrvRemoveDir, "ax", @progbits
.global pcdrvRemoveDir
.type pcdrvRemoveDir, @function

pcdrvRemoveDir:
	move  $a1, $a0
	break 0, 0x109 # (_, path) -> error

	jr    $ra
	nop

.section .text.pcdrvUnlink, "ax", @progbits
.global pcdrvUnlink
.type pcdrvUnlink, @function

pcdrvUnlink:
	move  $a1, $a0
	break 0, 0x10a # (_, path) -> error

	jr    $ra
	nop

.section .text.pcdrvChmod, "ax", @progbits
.global pcdrvChmod
.type pcdrvChmod, @function

pcdrvChmod:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x10b # (_, path, attributes) -> error

	jr    $ra
	nop

.section .text.pcdrvFindFirst, "ax", @progbits
.global pcdrvFindFirst
.type pcdrvFindFirst, @function

pcdrvFindFirst:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x10c # (_, path, entry) -> error, fd

	bgez  $v0, .LfindFirstOK # if (error < 0) fd = error
	nop
	move  $v1, $v0
.LfindFirstOK:
	jr    $ra # return fd
	move  $v0, $v1

.section .text.pcdrvFindNext, "ax", @progbits
.global pcdrvFindNext
.type pcdrvFindNext, @function

pcdrvFindNext:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x10d # (_, fd, entry) -> error

	jr    $ra
	nop

.section .text.pcdrvRename, "ax", @progbits
.global pcdrvRename
.type pcdrvRename, @function

pcdrvRename:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x10e # (_, path, newPath) -> error

	jr    $ra
	nop
