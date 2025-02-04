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

#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "ps1/system.h"

namespace util {

/* Date and time class */

class Date {
public:
	uint16_t year;
	uint8_t  month, day, hour, minute, second;

	inline void reset(void) {
		year   = 2025;
		month  = 1;
		day    = 1;
		hour   = 0;
		minute = 0;
		second = 0;
	}

	bool isValid(void) const;
	bool isLeapYear(void) const;
	int getDayOfWeek(void) const;
	int getMonthDayCount(void) const;
	uint32_t toDOSTime(void) const;
	size_t toString(char *output) const;
};

/* Critical section and mutex helpers */

class CriticalSection {
private:
	bool _enable;

public:
	inline CriticalSection(void) {
		_enable = disableInterrupts();
	}
	inline ~CriticalSection(void) {
		if (_enable)
			enableInterrupts();
	}
};

class ThreadCriticalSection {
public:
	inline ThreadCriticalSection(void) {
		bool enable = disableInterrupts();

		assert(enable);
		(void) enable;
	}
	inline ~ThreadCriticalSection(void) {
		enableInterrupts();
	}
};

template<typename T> class MutexFlags {
private:
	T _value;

public:
	inline MutexFlags(void)
	: _value(0) {}

	bool lock(T flags, int timeout = 0);
	void unlock(T flags);
};

template<typename T> class MutexLock {
private:
	MutexFlags<T> &_mutex;
	T             _flags;

public:
	bool locked;

	inline MutexLock(MutexFlags<T> &mutex, T flags, int timeout = 0)
	: _mutex(mutex), _flags(flags) {
		locked = _mutex.lock(_flags, timeout);
	}
	inline ~MutexLock(void) {
		_mutex.unlock(_flags);
	}
};

/* PS1 executable loader */

static constexpr size_t EXECUTABLE_BODY_OFFSET = 2048;
static constexpr size_t MAX_EXECUTABLE_ARGS    = 32;

class ExecutableHeader {
public:
	uint32_t magic[4];

	uint32_t entryPoint, initialGP;
	uint32_t textOffset, textLength;
	uint32_t dataOffset, dataLength;
	uint32_t bssOffset, bssLength;
	uint32_t stackOffset, stackLength;
	uint32_t _reserved[5];

	inline void *getEntryPoint(void) const {
		return reinterpret_cast<void *>(entryPoint);
	}
	inline void *getInitialGP(void) const {
		return reinterpret_cast<void *>(initialGP);
	}
	inline void *getTextPtr(void) const {
		return reinterpret_cast<void *>(textOffset);
	}
	inline void *getStackPtr(void) const {
		return reinterpret_cast<void *>(stackOffset + stackLength);
	}
	inline const char *getRegionString(void) const {
		return reinterpret_cast<const char *>(this + 1);
	}
	inline void relocateText(const void *source) const {
		__builtin_memcpy(getTextPtr(), source, textLength);
	}

	bool validateMagic(void) const;
};

class ExecutableLoader {
private:
	void *_entryPoint, *_initialGP;

	size_t     _numArgs;
	const char **_argListPtr;
	char       *_currentStackPtr;

public:
	inline bool copyArgument(const char *arg) {
		return copyArgument(arg, __builtin_strlen(arg));
	}
	[[noreturn]] inline void run(void) {
		run(_numArgs, _argListPtr);
	}

	ExecutableLoader(void *entryPoint, void *initialGP, void *stackTop);
	bool addArgument(const char *arg);
	bool copyArgument(const char *arg, size_t length);
	bool formatArgument(const char *format, ...);
	[[noreturn]] void run(int rawArgc, const char *const *rawArgv);
};

}
