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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ps1/registers.h"

typedef enum {
	CAUSE_INT  =  0, // Interrupt
	CAUSE_AdEL =  4, // Load address error
	CAUSE_AdES =  5, // Store address error
	CAUSE_IBE  =  6, // Instruction bus error
	CAUSE_DBE  =  7, // Data bus error
	CAUSE_SYS  =  8, // Syscall
	CAUSE_BP   =  9, // Breakpoint or break instruction
	CAUSE_RI   = 10, // Reserved instruction
	CAUSE_CpU  = 11, // Coprocessor unusable
	CAUSE_Ov   = 12  // Arithmetic overflow
} ExceptionCause;

typedef enum {
	SR_IEc = 1 <<  0, // Current interrupt enable
	SR_KUc = 1 <<  1, // Current privilege level
	SR_IEp = 1 <<  2, // Previous interrupt enable
	SR_KUp = 1 <<  3, // Previous privilege level
	SR_IEo = 1 <<  4, // Old interrupt enable
	SR_KUo = 1 <<  5, // Old privilege level
	SR_Im0 = 1 <<  8, // IRQ mask 0 (software interrupt)
	SR_Im1 = 1 <<  9, // IRQ mask 1 (software interrupt)
	SR_Im2 = 1 << 10, // IRQ mask 2 (hardware interrupt)
	SR_CU0 = 1 << 28, // Coprocessor 0 privilege level
	SR_CU2 = 1 << 30  // Coprocessor 2 enable
} SRFlag;

typedef struct {
	uint32_t pc, at, v0, v1, a0, a1, a2, a3;
	uint32_t t0, t1, t2, t3, t4, t5, t6, t7;
	uint32_t s0, s1, s2, s3, s4, s5, s6, s7;
	uint32_t t8, t9, gp, sp, fp, ra, hi, lo;
} Thread;

typedef void (*VoidFunction)(void);
typedef void (*ArgFunction)(void *arg);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read-only pointer to the currently running thread.
 */
extern Thread *currentThread;

/**
 * @brief Read-only pointer to the thread scheduled to be executed after the
 * currently running one. Use switchThread() or switchThreadImmediate() to set
 * this pointer.
 */
extern Thread *nextThread;

/**
 * @brief Disables interrupts temporarily, then sets the IRQ_MASK register to
 * the specified value (which should be a bitfield of (1 << IRQ_*) flags) and
 * returns its previous value. Must *not* be used in IRQ handlers.
 */
static inline uint32_t setInterruptMask(uint32_t mask) {
	register uint32_t v0 __asm__("v0");
	register uint32_t a0 __asm__("a0") = 0;
	register uint32_t a1 __asm__("a1") = mask;

	__asm__ volatile("syscall 0;" : "=r"(v0) : "r"(a0), "r"(a1) : "memory");
	return v0;
}

/**
 * @brief Initializes a thread structure with the provided entry point
 * (function) and stacktop. The function *must not* return. The stack should be
 * aligned to 8 bytes as required by the MIPS ABI.
 *
 * @param thread
 * @param func
 * @param arg Optional argument to entry point
 * @param stack Pointer to last 8 bytes in the stack
 */
static inline void initThread(
	Thread *thread, ArgFunction func, void *arg, void *stack
) {
	register uint32_t gp __asm__("gp");

	thread->pc = (uint32_t) func;
	thread->a0 = (uint32_t) arg;
	thread->gp = (uint32_t) gp;
	thread->sp = (uint32_t) stack;
	thread->fp = (uint32_t) stack;
	thread->ra = 0;
}

/**
 * @brief Sets up the exception handler, removes the BIOS from memory and
 * flushes the instruction cache. Must be called only once, before *any* other
 * function in this header is used.
 */
void installExceptionHandler(void);

/**
 * @brief Disables interrupts and sets the function that will be called whenever
 * a future interrupt or syscall occurs. Must be called after
 * installExceptionHandler() and before interrupts are enabled. As the callback
 * will run from within the exception handler, it is subject to several
 * limitations:
 *
 * - it cannot call functions that rely on syscalls such as enableInterrupts(),
 *   switchThreadImmediate() or setInterruptHandler();
 * - it cannot wait for other interrupts to occur;
 * - it must return quickly, as IRQs fired while the exception handler is
 *   running may otherwise be missed.
 *
 * Interrupts must be re-enabled manually using setInterruptMask() after setting
 * a new handler.
 *
 * @param func
 * @param arg Optional argument to be passed to handler
 */
void setInterruptHandler(ArgFunction func, void *arg);

/**
 * @brief Clears the instruction cache. Must *not* be used in IRQ handlers.
 */
void flushCache(void);

/**
 * @brief Jumps to the entry point in the BIOS. This function does not return.
 */
void softReset(void);

/**
 * @brief Blocks for (roughly) the specified number of microseconds. This
 * function does not rely on a hardware timer, so interrupts may throw off
 * timings if not explicitly disabled prior to calling delayMicroseconds().
 *
 * @param time
 */
void delayMicroseconds(int time);

/**
 * @brief Checks if the specified interrupt was fired but not yet acknowledged;
 * if so, acknowledges it and returns true. This function can be used in a
 * callback set using setInterruptHandler() to check for individual IRQs that
 * need to be processed, but will also work with interrupts that are not
 * explicitly enabled in the IRQ_MASK register.
 *
 * Note that most interrupts must additionally be acknowledged at the device
 * side (through DMA/SIO/SPU/CD-ROM registers or by issuing the GP1 IRQ
 * acknowledge command) once this function returns true. Lightgun, vblank and
 * timer interrupts do not require device-side acknowledgement.
 *
 * @param irq
 * @return True if the IRQ was pending and got acknowledged, false otherwise
 */
bool acknowledgeInterrupt(IRQChannel irq);

/**
 * @brief Waits for the specified interrupt to be fired for up to the specified
 * number of microseconds. This function will work with interrupts that are not
 * explicitly enabled in the IRQ_MASK register, but will *not* work with
 * interrupts that have been enabled if any callback set using
 * setInterruptHandler() acknowledges them.
 *
 * @param irq
 * @param timeout
 * @return False in case of a timeout, true otherwise
 */
bool waitForInterrupt(IRQChannel irq, int timeout);

/**
 * @brief Waits for the specified DMA channel to finish any ongoing transfer for
 * up to the specified number of microseconds.
 *
 * @param dma
 * @param timeout
 * @return False in case of a timeout, true otherwise
 */
bool waitForDMATransfer(DMAChannel dma, int timeout);

/**
 * @brief Pauses the thread calling this function and starts/resumes executing
 * the specified one. The switch will not happen immediately, but will only be
 * processed once an interrupt occurs or a syscall is issued. If called from an
 * IRQ handler, it will happen once all IRQ handlers have been executed.
 *
 * @param thread Pointer to new thread or NULL for main thread
 */
void switchThread(Thread *thread);

/**
 * @brief Pauses the thread calling this function immediately and starts/resumes
 * executing the specified one. Once the other thread switches back, execution
 * will resume from after the call to switchThreadImmediate(). This function
 * must *not* be used in IRQ handlers; use switchThread() (which will behave
 * identically as thread switches are processed right after IRQ handling)
 * instead.
 *
 * @param thread Pointer to new thread or NULL for main thread
 */
void switchThreadImmediate(Thread *thread);

#ifdef __cplusplus
}
#endif
