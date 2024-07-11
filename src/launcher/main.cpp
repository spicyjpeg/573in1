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

#include <stdint.h>
#include "common/args.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/util.hpp"
#include "ps1/system.h"

extern "C" uint8_t _textStart[];

static constexpr size_t _LOAD_CHUNK_LENGTH = 0x8000;

static int _loadFromFlash(args::ExecutableLauncherArgs &args) {
	io::setFlashBank(args.device);

	// The executable's offset and length are always passed as a single
	// fragment.
	auto ptr    = reinterpret_cast<uintptr_t>(args.loadAddress);
	auto source = uintptr_t(args.fragments[0].lba);
	auto length = size_t(args.fragments[0].length);

	while (length) {
		size_t chunkLength = util::min(length, _LOAD_CHUNK_LENGTH);

		__builtin_memcpy(
			reinterpret_cast<void *>(ptr),
			reinterpret_cast<const void *>(source), chunkLength
		);
		io::clearWatchdog();

		ptr    += chunkLength;
		source += chunkLength;
		length -= chunkLength;
	}

	return 0;
}

static int _loadFromIDE(args::ExecutableLauncherArgs &args) {
	int  drive = -(args.device + 1);
	auto &dev  = ide::devices[drive];

	auto error = dev.enumerate();

	if (error) {
		LOG_APP("drive %d: %s", drive, ide::getErrorString(error));
		return 2;
	}

	io::clearWatchdog();

	size_t sectorSize  = dev.getSectorSize();
	size_t skipSectors = util::EXECUTABLE_BODY_OFFSET / sectorSize;

	auto ptr      = reinterpret_cast<uintptr_t>(args.loadAddress);
	auto fragment = args.fragments;

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

		error = dev.readData(reinterpret_cast<void *>(ptr), lba, length);

		if (error) {
			LOG_APP("drive %d: %s", drive, ide::getErrorString(error));
			return 3;
		}

		io::clearWatchdog();
		ptr += length * sectorSize;
	}

	return 0;
}

int main(int argc, const char **argv) {
	disableInterrupts();
	io::init();

	args::ExecutableLauncherArgs args;

	for (; argc > 0; argc--)
		args.parseArgument(*(argv++));

#if defined(ENABLE_APP_LOGGING) || defined(ENABLE_IDE_LOGGING)
	util::logger.setupSyslog(args.baudRate);
#endif

	if (!args.entryPoint || !args.loadAddress || !args.numFragments) {
		LOG_APP("required arguments missing");
		return 1;
	}

	if (!args.stackTop)
		args.stackTop = _textStart - 16;

	int error = (args.device >= 0)
		? _loadFromFlash(args)
		: _loadFromIDE(args);

	if (error)
		return error;

	// Launch the executable.
	util::ExecutableLoader loader(
		args.entryPoint, args.initialGP, args.stackTop
	);

	auto executableArg = args.executableArgs;

	for (size_t i = args.numArgs; i; i--) {
		if (!loader.copyArgument(*(executableArg++)))
			break;
	}

	flushCache();
	io::clearWatchdog();

	loader.run();
	return 0;
}
