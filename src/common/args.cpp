
#include <stddef.h>
#include <stdlib.h>
#include "common/args.hpp"
#include "common/util.hpp"

namespace args {

/* Command line argument parsers */

bool CommonArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, VALUE_SEPARATOR)) {
#if 0
		case "boot.rom"_h:
			LOG("boot.rom=%s", &arg[9]);
			return true;

		case "boot.from"_h:
			LOG("boot.from=%s", &arg[10]);
			return true;
#endif

		case "console"_h:
			baudRate = int(strtol(&arg[8], nullptr, 0));
			return true;

		default:
			return false;
	}
}

bool MainArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, VALUE_SEPARATOR)) {
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

bool ExecutableLauncherArgs::parseArgument(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, VALUE_SEPARATOR)) {
		case "launcher.drive"_h:
			drive = &arg[15];
			return true;

		case "launcher.path"_h:
			path = &arg[14];
			return true;

		case "launcher.arg"_h:
			if (argCount >= int(util::countOf(executableArgs)))
				return false;

			executableArgs[argCount++] = &arg[13];
			return true;

		default:
			return CommonArgs::parseArgument(arg);
	}
}

}
