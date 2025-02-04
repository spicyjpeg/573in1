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
#include "common/blkdev/ata.hpp"
#include "common/blkdev/atapi.hpp"
#include "common/blkdev/device.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/args.hpp"
#include "ps1/system.h"

enum ExitCode {
	NO_ERROR     = 0,
	INVALID_ARGS = 1,
	INIT_FAILED  = 2,
	READ_FAILED  = 3
};

/* Executable loading */

static ExitCode _loadFromFlash(args::ExecutableLauncherArgs &args) {
	// The executable's offset and length are always passed as a single
	// fragment.
	if (args.numFragments != 1)
		return INVALID_ARGS;

	sys573::setFlashBank(args.deviceIndex);
	__builtin_memcpy(
		args.loadAddress,
		reinterpret_cast<const void *>(args.fragments[0].lba),
		size_t(args.fragments[0].length)
	);

	return NO_ERROR;
}

static ExitCode _loadFromBlockDevice(
	args::ExecutableLauncherArgs &args,
	blkdev::Device               &dev
) {
	auto error = dev.enumerate();

	if (error) {
		LOG_APP(
			"drive %d: %s",
			args.deviceIndex,
			blkdev::getErrorString(error)
		);
		return INIT_FAILED;
	}

	auto ptr         = reinterpret_cast<uintptr_t>(args.loadAddress);
	auto fragment    = args.fragments;
	auto skipSectors = util::EXECUTABLE_BODY_OFFSET / dev.sectorLength;

	for (size_t i = args.numFragments; i; i--, fragment++) {
		auto lba    = fragment->lba;
		auto length = fragment->length;

		// Skip the executable header by either shrinking the current fragment
		// or ignoring it altogether.
		if (skipSectors >= length) {
			skipSectors -= length;
			continue;
		}
		if (skipSectors) {
			lba    += skipSectors;
			length -= skipSectors;
		}

		error = dev.read(reinterpret_cast<void *>(ptr), lba, length);

		if (error) {
			LOG_APP(
				"drive %d: %s",
				args.deviceIndex,
				blkdev::getErrorString(error)
			);
			return READ_FAILED;
		}

		ptr += length * dev.sectorLength;
	}

	return NO_ERROR;
}

/* Main */

extern "C" uint8_t _textStart[];

extern "C" void _launcherExceptionVector(void);

int main(int argc, const char **argv) {
	installCustomExceptionHandler(&_launcherExceptionVector);
	sys573::init();

	args::ExecutableLauncherArgs args;

	for (; argc > 0; argc--)
		args.parseArgument(*(argv++));

#if defined(ENABLE_APP_LOGGING) || defined(ENABLE_STORAGE_LOGGING) || defined(ENABLE_FS_LOGGING)
	util::logger.setupSyslog(args.baudRate);
#endif

	if (!args.entryPoint || !args.loadAddress || !args.numFragments) {
		LOG_APP("required arguments missing");
		return INVALID_ARGS;
	}

	if (!args.stackTop)
		args.stackTop = _textStart - 16;

	ExitCode code;

	switch (args.deviceType) {
		case "ata"_h:
			{
				blkdev::ATADevice dev(args.deviceIndex);
				code = _loadFromBlockDevice(args, dev);
			};
			break;

		case "atapi"_h:
			{
				blkdev::ATAPIDevice dev(args.deviceIndex);
				code = _loadFromBlockDevice(args, dev);
			};
			break;

		case "flash"_h:
			code = _loadFromFlash(args);
			break;

		default:
			LOG_APP("invalid device type");
			code = INVALID_ARGS;
	}

	if (code)
		return code;

	// Launch the executable.
	util::ExecutableLoader loader(
		args.entryPoint, args.initialGP, args.stackTop
	);

	auto executableArg = args.executableArgs;

	for (size_t i = args.numArgs; i; i--) {
		if (!loader.copyArgument(*(executableArg++)))
			break;
	}

	LOG_APP("launching executable");
	uninstallExceptionHandler();
	sys573::clearWatchdog();

	loader.run();
	return NO_ERROR;
}
