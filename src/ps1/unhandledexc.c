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
 *
 * If any exception other than an IRQ or syscall (such as a bus or alignment
 * error) occurs, the exception handler defined in system.s will call
 * _unhandledException() to safely halt the program. This is a very simple
 * implementation of it that prints the state of all registers then locks up.
 */

#include <stdint.h>
#include <stdio.h>
#include "ps1/system.h"

#ifndef NDEBUG
static const char *const _causeNames[] = {
	"load address error",
	"store address error",
	"instruction bus error",
	"data bus error",
	"syscall",
	"break instruction",
	"reserved instruction",
	"coprocessor unusable",
	"arithmetic overflow"
};

static const char _registerNames[] =
	"pc" "at" "v0" "v1" "a0" "a1" "a2" "a3"
	"t0" "t1" "t2" "t3" "t4" "t5" "t6" "t7"
	"s0" "s1" "s2" "s3" "s4" "s5" "s6" "s7"
	"t8" "t9" "gp" "sp" "fp" "ra" "hi" "lo";
#endif

void _unhandledException(int cause, uint32_t badv) {
#ifndef NDEBUG
	if (cause <= 5)
		printf("Exception: %s (%08x)\nRegister dump:\n", _causeNames[cause - 4], badv);
	else
		printf("Exception: %s\nRegister dump:\n", _causeNames[cause - 4]);

	const char *name = _registerNames;
	uint32_t   *reg  = (uint32_t *) &(currentThread->pc);

	for (int i = 31; i >= 0; i--) {
		char a = *(name++), b = *(name++);

		printf("  %c%c=%08x", a, b, *(reg++));
		if (!(i % 4))
			putchar('\n');
	}

	printf("Stack dump:\n");

	uint32_t *addr = ((uint32_t *) currentThread->sp) - 7;
	uint32_t *end  = ((uint32_t *) currentThread->sp) + 7;

	for (; addr <= end; addr++) {
		char arrow = (((uint32_t) addr) == currentThread->sp) ? '>' : ' ';

		printf("%c %08x: %08x\n", arrow, addr, *addr);
	}
#endif

	for (;;)
		__asm__ volatile("");
}
