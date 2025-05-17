/*
 * ps1-bare-metal - (C) 2023-2025 spicyjpeg
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

#include <stdbool.h>
#include <stdint.h>
#include "ps1/cop0.h"
#include "ps1/registers.h"
#include "ps1/system.h"

#define BIOS_ENTRY_POINT     ((VoidFunction) 0xbfc00000)
#define BIOS_ALT_ENTRY_POINT ((VoidFunction) 0xbfc00390)
#define BIOS_SHELL_LOAD_ADDR ((VoidFunction) 0x80030000)
#define BIOS_BP_VECTOR       ((uint32_t *)   0x80000040)
#define BIOS_EXC_VECTOR      ((uint32_t *)   0x80000080)

/* Internal state */

static uint32_t _savedBreakpointVector[4];
static uint32_t _savedExceptionVector [4];
static Thread   _mainThread;

ArgFunction interruptHandler      = 0;
void        *interruptHandlerArg0 = 0;
void        *interruptHandlerArg1 = 0;

Thread *currentThread = &_mainThread;
Thread *nextThread    = &_mainThread;

/* Exception handler setup */

void resetInterrupts(void) {
	// Clear the current COP0 state, ensuring interrupts are disabled and the
	// GTE is enabled.
	cop0_setReg(COP0_STATUS, COP0_STATUS_CU0 | COP0_STATUS_CU2);

	// Clear all pending IRQ flags and prevent the interrupt controller from
	// generating further IRQs.
	IRQ_MASK = 0;
	IRQ_STAT = 0;
	DMA_DPCR = 0;
	DMA_DICR = DMA_DICR_CH_STAT_BITMASK;
}

void installCustomExceptionHandler(VoidFunction func) {
	resetInterrupts();

	// Overwrite the default breakpoint and exception handlers placed into RAM
	// by the BIOS.
	__builtin_memcpy(_savedBreakpointVector, BIOS_BP_VECTOR,  16);
	__builtin_memcpy(_savedExceptionVector,  BIOS_EXC_VECTOR, 16);
	__builtin_memcpy(BIOS_BP_VECTOR,         func,            16);
	__builtin_memcpy(BIOS_EXC_VECTOR,        func,            16);
	flushCache();

	DMA_DPCR = 0
		| DMA_DPCR_CH_PRIORITY(DMA_MDEC_IN,  3) | DMA_DPCR_CH_ENABLE(DMA_MDEC_IN)
		| DMA_DPCR_CH_PRIORITY(DMA_MDEC_OUT, 3) | DMA_DPCR_CH_ENABLE(DMA_MDEC_OUT)
		| DMA_DPCR_CH_PRIORITY(DMA_GPU,      3) | DMA_DPCR_CH_ENABLE(DMA_GPU)
		| DMA_DPCR_CH_PRIORITY(DMA_CDROM,    3) | DMA_DPCR_CH_ENABLE(DMA_CDROM)
		| DMA_DPCR_CH_PRIORITY(DMA_SPU,      3) | DMA_DPCR_CH_ENABLE(DMA_SPU)
		| DMA_DPCR_CH_PRIORITY(DMA_PIO,      3) | DMA_DPCR_CH_ENABLE(DMA_PIO)
		| DMA_DPCR_CH_PRIORITY(DMA_OTC,      3) | DMA_DPCR_CH_ENABLE(DMA_OTC);
	DMA_DICR = DMA_DICR_IRQ_ENABLE;

	// Ensure interrupt masking is set up properly at the COP0 side.
	cop0_setReg(
		COP0_STATUS,
		COP0_STATUS_Im2 | COP0_STATUS_CU0 | COP0_STATUS_CU2
	);
}

void uninstallExceptionHandler(void) {
	resetInterrupts();

	// Restore the original BIOS breakpoint and exception handlers.
	__builtin_memcpy(BIOS_BP_VECTOR,  _savedBreakpointVector, 16);
	__builtin_memcpy(BIOS_EXC_VECTOR, _savedExceptionVector,  16);
	flushCache();
}

void setInterruptHandler(ArgFunction func, void *arg0, void *arg1) {
	disableInterrupts();

	interruptHandler     = func;
	interruptHandlerArg0 = arg0;
	interruptHandlerArg1 = arg1;
	flushWriteQueue();
}

/* Reset functions */

void _fastRebootBreakVector(void);
void _fastRebootDummyShell(void);

void softReset(void) {
	resetInterrupts();

	// Jump back to the entry point of the BIOS ROM. While not technically
	// equivalent to a full system reset, this will still result in the BIOS
	// reinitializing most of the hardware and running again.
	BIOS_ENTRY_POINT();
	__builtin_unreachable();
}

void softFastReboot(void) {
	resetInterrupts();

	// Place a dummy shell (a function that returns immediately) at the location
	// the BIOS will try to load the actual shell binary at, then set up a COP0
	// breakpoint to protect it from being overwritten (see
	// _fastRebootBreakVector()). The breakpoint will be triggered by the first
	// write to the 0x80030000-0x8003ffff range.
	__builtin_memcpy(BIOS_BP_VECTOR,       &_fastRebootBreakVector, 16);
	__builtin_memcpy(BIOS_SHELL_LOAD_ADDR, &_fastRebootDummyShell,   8);

	cop0_setReg(COP0_DCIC, 0);
	cop0_setReg(COP0_BDA,  (uint32_t) BIOS_SHELL_LOAD_ADDR);
	cop0_setReg(COP0_BDAM, 0xffff0000);
	cop0_setReg(
		COP0_DCIC,
		0
			| COP0_DCIC_DE
			| COP0_DCIC_DAE
			| COP0_DCIC_DW
			| COP0_DCIC_KD
			| COP0_DCIC_UD
	);

	// Once the breakpoint is configured, jump to the middle of the BIOS entry
	// point in order to skip the code that clears COP0 registers (and would
	// thus disable our breakpoint).
	BIOS_ALT_ENTRY_POINT();
	__builtin_unreachable();
}

/* IRQ acknowledgement */

bool acknowledgeInterrupt(IRQChannel irq) {
	if (IRQ_STAT & (1 << irq)) {
		IRQ_STAT = ~(1 << irq);
		return true;
	}

	return false;
}

bool waitForInterrupt(IRQChannel irq, int timeout) {
	for (; timeout >= 0; timeout -= 10) {
		if (acknowledgeInterrupt(irq))
			return true;

		delayMicroseconds(10);
	}

	return false;
}

bool waitForDMATransfer(DMAChannel dma, int timeout) {
	for (; timeout >= 0; timeout -= 10) {
		if (!(DMA_CHCR(dma) & DMA_CHCR_ENABLE))
			return true;

		delayMicroseconds(10);
	}

	return false;
}

/* Thread switching */

void switchThread(Thread *thread) {
	if (!thread)
		thread = &_mainThread;

	nextThread = thread;
	flushWriteQueue();
}
