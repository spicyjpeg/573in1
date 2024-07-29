/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/util/hash.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "ps1/system.h"

namespace util {

/* Date and time class */

bool Date::isValid(void) const {
	if ((hour > 23) || (minute > 59) || (second > 59))
		return false;
	if ((month < 1) || (month > 12))
		return false;
	if ((day < 1) || (day > getMonthDayCount()))
		return false;

	return true;
}

bool Date::isLeapYear(void) const {
	if (year % 4)
		return false;
	if (!(year % 100) && (year % 400))
		return false;

	return true;
}

int Date::getDayOfWeek(void) const {
	// See https://datatracker.ietf.org/doc/html/rfc3339#appendix-B
	int _year = year, _month = month - 2;

	if (_month <= 0) {
		_month += 12;
		_year--;
	}

	int century = _year / 100;
	_year      %= 100;

	int weekday = 0
		+ day
		+ (_month * 26 - 2) / 10
		+ _year
		+ _year / 4
		+ century / 4
		+ century * 5;

	return weekday % 7;
}

int Date::getMonthDayCount(void) const {
	switch (month) {
		case 2:
			return isLeapYear() ? 29 : 28;

		case 4:
		case 6:
		case 9:
		case 11:
			return 30;

		default:
			return 31;
	}
}

uint32_t Date::toDOSTime(void) const {
	int _year = year - 1980;

	if (!isValid())
		return 0;
	if ((_year < 0) || (_year > 127))
		return 0;

	return 0
		| (_year  << 25)
		| (month  << 21)
		| (day    << 16)
		| (hour   << 11)
		| (minute <<  5)
		| (second >>  1);
}

size_t Date::toString(char *output) const {
	if (!isValid()) {
		*output = 0;
		return 0;
	}

	return sprintf(
		output, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute,
		second
	);
}

/* PS1 executable loader */

bool ExecutableHeader::validateMagic(void) const {
#if 0
	return (
		hash(magic, sizeof(magic)) ==
		hash("PS-X EXE\0\0\0\0\0\0\0\0", sizeof(magic), 0)
	);
#else
	return true
		&& (magic[0] == concatenate('P', 'S', '-', 'X'))
		&& (magic[1] == concatenate(' ', 'E', 'X', 'E'))
		&& !magic[2]
		&& !magic[3]
		&& !(entryPoint % 4)
		&& !(textOffset % 4)
		&& !(textLength % 2048)
		&& !dataLength
		&& !bssLength;
#endif
}

ExecutableLoader::ExecutableLoader(
	void *entryPoint, void *initialGP, void *stackTop
) : _entryPoint(entryPoint), _initialGP(initialGP), _numArgs(0) {
	_argListPtr      = reinterpret_cast<const char **>(uintptr_t(stackTop) & ~7)
		- MAX_EXECUTABLE_ARGS;
	_currentStackPtr = reinterpret_cast<char *>(_argListPtr);
}

bool ExecutableLoader::addArgument(const char *arg) {
	if (_numArgs >= MAX_EXECUTABLE_ARGS)
		return false;

	_argListPtr[_numArgs++] = arg;
	return true;
}

bool ExecutableLoader::copyArgument(const char *arg, size_t length) {
	if (_numArgs >= MAX_EXECUTABLE_ARGS)
		return false;

	// Command-line arguments must be copied to the top of the new stack in
	// order to ensure the executable is going to be able to access them at any
	// time.
	*(--_currentStackPtr) = 0;
	_currentStackPtr     -= length;
	__builtin_memcpy(_currentStackPtr, arg, length);

	_argListPtr[_numArgs++] = _currentStackPtr;
	return true;
}

bool ExecutableLoader::formatArgument(const char *format, ...) {
	char    buffer[64];
	va_list ap;

	va_start(ap, format);
	int length = vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	return copyArgument(buffer, length + 1);
}

[[noreturn]] void ExecutableLoader::run(
	int rawArgc, const char *const *rawArgv
) {
#if 0
	disableInterrupts();
	flushCache();
#endif

	register int               a0  __asm__("a0") = rawArgc;
	register const char *const *a1 __asm__("a1") = rawArgv;
	register void              *gp __asm__("gp") = _initialGP;

	auto stackTop = uintptr_t(_currentStackPtr) & ~7;

	// Changing the stack pointer and return address is not something that
	// should be done in a C++ function, but hopefully it's fine here since
	// we're jumping out right after setting it.
	__asm__ volatile(
		".set push\n"
		".set noreorder\n"
		"li    $ra, %0\n"
		"jr    %1\n"
		"addiu $sp, %2, -8\n"
		".set pop\n"
		:: "i"(DEV2_BASE), "r"(_entryPoint), "r"(stackTop),
		"r"(a0), "r"(a1), "r"(gp)
	);
	__builtin_unreachable();
}

}
