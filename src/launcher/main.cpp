
#include <stdint.h>
#include "common/args.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/util.hpp"

extern "C" uint8_t _textStart[];

int main(int argc, const char **argv) {
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

	auto &dev = ide::devices[args.drive];

	if (dev.enumerate()) {
		LOG_APP("drive %d initialization failed", args.drive);
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

		if (dev.readData(reinterpret_cast<void *>(ptr), lba, length)) {
			LOG_APP("read failed, lba=0x%08x", lba);
			return 3;
		}

		io::clearWatchdog();
		ptr += length * sectorSize;
	}

	// Launch the executable.
	util::ExecutableLoader loader(
		args.entryPoint, args.initialGP, args.stackTop
	);

	auto executableArg = args.executableArgs;

	for (size_t i = args.numArgs; i; i--)
		loader.copyArgument(*(executableArg++));

	loader.run();
	return 0;
}
