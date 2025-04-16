/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "common/gpu.hpp"
#include "common/spu.hpp"
#include "main/app/app.hpp"
#include "main/app/fileio.hpp"
#include "main/app/threads.hpp"
#include "main/workers/miscworkers.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

/* App class */

static constexpr size_t _WORKER_STACK_SIZE = 0x8000;

static constexpr int _SPLASH_SCREEN_TIMEOUT = 5;

App::App(ui::Context &ctx)
:
#ifdef ENABLE_LOG_BUFFER
	_logOverlay(_logBuffer),
#endif
	_ctx(ctx),
	_cartParser(nullptr),
	_identified(nullptr) {}

App::~App(void) {
	_unloadCartData();
}

void App::_unloadCartData(void) {
	if (_cartParser) {
		delete _cartParser;
		_cartParser = nullptr;
	}

	_cartDump.chipType = cart::NONE;
	_cartDump.flags    = 0;
	_cartDump.clearIdentifiers();
	util::clear(_cartDump.data);

	_identified    = nullptr;
#if 0
	_selectedEntry = nullptr;
#endif
}

void App::_updateOverlays(void) {
	// Date and time overlay
	static char dateString[24];
	util::Date  date;

	sys573::getRTCTime(date);
	date.toString(dateString);

	_textOverlay.leftText = dateString;

	// Splash screen overlay
	int timeout = _ctx.gpuCtx.refreshRate * _SPLASH_SCREEN_TIMEOUT;

	__atomic_signal_fence(__ATOMIC_ACQUIRE);

	if (!(_workerFlags & WORKER_BUSY) || (_ctx.time > timeout))
		_splashOverlay.hide(_ctx);

#ifdef ENABLE_LOG_BUFFER
	// Log overlay
	if (
		_ctx.buttons.released(ui::BTN_DEBUG) &&
		!_ctx.buttons.longReleased(ui::BTN_DEBUG)
	)
		_logOverlay.toggle(_ctx);
#endif

	// Screenshot overlay
	if (_ctx.buttons.longPressed(ui::BTN_DEBUG)) {
		if (_takeScreenshot())
			_screenshotOverlay.animate(_ctx);
	}
}

/* App filesystem functions */

static const char *const _UI_SOUND_PATHS[ui::NUM_UI_SOUNDS]{
	"res:/assets/sounds/startup.vag",    // ui::SOUND_STARTUP
	"res:/assets/sounds/startupalt.vag", // ui::SOUND_STARTUP_ALT
	"res:/assets/sounds/about.vag",      // ui::SOUND_ABOUT_SCREEN
	"res:/assets/sounds/alert.vag",      // ui::SOUND_ALERT
	"res:/assets/sounds/move.vag",       // ui::SOUND_MOVE
	"res:/assets/sounds/enter.vag",      // ui::SOUND_ENTER
	"res:/assets/sounds/exit.vag",       // ui::SOUND_EXIT
	"res:/assets/sounds/click.vag",      // ui::SOUND_CLICK
	"res:/assets/sounds/screenshot.vag"  // ui::SOUND_SCREENSHOT
};

void App::_loadResources(void) {
	_fileIO.loadStruct(_ctx.colors,       "res:/assets/palette.dat");
	_fileIO.loadTIM(_background.tile,     "res:/assets/textures/background.tim");
	_fileIO.loadTIM(_ctx.font.image,      "res:/assets/textures/font.tim");
	_fileIO.loadData(_ctx.font.metrics,   "res:/assets/textures/font.metrics");
	_fileIO.loadTIM(_splashOverlay.image, "res:/assets/textures/splash.tim");
	_fileIO.loadData(_stringTable,        "res:/assets/lang/en.lang");

	uint32_t spuOffset = spu::DUMMY_BLOCK_END;

	for (int i = 0; i < ui::NUM_UI_SOUNDS; i++)
		spuOffset += _fileIO.loadVAG(
			_ctx.sounds[i],
			spuOffset,
			_UI_SOUND_PATHS[i]
		);
}

bool App::_createDataDirectory(void) {
	fs::FileInfo info;

	if (!_fileIO.getFileInfo(info, EXTERNAL_DATA_DIR))
		return _fileIO.createDirectory(EXTERNAL_DATA_DIR);
	if (info.attributes & fs::DIRECTORY)
		return true;

	return false;
}

