/*
 * ps1-bare-metal - (C) 2023 spicyjpeg
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include "ps1/registers.h"
#include "ps1/system.h"

#define BIOS_API_TABLE  ((VoidFunction *) 0x80000200)
#define BIOS_BP_VECTOR  ((uint32_t *)     0x80000040)
#define BIOS_EXC_VECTOR ((uint32_t *)     0x80000080)

/* Internal state */

static VoidFunction _flushCache = 0;
static Thread       _mainThread;

ArgFunction interruptHandler     = 0;
void        *interruptHandlerArg = 0;

Thread *currentThread = &_mainThread;
Thread *nextThread    = &_mainThread;

/* Exception handler setup */

void _exceptionVector(void);

void installExceptionHandler(void) {
	// Clear all pending IRQ flags and prevent the interrupt controller from
	// generating further IRQs.
	IRQ_MASK = 0;
	IRQ_STAT = 0;
	DMA_DPCR = 0;
	DMA_DICR = DMA_DICR_CH_STAT_MASK;

	// Ensure interrupts and the GTE are enabled at the COP0 side.
	uint32_t sr = SR_IEc | SR_Im2 | SR_CU0 | SR_CU2;
	__asm__ volatile("mtc0 %0, $12;" :: "r"(sr));

	// Grab a direct pointer to the BIOS function to flush the instruction
	// cache. This is the only function that must always run from the BIOS ROM
	// as it temporarily disables main RAM.
	_flushCache = BIOS_API_TABLE[0x44];

	// Overwrite the default breakpoint and exception handlers placed into RAM
	// by the BIOS with a function that will jump to our custom handler.
	__builtin_memcpy(BIOS_BP_VECTOR,  &_exceptionVector, 16);
	__builtin_memcpy(BIOS_EXC_VECTOR, &_exceptionVector, 16);
	_flushCache();

	DMA_DPCR = 0x0bbbbbbb;
	DMA_DICR = DMA_DICR_IRQ_ENABLE;
}

void setInterruptHandler(ArgFunction func, void *arg) {
	setInterruptMask(0);

	interruptHandler    = func;
	interruptHandlerArg = arg;
	atomic_signal_fence(memory_order_release);
}

void flushCache(void) {
	//if (!_flushCache)
		//_flushCache = BIOS_API_TABLE[0x44];

	uint32_t mask = setInterruptMask(0);

	_flushCache();
	if (mask)
		setInterruptMask(mask);
}

/* IRQ acknowledgement and blocking delay */

void delayMicroseconds(int us) {
	// 1 us = 33.8688 cycles = 17 loop iterations (2 cycles per iteration)
	us *= (F_CPU + 1000000) / 2000000;

	__asm__ volatile(
		".set noreorder;"
		"bgtz  %0, .;"
		"addiu %0, -1;"
		".set reorder;"
		: "=r"(us) : "r"(us)
	);
}

bool acknowledgeInterrupt(IRQChannel irq) {
	if (IRQ_STAT & (1 << irq)) {
		IRQ_STAT = ~(1 << irq);
		return true;
	}

	return false;
}

bool waitForInterrupt(IRQChannel irq, int timeout) {
	for (; timeout > 0; timeout--) {
		if (acknowledgeInterrupt(irq))
			return true;

		delayMicroseconds(1);
	}

	return false;
}

bool waitForDMATransfer(DMAChannel dma, int timeout) {
	for (; timeout > 0; timeout--) {
		if (!(DMA_CHCR(dma) & DMA_CHCR_ENABLE))
			return true;

		delayMicroseconds(1);
	}

	return false;
}

/* Thread switching */

void switchThread(Thread *thread) {
	if (!thread)
		thread = &_mainThread;

	nextThread = thread;
	atomic_signal_fence(memory_order_release);
}

void switchThreadImmediate(Thread *thread) {
	if (!thread)
		thread = &_mainThread;

	nextThread = thread;
	atomic_signal_fence(memory_order_release);

	// Execute a syscall to force the switch to happen. Note that the syscall
	// handler will take a different path if $a0 is zero (see system.s), but
	// that will never happen here since the check above is ensuring $a0 (i.e.
	// the first argument) will always be a valid pointer.
	__asm__ volatile("syscall 0;" :: "r"(thread) : "a0", "memory");
}
