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
#include "common/fs/file.hpp"
#include "common/util/hash.hpp"
#include "common/util/misc.hpp"

namespace args {

/* Command line argument parsers */

static constexpr size_t MAX_LAUNCHER_FRAGMENTS = 64;

class CommonArgs {
public:
	int baudRate;

	CommonArgs(void);
	bool parseArgument(const char *arg);
};

class MainArgs : public CommonArgs {
public:
	int        screenWidth, screenHeight;
	bool       forceInterlace;
	const void *resourcePtr;
	size_t     resourceLength;

	MainArgs(void);
	bool parseArgument(const char *arg);
};

class ExecutableLauncherArgs : public CommonArgs {
public:
	void *loadAddress;
	void *entryPoint, *initialGP, *stackTop;

	util::Hash deviceType;
	int        deviceIndex;

	size_t           numFragments;
	fs::FileFragment fragments[MAX_LAUNCHER_FRAGMENTS];

	size_t     numArgs;
	const char *executableArgs[util::MAX_EXECUTABLE_ARGS];

	ExecutableLauncherArgs(void);
	bool parseArgument(const char *arg);
};

}