bool App::_takeScreenshot(void) {
	char        path[fs::MAX_PATH_LENGTH];
	gpu::RectWH clip;

	if (!_createDataDirectory())
		return false;
	if (!_fileIO.getNumberedPath(
		path,
		sizeof(path),
		EXTERNAL_DATA_DIR "/shot%04d.bmp"
	))
		return false;

	_ctx.gpuCtx.getVRAMClipRect(clip);

	if (!_fileIO.saveVRAMBMP(clip, path))
		return false;

	LOG_APP("%s saved", path);
	return true;
}

/* App callbacks */

void _appInterruptHandler(void *arg0, void *arg1) {
	auto app = reinterpret_cast<App *>(arg0);

	if (acknowledgeInterrupt(IRQ_VSYNC)) {
		app->_ctx.tick();

		__atomic_signal_fence(__ATOMIC_ACQUIRE);

		if (!(app->_workerFlags & WORKER_REBOOT))
			sys573::clearWatchdog();
		if (!(app->_workerFlags & WORKER_SUSPEND_MAIN) && gpu::isIdle())
			switchThread(nullptr);
	}

	if (acknowledgeInterrupt(IRQ_SPU))
		app->_audioStream.handleInterrupt();

	if (acknowledgeInterrupt(IRQ_PIO)) {
		for (auto &mp : app->_fileIO.mountPoints) {
			if (mp.dev)
				mp.dev->handleInterrupt();
		}
	}
}

void _workerMain(void *arg0, void *arg1) {
	auto app  = reinterpret_cast<App *>(arg0);
	auto func = reinterpret_cast<bool (*)(App &)>(arg1);

	if (func)
		func(*app);

	app->_workerFlags &= ~(WORKER_BUSY | WORKER_SUSPEND_MAIN);
	flushWriteQueue();

	// Do nothing while waiting for vblank once the task is done.
	for (;;)
		__asm__ volatile("");
}

void App::_setupInterrupts(void) {
	setInterruptHandler(&_appInterruptHandler, this, nullptr);

	IRQ_MASK = 0
		| (1 << IRQ_VSYNC)
		| (1 << IRQ_SPU)
		| (1 << IRQ_PIO);
	enableInterrupts();
}

void App::_runWorker(bool (*func)(App &app), bool playSound) {
	{
		util::CriticalSection sec;

		_workerFlags = WORKER_BUSY;

		if (!_workerStack.ptr) {
			_workerStack.allocate(_WORKER_STACK_SIZE);
			assert(_workerStack.ptr);
		}

		auto stackPtr = _workerStack.as<uint8_t>();
		auto stackTop = &stackPtr[(_WORKER_STACK_SIZE - 1) & ~7];

		flushWriteQueue();
		initThread(
			&_workerThread,
			&_workerMain,
			this,
			reinterpret_cast<void *>(func),
			stackTop
		);
		LOG_APP("stack: 0x%08x-0x%08x", stackPtr, stackTop);
	}

	_ctx.show(_workerStatusScreen, false, playSound);
}

/* App main loop */

[[noreturn]] void App::run(const void *resourcePtr, size_t resourceLength) {
#ifdef ENABLE_LOG_BUFFER
	util::logger.setLogBuffer(&_logBuffer);
#endif

	LOG_APP("build " VERSION_STRING " (" __DATE__ " " __TIME__ ")");
	LOG_APP("(C) 2022-2024 spicyjpeg");

	_ctx.screenData        = this;
	_fileIO.resourcePtr    = resourcePtr;
	_fileIO.resourceLength = resourceLength;

	_fileIO.loadResourceFile(nullptr);
	_loadResources();

	_textOverlay.rightText = "v" VERSION_STRING;

	_ctx.backgrounds[0] = &_background;
	_ctx.backgrounds[1] = &_textOverlay;
	_ctx.overlays[0]    = &_splashOverlay;
#ifdef ENABLE_LOG_BUFFER
	_ctx.overlays[1]    = &_logOverlay;
#endif
	_ctx.overlays[2]    = &_screenshotOverlay;

	_audioStream.init(&_workerThread);
	_runWorker(&startupWorker);
	_setupInterrupts();

	util::Date date;

	sys573::getRTCTime(date);

	auto &sound = ((date.month == 11) && (date.day == 20))
		? _ctx.sounds[ui::SOUND_STARTUP_ALT]
		: _ctx.sounds[ui::SOUND_STARTUP];

	_splashOverlay.show(_ctx);
	sound.play();

	for (;;) {
		_ctx.update();
		_updateOverlays();

		_ctx.draw();
		_audioStream.yield();
		_ctx.gpuCtx.flip();
	}
}
