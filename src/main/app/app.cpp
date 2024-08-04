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
#include <stdio.h>
#include "common/file/fat.hpp"
#include "common/file/file.hpp"
#include "common/file/iso9660.hpp"
#include "common/file/misc.hpp"
#include "common/file/zip.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "common/gpu.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "main/app/app.hpp"
#include "main/cart/cart.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

/* Worker status class */

void WorkerStatus::reset(ui::Screen &next, bool goBack) {
	status        = WORKER_IDLE;
	progress      = 0;
	progressTotal = 1;
	message       = nullptr;
	nextScreen    = &next;
	nextGoBack    = goBack;

	flushWriteQueue();
}

void WorkerStatus::update(int part, int total, const char *text) {
	util::CriticalSection sec;

	status        = WORKER_BUSY;
	progress      = part;
	progressTotal = total;

	if (text)
		message = text;

	flushWriteQueue();
}

ui::Screen &WorkerStatus::setNextScreen(ui::Screen &next, bool goBack) {
	util::CriticalSection sec;

	auto oldNext = nextScreen;
	nextScreen   = &next;
	nextGoBack   = goBack;

	flushWriteQueue();
	return *oldNext;
}

WorkerStatusType WorkerStatus::setStatus(WorkerStatusType value) {
	util::CriticalSection sec;

	auto oldStatus = status;
	status         = value;

	flushWriteQueue();
	return oldStatus;
}

/* Filesystem manager class */

const char *const IDE_MOUNT_POINTS[]{ "ide0:", "ide1:" };

FileIOManager::FileIOManager(void)
: _resourceFile(nullptr), resourcePtr(nullptr), resourceLength(0) {
	__builtin_memset(ide, 0, sizeof(ide));

	vfs.mount("resource:", &resource);
#ifdef ENABLE_PCDRV
	vfs.mount("host:",     &host);
#endif
}

void FileIOManager::initIDE(void) {
	closeIDE();

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		auto &dev = ide::devices[i];

		if (!(dev.flags & ide::DEVICE_READY))
			continue;

		// Note that calling vfs.mount() multiple times will *not* update any
		// already mounted device, so if two hard drives or CD-ROMs are present
		// the hdd:/cdrom: prefix will be assigned to the first one.
		if (dev.flags & ide::DEVICE_ATAPI) {
			auto iso = new file::ISO9660Provider();

			if (!iso->init(i)) {
				delete iso;
				continue;
			}

			ide[i] = iso;
			vfs.mount("cdrom:", iso);
		} else {
			auto fat = new file::FATProvider();

			if (!fat->init(i)) {
				delete fat;
				continue;
			}

			ide[i] = fat;
			vfs.mount("hdd:", fat);
		}

		vfs.mount(IDE_MOUNT_POINTS[i], ide[i], true);
	}
}

void FileIOManager::closeIDE(void) {
	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		if (!ide[i])
			continue;

		ide[i]->close();
		delete ide[i];
		ide[i] = nullptr;

		vfs.unmount(IDE_MOUNT_POINTS[i]);
	}

	vfs.unmount("cdrom:");
	vfs.unmount("hdd:");
}

bool FileIOManager::loadResourceFile(const char *path) {
	closeResourceFile();

	if (path)
		_resourceFile = vfs.openFile(path, file::READ);

	// Fall back to the default in-memory resource archive in case of failure.
	if (_resourceFile) {
		if (resource.init(_resourceFile))
			return true;

		_resourceFile->close();
		delete _resourceFile;
		_resourceFile = nullptr;
	}

	resource.init(resourcePtr, resourceLength);
	return false;
}

void FileIOManager::closeResourceFile(void) {
	resource.close();

	if (_resourceFile) {
		_resourceFile->close();
		delete _resourceFile;
		_resourceFile = nullptr;
	}
}

/* App class */

static constexpr size_t _WORKER_STACK_SIZE = 0x20000;

static constexpr int _SPLASH_SCREEN_TIMEOUT = 5;

App::App(ui::Context &ctx)
#ifdef ENABLE_LOG_BUFFER
: _logOverlay(_logBuffer),
#else
:
#endif
_ctx(ctx), _cartDriver(nullptr), _cartParser(nullptr), _identified(nullptr) {}

App::~App(void) {
	_unloadCartData();
	_workerStack.destroy();
}

void App::_unloadCartData(void) {
	if (_cartDriver) {
		delete _cartDriver;
		_cartDriver = nullptr;
	}
	if (_cartParser) {
		delete _cartParser;
		_cartParser = nullptr;
	}

	_cartDump.chipType = cart::NONE;
	_cartDump.flags    = 0;
	_cartDump.clearIdentifiers();
	util::clear(_cartDump.data);

	_identified    = nullptr;
	//_selectedEntry = nullptr;
}

void App::_setupInterrupts(void) {
	setInterruptHandler(
		util::forcedCast<ArgFunction>(&App::_interruptHandler), this
	);

	IRQ_MASK = 0
		| (1 << IRQ_VSYNC)
		| (1 << IRQ_SPU)
		| (1 << IRQ_PIO);
	enableInterrupts();
}

static const char *const _UI_SOUND_PATHS[ui::NUM_UI_SOUNDS]{
	"assets/sounds/startup.vag",   // ui::SOUND_STARTUP
	"assets/sounds/about.vag",     // ui::SOUND_ABOUT_SCREEN
	"assets/sounds/alert.vag",     // ui::SOUND_ALERT
	"assets/sounds/move.vag",      // ui::SOUND_MOVE
	"assets/sounds/enter.vag",     // ui::SOUND_ENTER
	"assets/sounds/exit.vag",      // ui::SOUND_EXIT
	"assets/sounds/click.vag",     // ui::SOUND_CLICK
	"assets/sounds/screenshot.vag" // ui::SOUND_SCREENSHOT
};

