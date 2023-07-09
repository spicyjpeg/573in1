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

.section .text.pcdrvInit, "ax", @progbits
.global pcdrvInit
.type pcdrvInit, @function

pcdrvInit:
	break 0, 0x101 # () -> error

	jr    $ra
	nop

.section .text.pcdrvCreate, "ax", @progbits
.global pcdrvCreate
.type pcdrvCreate, @function

pcdrvCreate:
	move  $a2, $a1
	move  $a1, $a0
	break 0, 0x102 # (path, path, 0) -> error, fd

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
	break 0, 0x103 # (path, path, mode) -> error, fd

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
	break 0, 0x104 # (fd, fd) -> error

	jr    $ra
	nop

.section .text.pcdrvRead, "ax", @progbits
.global pcdrvRead
.type pcdrvRead, @function

pcdrvRead:
	move  $a3, $a1
	move  $a1, $a0
	break 0, 0x105 # (fd, fd, length, data) -> error, length

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
	break 0, 0x106 # (fd, fd, length, data) -> error, length

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
	break 0, 0x107 # (fd, fd, offset, mode) -> error, offset

	bgez  $v0, .LseekOK # if (error < 0) offset = error
	nop
	move  $v1, $v0
.LseekOK:
	jr    $ra # return offset
	move  $v0, $v1
