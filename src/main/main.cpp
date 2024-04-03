
#include <stdio.h>
#include <stdlib.h>
#include "common/file.hpp"
#include "common/gpu.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"
#include "ps1/system.h"

extern "C" const uint8_t _resources[];
extern "C" const size_t  _resourcesSize;

class Settings {
public:
	int        width, height;
	bool       forceInterlace;
	int        baudRate;
	const void *resPtr;
	size_t     resLength;

	inline Settings(void)
	: width(320), height(240), forceInterlace(false), baudRate(0),
	resPtr(nullptr), resLength(0) {}

	bool parse(const char *arg);
};

bool Settings::parse(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, '=')) {
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

		case "screen.width"_h:
			width = int(strtol(&arg[13], nullptr, 0));
			return true;

		case "screen.height"_h:
			height = int(strtol(&arg[14], nullptr, 0));
			return true;

		case "screen.interlace"_h:
			forceInterlace = bool(strtol(&arg[17], nullptr, 0));
			return true;

		// Allow the default assets to be overridden by passing a pointer to an
		// in-memory ZIP file as a command-line argument.
		case "resources.ptr"_h:
			resPtr = reinterpret_cast<const void *>(
				strtol(&arg[14], nullptr, 16)
			);
			return true;

		case "resources.length"_h:
			resLength = size_t(strtol(&arg[17], nullptr, 16));
			return true;

		default:
			return false;
	}
}

int main(int argc, const char **argv) {
	installExceptionHandler();
	gpu::init();
	spu::init();
	io::init();
	util::initZipCRC32();

	Settings settings;

#ifndef NDEBUG
	// Enable serial port logging by default in debug builds.
	settings.baudRate = 115200;
#endif

#ifdef ENABLE_ARGV
	for (; argc > 0; argc--)
		settings.parse(*(argv++));
#endif

	util::logger.setupSyslog(settings.baudRate);

	// Load the resource archive, first from memory if a pointer was given and
	// then from the HDD. If both attempts fail, fall back to the archive
	// embedded into the executable.
	auto resourceProvider = new file::ZIPProvider;

	if (settings.resPtr && settings.resLength) {
		if (resourceProvider->init(settings.resPtr, settings.resLength))
			goto _resourceInitDone;
	}

	resourceProvider->init(_resources, _resourcesSize);

_resourceInitDone:
	io::clearWatchdog();

	auto gpuCtx = new gpu::Context(
		GP1_MODE_NTSC, settings.width, settings.height, settings.forceInterlace
	);
	auto uiCtx  = new ui::Context(*gpuCtx);
	auto app    = new App(*uiCtx, *resourceProvider);

	gpu::enableDisplay(true);
	spu::setMasterVolume(spu::MAX_VOLUME);
	io::setMiscOutput(io::MISC_SPU_ENABLE, true);
	app->run();

	delete app;
	delete uiCtx;
	delete gpuCtx;

	uninstallExceptionHandler();
	return 0;
}
