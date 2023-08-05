
#include <stdio.h>
#include <stdlib.h>
#include "app/app.hpp"
#include "ps1/gpucmd.h"
#include "ps1/system.h"
#include "defs.hpp"
#include "file.hpp"
#include "gpu.hpp"
#include "io.hpp"
#include "spu.hpp"
#include "uibase.hpp"
#include "util.hpp"

extern "C" const uint8_t _resources[];
extern "C" const size_t  _resourcesSize;

static const char _DEFAULT_RESOURCE_ZIP_PATH[]{ "cart_tool_resources.zip" };

static const char *const _UI_SOUND_PATHS[ui::NUM_UI_SOUNDS]{
	"assets/sounds/startup.vag", // ui::SOUND_STARTUP
	"assets/sounds/error.vag",   // ui::SOUND_ERROR
	"assets/sounds/move.vag",    // ui::SOUND_MOVE
	"assets/sounds/enter.vag",   // ui::SOUND_ENTER
	"assets/sounds/exit.vag",    // ui::SOUND_EXIT
	"assets/sounds/click.vag"    // ui::SOUND_CLICK
};

int main(int argc, const char **argv) {
	installExceptionHandler();
	gpu::init();
	spu::init();
	io::init();
	util::initZipCRC32();

	int width = 320, height = 240;

	const char *resPath  = _DEFAULT_RESOURCE_ZIP_PATH;
	const void *resPtr   = nullptr;
	size_t     resLength = 0;

#ifdef ENABLE_ARGV
	for (; argc > 0; argc--) {
		auto arg = *(argv++);

		if (!arg)
			continue;

		switch (util::hash(arg, '=')) {
			case "boot.rom"_h:
				//LOG("boot.rom=%s", &arg[9]);
				break;

			case "boot.from"_h:
				//LOG("boot.from=%s", &arg[10]);
				break;

			case "console"_h:
				initSerialIO(strtol(&arg[8], nullptr, 0));
				util::logger.enableSyslog = true;
				break;

			case "screen.width"_h:
				width = int(strtol(&arg[13], nullptr, 0));
				break;

			case "screen.height"_h:
				height = int(strtol(&arg[14], nullptr, 0));
				break;

			// Allow the default assets to be overridden by passing a path or a
			// pointer to an in-memory ZIP file as a command-line argument.
			case "resources.path"_h:
				resPath = &arg[15];
				break;

			case "resources.ptr"_h:
				resPtr = reinterpret_cast<const void *>(
					strtol(&arg[14], nullptr, 16)
				);
				break;

			case "resources.length"_h:
				resLength = size_t(strtol(&arg[17], nullptr, 16));
				break;
		}
	}
#endif

#ifndef NDEBUG
	// Enable serial port logging by default in debug builds.
	initSerialIO(115200);
	util::logger.enableSyslog = true;
#endif

	LOG("build " VERSION_STRING " (" __DATE__ " " __TIME__ ")");
	LOG("(C) 2022-2023 spicyjpeg");

	io::clearWatchdog();

	file::FATProvider fileProvider;

	// Attempt to initialize the secondary drive first, then in case of failure
	// try to initialize the primary drive instead.
	if (fileProvider.init("1:"))
		goto _fileInitDone;
	if (fileProvider.init("0:"))
		goto _fileInitDone;

_fileInitDone:
	io::clearWatchdog();

	// Load the resource archive, first from memory if a pointer was given and
	// then from the HDD. If both attempts fail, fall back to the archive
	// embedded into the executable.
	file::ZIPProvider resourceProvider;
	file::File        *zipFile;

	if (resPtr && resLength) {
		if (resourceProvider.init(resPtr, resLength))
			goto _resourceInitDone;
	}

	if (fileProvider.fileExists(resPath)) {
		zipFile = fileProvider.openFile(resPath, file::READ);

		if (zipFile) {
			if (resourceProvider.init(zipFile))
				goto _resourceInitDone;
		}
	}

	resourceProvider.init(_resources, _resourcesSize);

_resourceInitDone:
	io::clearWatchdog();

	gpu::Context gpuCtx(GP1_MODE_NTSC, width, height, height > 256);
	ui::Context  uiCtx(gpuCtx);

	ui::TiledBackground background;
	ui::LogOverlay      overlay(util::logger);

	file::StringTable strings;

	if (
		!resourceProvider.loadTIM(
			background.tile, "assets/textures/background.tim"
		) ||
		!resourceProvider.loadTIM(
			uiCtx.font.image, "assets/textures/font.tim"
		) ||
		!resourceProvider.loadStruct(
			uiCtx.font.metrics, "assets/textures/font.metrics"
		) ||
		!resourceProvider.loadData(
			strings, "assets/app.strings"
		)
	) {
		LOG("required assets not found, exiting");
		return 1;
	}

	io::clearWatchdog();

	for (int i = 0; i < ui::NUM_UI_SOUNDS; i++)
		resourceProvider.loadVAG(uiCtx.sounds[i], _UI_SOUND_PATHS[i]);

	io::clearWatchdog();

	background.text = "v" VERSION_STRING;
	uiCtx.setBackgroundLayer(background);
	uiCtx.setOverlayLayer(overlay);

	App app;

	gpu::enableDisplay(true);
	spu::setVolume(0x3fff);
	io::setMiscOutput(io::MISC_SPU_ENABLE, true);
	io::clearWatchdog();

	app.run(uiCtx, resourceProvider, fileProvider, strings);
	return 0;
}
