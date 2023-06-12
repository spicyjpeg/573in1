
#include <stdio.h>
#include <stdlib.h>
#include "app/app.hpp"
#include "ps1/gpucmd.h"
#include "ps1/system.h"
#include "vendor/ff.h"
#include "asset.hpp"
#include "defs.hpp"
#include "gpu.hpp"
#include "io.hpp"
#include "spu.hpp"
#include "uibase.hpp"
#include "util.hpp"

extern "C" const uint8_t _resources[];
extern "C" const size_t  _resourcesSize;

int main(int argc, const char **argv) {
	installExceptionHandler();
	gpu::init();
	spu::init();
	io::init();

	int width = 320, height = 240;

	const void *resPtr   = nullptr;
	size_t     resLength = 0;

	char mountPath[16];
	strcpy(mountPath, "1:");

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

			case "mount"_h:
				__builtin_strncpy(mountPath, &arg[6], sizeof(mountPath));
				break;

			case "screen.width"_h:
				width = int(strtol(&arg[13], nullptr, 0));
				break;

			case "screen.height"_h:
				height = int(strtol(&arg[14], nullptr, 0));
				break;

			// Allow the default assets to be overridden by passing a pointer to
			// an in-memory ZIP file as a command-line argument.
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

	FATFS fat;

	auto error = f_mount(&fat, mountPath, true);
	if (error)
		LOG("FAT init error, code=%d", error);

	io::clearWatchdog();

	asset::AssetLoader loader;

	if (resPtr && resLength)
		loader.openMemory(resPtr, resLength);
	if (!loader.ready)
		loader.openFAT("1:/cart_tool/resources.zip");
	//if (!loader.ready)
		//loader.openHost("resources.zip");
	if (!loader.ready)
		loader.openMemory(_resources, _resourcesSize);

	io::clearWatchdog();

	gpu::Context gpuCtx(GP1_MODE_NTSC, width, height, height > 256);
	ui::Context  uiCtx(gpuCtx);

	ui::TiledBackground background;
	ui::LogOverlay      overlay(util::logger);

	asset::StringTable strings;

	if (
		!loader.loadTIM(background.tile,       "assets/textures/background.tim") ||
		!loader.loadTIM(uiCtx.font.image,      "assets/textures/font.tim") ||
		!loader.loadStruct(uiCtx.font.metrics, "assets/textures/font.metrics") ||
		!loader.loadAsset(strings,             "assets/app.strings")
	) {
		LOG("required assets not found, exiting");
		return 1;
	}

	io::clearWatchdog();

	loader.loadVAG(uiCtx.sounds[ui::SOUND_STARTUP], "assets/sounds/startup.vag");
	loader.loadVAG(uiCtx.sounds[ui::SOUND_MOVE],    "assets/sounds/move.vag");
	loader.loadVAG(uiCtx.sounds[ui::SOUND_ENTER],   "assets/sounds/enter.vag");
	loader.loadVAG(uiCtx.sounds[ui::SOUND_EXIT],    "assets/sounds/exit.vag");
	loader.loadVAG(uiCtx.sounds[ui::SOUND_CLICK],   "assets/sounds/click.vag");

	background.text = "v" VERSION_STRING;
	uiCtx.setBackgroundLayer(background);
	uiCtx.setOverlayLayer(overlay);

	App app;

	gpu::enableDisplay(true);
	spu::setVolume(0x3fff);
	io::setMiscOutput(io::MISC_SPU_ENABLE, true);
	io::clearWatchdog();

	app.run(uiCtx, loader, strings);
	return 0;
}
