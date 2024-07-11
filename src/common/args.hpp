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

#include <stddef.h>
#include "common/file/file.hpp"
#include "common/util.hpp"

namespace args {

/* Command line argument parsers */

static constexpr char VALUE_SEPARATOR = '=';

static constexpr int DEFAULT_BAUD_RATE     = 115200;
static constexpr int DEFAULT_SCREEN_WIDTH  = 320;
static constexpr int DEFAULT_SCREEN_HEIGHT = 240;

static constexpr size_t MAX_LAUNCHER_FRAGMENTS = 64;

class CommonArgs {
public:
	int baudRate;

#ifdef NDEBUG
	inline CommonArgs(void)
	: baudRate(0) {}
#else
	// Enable serial port logging by default in debug builds.
	inline CommonArgs(void)
	: baudRate(DEFAULT_BAUD_RATE) {}
#endif

	bool parseArgument(const char *arg);
};

class MainArgs : public CommonArgs {
public:
	int        screenWidth, screenHeight;
	bool       forceInterlace;
	const void *resourcePtr;
	size_t     resourceLength;

	inline MainArgs(void)
	: screenWidth(DEFAULT_SCREEN_WIDTH), screenHeight(DEFAULT_SCREEN_HEIGHT),
	forceInterlace(false), resourcePtr(nullptr), resourceLength(0) {}

	bool parseArgument(const char *arg);
};

class ExecutableLauncherArgs : public CommonArgs {
public:
	void *entryPoint, *initialGP, *stackTop;

	void *loadAddress;
	int  device; // 0-63 = flash, -1 or -2 = IDE

	size_t             numArgs, numFragments;
	const char         *executableArgs[util::MAX_EXECUTABLE_ARGS];
	file::FileFragment fragments[MAX_LAUNCHER_FRAGMENTS];

	inline ExecutableLauncherArgs(void)
	: entryPoint(nullptr), initialGP(nullptr), stackTop(nullptr),
	loadAddress(nullptr), device(0), numArgs(0), numFragments(0) {}

	bool parseArgument(const char *arg);
};

}
