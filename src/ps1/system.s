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
.set noat

.set BADV,  $8
.set SR,    $12
.set CAUSE, $13
.set EPC,   $14

.set IO_BASE,  0xbf80
.set IRQ_STAT, 0x1070
.set IRQ_MASK, 0x1074

.section .text._exceptionVector, "ax", @progbits
.global _exceptionVector
.type _exceptionVector, @function

_exceptionVector:
	# This tiny stub is going to be relocated to the address the CPU jumps to
	# when an exception occurs (0x80000080) at runtime, overriding the default
	# one installed by the BIOS.
	lui   $k0, %hi(currentThread)
	j     _exceptionHandler
	lw    $k0, %lo(currentThread)($k0)

	nop

.section .text._exceptionHandler, "ax", @progbits
.global _exceptionHandler
.type _exceptionHandler, @function

_exceptionHandler:
	# We're going to need at least 3 registers to store a pointer to the current
	# thread, the return pointer (EPC) and the state of the CAUSE register
	# respectively. $k0 and $k1 are always available to the exception handler,
	# so only $at needs to be saved.
	nop
	sw    $at, 0x04($k0)
	mfc0  $at, CAUSE

	# Check CAUSE bits 2-6 to determine what triggered the exception. If it was
	# caused by a syscall, increment EPC so it won't be executed again;
	# furthermore, if the first argument passed to the syscall was zero, take an
	# alternate, shorter code path (as it is the syscall to set IRQ_MASK).
	li    $k1, 8 << 2 # if (((CAUSE >> 2) & 0x1f) != 8) goto notFastSyscall
	andi  $at, 0x1f << 2
	bne   $at, $k1, .LnotFastSyscall
	mfc0  $k1, EPC

	bnez  $a0, .LnotFastSyscall # if (arg0) goto notFastSyscall
	addiu $k1, 4 # EPC++

.LfastSyscall:
	# Save the current value of IRQ_MASK then set it "atomically" (as interrupts
	# are disabled while the exception handler runs), then restore $at and
	# return immediately without the overhead of saving and restoring the whole
	# thread.
	lui   $at, IO_BASE
	lhu   $v0, IRQ_MASK($at) # returnValue = IRQ_MASK
	lw    $at, 0x04($k0)
	sh    $a1, IRQ_MASK($at) # IRQ_MASK = arg1

	jr    $k1
	rfe

.LnotFastSyscall:
	# If the fast path couldn't be taken, save the full state of the thread. The
	# state of $hi/$lo is saved after all other registers in order to let the
	# multiplier finish any ongoing calculation.
	sw    $v0, 0x08($k0)
	sw    $v1, 0x0c($k0)
	sw    $a0, 0x10($k0)
	sw    $a1, 0x14($k0)
	sw    $a2, 0x18($k0)
	sw    $a3, 0x1c($k0)
	sw    $t0, 0x20($k0)
	sw    $t1, 0x24($k0)
	sw    $t2, 0x28($k0)
	sw    $t3, 0x2c($k0)
	sw    $t4, 0x30($k0)
	sw    $t5, 0x34($k0)
	sw    $t6, 0x38($k0)
	sw    $t7, 0x3c($k0)
	sw    $s0, 0x40($k0)
	sw    $s1, 0x44($k0)
	sw    $s2, 0x48($k0)
	sw    $s3, 0x4c($k0)
	sw    $s4, 0x50($k0)
	sw    $s5, 0x54($k0)
	sw    $s6, 0x58($k0)
	sw    $s7, 0x5c($k0)
	sw    $t8, 0x60($k0)
	sw    $t9, 0x64($k0)
	sw    $gp, 0x68($k0)
	sw    $sp, 0x6c($k0)
	sw    $fp, 0x70($k0)
	sw    $ra, 0x74($k0)

	mfhi  $v0
	mflo  $v1
	sw    $v0, 0x78($k0)
	sw    $v1, 0x7c($k0)

	# Check again if the CAUSE code is either 0 (IRQ) or 8 (syscall). If not
	# call _unhandledException(), which will then display information about the
	# exception and lock up.
	lui   $v0, %hi(interruptHandler)
	andi  $v1, $at, 0x17 << 2 # if (!(((CAUSE >> 2) & 0x1f) % 8)) goto irqOrSyscall
	beqz  $v1, .LirqOrSyscall
	lw    $v0, %lo(interruptHandler)($v0)

.LotherException:
	mfc0  $a1, BADV # _unhandledException((CAUSE >> 2) & 0x1f, BADV)
	srl   $a0, $at, 2
	jal   _unhandledException
	addiu $sp, -8

	b     .Lreturn
	addiu $sp, 8

.LirqOrSyscall:
	# Otherwise, check if the interrupted instruction was a GTE opcode and
	# increment EPC to avoid executing it again (as with syscalls). This is a
	# workaround for a hardware bug.
	lw    $v1, 0($k1)
	li    $at, 0x25 # if ((*EPC >> 25) == 0x25) EPC++
	srl   $v1, 25
	bne   $v1, $at, .LskipIncrement
	lui   $a0, %hi(interruptHandlerArg)

	addiu $k1, 4

.LskipIncrement:
	lw    $a0, %lo(interruptHandlerArg)($a0)
	sw    $k1, 0x00($k0)

	# Dispatch any pending interrupts.
	jalr  $v0 # interruptHandler(interruptHandlerArg)
	addiu $sp, -8

	addiu $sp, 8

.Lreturn:
	# Grab a pointer to the next thread to be executed, restore its state and
	# return.
	lui   $k0, %hi(nextThread)
	lw    $k0, %lo(nextThread)($k0)
	lui   $at, %hi(currentThread)
	sw    $k0, %lo(currentThread)($at)

	lw    $v0, 0x78($k0)
	lw    $v1, 0x7c($k0)
	mthi  $v0
	mtlo  $v1

	lw    $k1, 0x00($k0)
	lw    $at, 0x04($k0)
	lw    $v0, 0x08($k0)
	lw    $v1, 0x0c($k0)
	lw    $a0, 0x10($k0)
	lw    $a1, 0x14($k0)
	lw    $a2, 0x18($k0)
	lw    $a3, 0x1c($k0)
	lw    $t0, 0x20($k0)
	lw    $t1, 0x24($k0)
	lw    $t2, 0x28($k0)
	lw    $t3, 0x2c($k0)
	lw    $t4, 0x30($k0)
	lw    $t5, 0x34($k0)
	lw    $t6, 0x38($k0)
	lw    $t7, 0x3c($k0)
	lw    $s0, 0x40($k0)
	lw    $s1, 0x44($k0)
	lw    $s2, 0x48($k0)
	lw    $s3, 0x4c($k0)
	lw    $s4, 0x50($k0)
	lw    $s5, 0x54($k0)
	lw    $s6, 0x58($k0)
	lw    $s7, 0x5c($k0)
	lw    $t8, 0x60($k0)
	lw    $t9, 0x64($k0)
	lw    $gp, 0x68($k0)
	lw    $sp, 0x6c($k0)
	lw    $fp, 0x70($k0)
	lw    $ra, 0x74($k0)

	jr    $k1
	rfe