void App::_loadResources(void) {
	auto &res = _fileIO.resource;

	res.loadStruct(_ctx.colors,       "assets/palette.dat");
	res.loadTIM(_background.tile,     "assets/textures/background.tim");
	res.loadTIM(_ctx.font.image,      "assets/textures/font.tim");
	res.loadData(_ctx.font.metrics,   "assets/textures/font.metrics");
	res.loadTIM(_splashOverlay.image, "assets/textures/splash.tim");
	res.loadData(_stringTable,        "assets/lang/en.lang");

	file::currentSPUOffset = spu::DUMMY_BLOCK_END;

	for (int i = 0; i < ui::NUM_UI_SOUNDS; i++)
		res.loadVAG(_ctx.sounds[i], _UI_SOUND_PATHS[i]);
}

bool App::_createDataDirectory(void) {
	file::FileInfo info;

	if (!_fileIO.vfs.getFileInfo(info, EXTERNAL_DATA_DIR))
		return _fileIO.vfs.createDirectory(EXTERNAL_DATA_DIR);
	if (info.attributes & file::DIRECTORY)
		return true;

	return false;
}

bool App::_getNumberedPath(
	char *output, size_t length, const char *path, int maxIndex
) {
	file::FileInfo info;

	// Perform a binary search in order to quickly find the first unused path.
	int low  = 0;
	int high = maxIndex;

	while (low <= high) {
		int index = low + (high - low) / 2;

		snprintf(output, length, path, index);

		if (_fileIO.vfs.getFileInfo(info, output))
			low = index + 1;
		else
			high = index - 1;
	}

	if (low > maxIndex)
		return false;

	snprintf(output, length, path, low);
	return true;
}

bool App::_takeScreenshot(void) {
	char path[file::MAX_PATH_LENGTH];

	if (!_createDataDirectory())
		return false;
	if (!_getNumberedPath(path, sizeof(path), EXTERNAL_DATA_DIR "/shot%04d.bmp"))
		return false;

	gpu::RectWH clip;

	_ctx.gpuCtx.getVRAMClipRect(clip);
	if (!_fileIO.vfs.saveVRAMBMP(clip, path))
		return false;

	LOG_APP("%s saved", path);
	return true;
}

void App::_updateOverlays(void) {
	// Date and time overlay
	static char dateString[24];
	util::Date  date;

	io::getRTCTime(date);
	date.toString(dateString);

	_textOverlay.leftText = dateString;

	// Splash screen overlay
	int timeout = _ctx.gpuCtx.refreshRate * _SPLASH_SCREEN_TIMEOUT;

	__atomic_signal_fence(__ATOMIC_ACQUIRE);

	if ((_workerStatus.status != WORKER_BUSY) || (_ctx.time > timeout))
		_splashOverlay.hide(_ctx);

	// Log overlay
	if (
		_ctx.buttons.released(ui::BTN_DEBUG) &&
		!_ctx.buttons.longReleased(ui::BTN_DEBUG)
	)
		_logOverlay.toggle(_ctx);

	// Screenshot overlay
	if (_ctx.buttons.longPressed(ui::BTN_DEBUG)) {
		if (_takeScreenshot())
			_screenshotOverlay.animate(_ctx);
	}
}

void App::_runWorker(
	bool (App::*func)(void), ui::Screen &next, bool goBack, bool playSound
) {
	{
		util::CriticalSection sec;

		_workerStatus.reset(next, goBack);
		_workerStack.allocate(_WORKER_STACK_SIZE);

		_workerFunction  = func;
		auto stackBottom = _workerStack.as<uint8_t>();

		initThread(
			&_workerThread, util::forcedCast<ArgFunction>(&App::_worker), this,
			&stackBottom[(_WORKER_STACK_SIZE - 1) & ~7]
		);
	}

	_ctx.show(_workerStatusScreen, false, playSound);
}

void App::_worker(void) {
	if (_workerFunction)
		(this->*_workerFunction)();

	_workerStatus.setStatus(WORKER_DONE);

	// Do nothing while waiting for vblank once the task is done.
	for (;;)
		__asm__ volatile("");
}

void App::_interruptHandler(void) {
	if (acknowledgeInterrupt(IRQ_VSYNC)) {
		_ctx.tick();

		__atomic_signal_fence(__ATOMIC_ACQUIRE);

		if (_workerStatus.status != WORKER_REBOOT)
			io::clearWatchdog();
		if (gpu::isIdle() && (_workerStatus.status != WORKER_BUSY_SUSPEND))
			switchThread(nullptr);
	}

	if (acknowledgeInterrupt(IRQ_SPU))
		_ctx.audioStream.handleInterrupt();

	if (acknowledgeInterrupt(IRQ_PIO)) {
		for (auto &dev : ide::devices)
			dev.handleInterrupt();
	}
}

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

	_runWorker(&App::_ideInitWorker, _warningScreen);
	_setupInterrupts();

	_splashOverlay.show(_ctx);
	_ctx.sounds[ui::SOUND_STARTUP].play();

	for (;;) {
		_ctx.update();
		_updateOverlays();

		_ctx.draw();
		switchThreadImmediate(&_workerThread);
		_ctx.gpuCtx.flip();
	}
}
