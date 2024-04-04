
#include "common/args.hpp"
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "launcher/launcher.hpp"

static const uint32_t _EXECUTABLE_OFFSETS[]{
	0,
	rom::FLASH_EXECUTABLE_OFFSET,
	util::EXECUTABLE_BODY_OFFSET
};

int main(int argc, const char **argv) {
	io::init();

	ExecutableLauncher launcher;

	for (; argc > 0; argc--)
		launcher.args.parseArgument(*(argv++));

#ifdef ENABLE_LOGGING
	util::logger.setupSyslog(launcher.args.baudRate);
#endif

	auto error = launcher.openFile();
	io::clearWatchdog();

	if (error)
		goto _exit;

	// Check for the presence of an executable at several different offsets
	// within the file before giving up.
	for (auto offset : _EXECUTABLE_OFFSETS) {
		error = launcher.parseHeader(offset);
		io::clearWatchdog();

		if (error == INVALID_FILE)
			continue;
		if (error)
			goto _exit;

		error = launcher.loadBody();
		io::clearWatchdog();

		if (error)
			goto _exit;

		launcher.closeFile();
		launcher.run();
	}

_exit:
	launcher.closeFile();
	return error;
}
