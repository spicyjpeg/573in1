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
#include <stdlib.h>
#include "common/args.hpp"
#include "common/util.hpp"

namespace args {

/* Command line argument parsers */

static constexpr char _VALUE_SEPARATOR = '=';

static constexpr int _DEFAULT_BAUD_RATE     = 115200;
static constexpr int _DEFAULT_SCREEN_WIDTH  = 320;
static constexpr int _DEFAULT_SCREEN_HEIGHT = 240;

CommonArgs::CommonArgs(void)
#ifdef NDEBUG
: baudRate(0) {}
#else
// Enable serial port logging by default in debug builds.
: baudRate(_DEFAULT_BAUD_RATE) {}
#endif


bool CommonArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, _VALUE_SEPARATOR)) {
#if 0
		case "boot.rom"_h:
			LOG_APP("boot.rom=%s", &arg[9]);
			return true;

		case "boot.from"_h:
			LOG_APP("boot.from=%s", &arg[10]);
			return true;
#endif

		case "console"_h:
			baudRate = int(strtol(&arg[8], nullptr, 0));
			return true;

		default:
			return false;
	}
}

MainArgs::MainArgs(void)
: screenWidth(_DEFAULT_SCREEN_WIDTH), screenHeight(_DEFAULT_SCREEN_HEIGHT),
forceInterlace(false), resourcePtr(nullptr), resourceLength(0) {}

bool MainArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, _VALUE_SEPARATOR)) {
		case "screen.width"_h:
			screenWidth = int(strtol(&arg[13], nullptr, 0));
			return true;

		case "screen.height"_h:
			screenHeight = int(strtol(&arg[14], nullptr, 0));
			return true;

		case "screen.interlace"_h:
			forceInterlace = bool(strtol(&arg[17], nullptr, 0));
			return true;

		// Allow the default assets to be overridden by passing a pointer to an
		// in-memory ZIP file as a command-line argument.
		case "resource.ptr"_h:
			resourcePtr = reinterpret_cast<const void *>(
				strtol(&arg[13], nullptr, 16)
			);
			return true;

		case "resource.length"_h:
			resourceLength = size_t(strtol(&arg[16], nullptr, 16));
			return true;

		default:
			return CommonArgs::parseArgument(arg);
	}
}

ExecutableLauncherArgs::ExecutableLauncherArgs(void)
: entryPoint(nullptr), initialGP(nullptr), stackTop(nullptr),
loadAddress(nullptr), device(0), numArgs(0), numFragments(0) {}

bool ExecutableLauncherArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, _VALUE_SEPARATOR)) {
		case "entry.pc"_h:
			entryPoint = reinterpret_cast<void *>(strtol(&arg[9], nullptr, 16));
			return true;

		case "entry.gp"_h:
			initialGP = reinterpret_cast<void *>(strtol(&arg[9], nullptr, 16));
			return true;

		case "entry.sp"_h:
			stackTop = reinterpret_cast<void *>(strtol(&arg[9], nullptr, 16));
			return true;

		case "load"_h:
			loadAddress = reinterpret_cast<void *>(strtol(&arg[5], nullptr, 16));
			return true;

		case "device"_h:
			device = int(strtol(&arg[7], nullptr, 0));
			return true;

		case "frag"_h:
			if (numFragments >= util::countOf(fragments))
				return false;

			{
				auto &fragment = fragments[numFragments++];
				char *ptr;

				fragment.lba    = strtoll(&arg[5], &ptr,    16);
				fragment.length = strtoll(&ptr[1], nullptr, 16);
			}
			return true;

		case "arg"_h:
			if (numArgs >= util::countOf(executableArgs))
				return false;

			executableArgs[numArgs++] = &arg[4];
			return true;

		default:
			return CommonArgs::parseArgument(arg);
	}
}

}
