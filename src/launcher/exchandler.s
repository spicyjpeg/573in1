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

.set EPC, $14

.set SYS573_WATCHDOG, 0xbf5c0000

# We're going to override ps1-bare-metal's exception handler with a minimal
# version that only clears the watchdog and works around the GTE program counter
# bug, in order to minimize the size of the launcher binaries.
.section .text._launcherExceptionVector, "ax", @progbits
.global _launcherExceptionVector
.type _launcherExceptionVector, @function

_launcherExceptionVector:
	lui   $k0, %hi(SYS573_WATCHDOG)
	sw    $0,  %lo(SYS573_WATCHDOG)($k0)

	j     _launcherExceptionHandler
	mfc0  $k1, EPC

.section .text._launcherExceptionHandler, "ax", @progbits
.global _launcherExceptionHandler
.type _launcherExceptionHandler, @function

_launcherExceptionHandler:
	lw    $k0, 0($k1) # if ((*EPC >> 25) == 0x25) EPC++;
	li    $k1, 0x25
	srl   $k0, 25
	bne   $k0, $k1, .LskipIncrement
	mfc0  $k1, EPC

.LapplyIncrement:
	nop
	addiu $k1, 4

.LskipIncrement:
	nop

	jr    $k1
	rfe
