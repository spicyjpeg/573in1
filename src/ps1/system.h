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
#include "ps1/cop0.h"
#include "ps1/registers.h"

typedef struct {
	uint32_t pc, at, v0, v1, a0, a1, a2, a3;
	uint32_t t0, t1, t2, t3, t4, t5, t6, t7;
	uint32_t s0, s1, s2, s3, s4, s5, s6, s7;
	uint32_t t8, t9, gp, sp, fp, ra, hi, lo;
} Thread;

typedef void (*VoidFunction)(void);
typedef void (*ArgFunction)(void *arg0, void *arg1);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read-only pointer to the currently running thread.
 */
extern Thread *currentThread;

/**
 * @brief Read-only pointer to the thread scheduled to be executed after the
 * currently running one. Use switchThread() to set this pointer.
 */
extern Thread *nextThread;

void _exceptionVector(void);

/**
 * @brief Enables all interrupts at the COP0 side (without altering the IRQ_MASK
 * register). If any IRQs occurred and were not acknowledged while interrupts
 * were disabled, any callback set using setInterruptHandler() will be invoked
 * immediately.
 */
__attribute__((always_inline)) static inline void enableInterrupts(void) {
	cop0_setReg(COP0_SR, cop0_getReg(COP0_SR) | COP0_SR_IEc);
}

/**
 * @brief Disables all interrupts at the COP0 side (without altering the
 * IRQ_MASK register). This function is not atomic, but can be used safely as
 * long as no other code is manipulating the COP0 SR register while interrupts
 * are enabled.
 *
 * @return True if interrupts were previously enabled, false otherwise
 */
__attribute__((always_inline)) static inline bool disableInterrupts(void) {
	uint32_t sr = cop0_getReg(COP0_SR);

	cop0_setReg(COP0_SR, sr & ~COP0_SR_IEc);
	return (sr & COP0_SR_IEc);
}

/**
 * @brief Forces all pending memory writes to complete and stalls until the
 * write queue is empty. Calling this function is not necessary when accessing
 * memory or hardware registers through KSEG1 as the write queue is only enabled
 * when using KUSEG or KSEG0.
 */
__attribute__((always_inline)) static inline void flushWriteQueue(void) {
	__atomic_signal_fence(__ATOMIC_RELEASE);
	*((volatile uint8_t *) 0xbfc00000);
}

/**
 * @brief Initializes a thread structure with the provided entry point
 * (function) and stacktop. The function *must not* return. The stack should be
 * aligned to 8 bytes as required by the MIPS ABI.
 *
 * @param thread
 * @param func
 * @param arg0 Optional first argument to entry point
 * @param arg1 Optional second argument to entry point
 * @param stack Pointer to last 8 bytes in the stack
 */
__attribute__((always_inline)) static inline void initThread(
	Thread *thread, ArgFunction func, void *arg0, void *arg1, void *stack
) {
	register uint32_t gp __asm__("gp");

	thread->pc = (uint32_t) func;
	thread->a0 = (uint32_t) arg0;
	thread->a1 = (uint32_t) arg1;
	thread->gp = (uint32_t) gp;
	thread->sp = (uint32_t) stack;
	thread->fp = (uint32_t) stack;
	thread->ra = 0;
}

/**
 * @brief Disables the exception handler provided by the BIOS, replaces it with
 * the provided function, which must be relocatable and consist of no more than
 * 4 instructions (16 bytes), and flushes the instruction cache (but does not
 * enable interrupts). This function is mutually exclusive with
 * installExceptionHandler() and must be called only once.
 *
 * @param func
 */
void installCustomExceptionHandler(VoidFunction func);

/**
 * @brief Sets up the exception handler, disables the one provided by the BIOS
 * kernel and flushes the instruction cache (but does not enable interrupts).
 * This function is mutually exclusive with installCustomExceptionHandler() and
 * must be called only once.
 */
static inline void installExceptionHandler(void) {
	installCustomExceptionHandler(&_exceptionVector);
}

/**
 * @brief Restores the BIOS kernel's exception handler. Must be called before
 * returning to the kernel or launching another executable, if
 * installExceptionHandler() or installCustomExceptionHandler() were previously
 * called.
 */
void uninstallExceptionHandler(void);

/**
 * @brief Disables interrupts and sets the function that will be called whenever
 * a future interrupt or syscall occurs. Must be called after
 * installExceptionHandler() and before interrupts are enabled. As the callback
 * will run from within the exception handler, it is subject to several
 * limitations:
 *
 * - it cannot call functions that rely on syscalls such as enableInterrupts(),
 *   forceThreadSwitch() or setInterruptHandler();
 * - it cannot wait for other interrupts to occur;
 * - it must return quickly, as IRQs fired while the exception handler is
 *   running may otherwise be missed.
 *
 * Interrupts must be re-enabled manually using enableInterrupts() after setting
 * a new handler.
 *
 * @param func
 * @param arg Optional first argument to be passed to handler
 * @param arg Optional second argument to be passed to handler
 */
void setInterruptHandler(ArgFunction func, void *arg0, void *arg1);

/**
 * @brief Temporarily disables interrupts, then calls the BIOS function to clear
 * the instruction cache.
 */
void flushCache(void);

/**
 * @brief Jumps to the entry point in the BIOS. This function does not return.
 */
void softReset(void);

/**
 * @brief Blocks for (roughly) the specified number of microseconds. This
 * function will reset hardware timer 2 and use it for timing. Disabling
 * interrupts prior to calling delayMicroseconds() is highly recommended to
 * prevent jitter, but not strictly necessary unless the interrupt handler
 * accesses timer 2.
 *
 * @param time
 */
void delayMicroseconds(int time);

/**
 * @brief Blocks for (roughly) the specified number of microseconds. This
 * function does not rely on a hardware timer, so interrupts may throw off
 * timings if not explicitly disabled prior to calling delayMicrosecondsBusy().
 *
 * @param time
 */
void delayMicrosecondsBusy(int time);

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
 * number of microseconds (with 10 us granularity). This function will work with
 * interrupts that are not explicitly enabled in the IRQ_MASK register, but will
 * *not* work with interrupts that have been enabled if any callback set using
 * setInterruptHandler() acknowledges them.
 *
 * @param irq
 * @param timeout
 * @return False in case of a timeout, true otherwise
 */
bool waitForInterrupt(IRQChannel irq, int timeout);

/**
 * @brief Waits for the specified DMA channel to finish any ongoing transfer for
 * up to the specified number of microseconds (with 10 us granularity).
 *
 * @param dma
 * @param timeout
 * @return False in case of a timeout, true otherwise
 */
bool waitForDMATransfer(DMAChannel dma, int timeout);

/**
 * @brief Pauses the thread calling this function and starts/resumes executing
 * the specified one. The switch will not happen immediately, but will only be
 * processed once an interrupt occurs or forceThreadSwitch() is invoked. If
 * called from the IRQ handler, it will be deferred to when the handler returns.
 *
 * @param thread Pointer to new thread or NULL for main thread
 */
void switchThread(Thread *thread);

/**
 * @brief Runs the exception handler. If called after switchThread(), pauses the
 * thread calling this function immediately and starts/resumes executing the
 * specified one. Once the other thread switches back, execution will resume
 * from after the call to forceThreadSwitch(). This function must *not* be used
 * in IRQ handlers (as thread switches are processed right after IRQ handling).
 *
 * @param thread Pointer to new thread or NULL for main thread
 */
__attribute__((always_inline)) static inline void forceThreadSwitch(void) {
	__asm__ volatile("syscall 0\n" ::: "memory");
}

#ifdef __cplusplus
}
#endif
