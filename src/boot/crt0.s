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

.set _STACK_SIZE, 0x100

# We're going to override ps1-bare-metal's _start() with a minimal version that
# skips .bss initialization (getting rid of memset() in the process) and moves
# the stack to a statically allocated buffer.
.section .text._start, "ax", @progbits
.global _start
.type _start, @function

_start:
	la    $gp, _gp
	j     main
	addiu $sp, $gp, %gprel(_stackBuffer) + _STACK_SIZE - 16

.section .sbss._stackBuffer, "aw"
.type _stackBuffer, @object

_stackBuffer:
	.space _STACK_SIZE
