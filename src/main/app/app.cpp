
#include <stddef.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/file9660.hpp"
#include "common/filefat.hpp"
#include "common/filemisc.hpp"
#include "common/filezip.hpp"
#include "common/gpu.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/cart.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

/* Worker status class */

void WorkerStatus::reset(void) {
	status        = WORKER_IDLE;
	progress      = 0;
	progressTotal = 1;
	message       = nullptr;
	nextScreen    = nullptr;
}

void WorkerStatus::update(int part, int total, const char *text) {
	auto enable   = disableInterrupts();
	status        = WORKER_BUSY;
	progress      = part;
	progressTotal = total;

	if (text)
		message = text;
	if (enable)
		enableInterrupts();
}

void WorkerStatus::setStatus(WorkerStatusType value) {
	auto enable = disableInterrupts();
	status      = value;

	if (enable)
		enableInterrupts();
}

void WorkerStatus::setNextScreen(ui::Screen &next, bool goBack) {
	auto enable = disableInterrupts();
	_nextGoBack = goBack;
	_nextScreen = &next;

	if (enable)
		enableInterrupts();
}

void WorkerStatus::finish(void) {
	auto enable = disableInterrupts();
	status      = _nextGoBack ? WORKER_NEXT_BACK : WORKER_NEXT;
	nextScreen  = _nextScreen;

	if (enable)
		enableInterrupts();
}

/* Filesystem manager class */

FileIOManager::FileIOManager(void)
: _resourceFile(nullptr) {
	__builtin_memset(ide, 0, sizeof(ide));

	vfs.mount("resource:", &resource);
#ifndef NDEBUG
	vfs.mount("host:",     &host);
#endif
}

void FileIOManager::initIDE(void) {
	closeIDE();

	char name[6]{ "ide#:" };

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		auto &dev = ide::devices[i];
		name[3]   = i + '0';

		// Note that calling vfs.mount() multiple times will *not* update any
		// already mounted device, so if two hard drives or CD-ROMs are present
		// the hdd:/cdrom: prefix will be assigned to the first one.
		if (dev.flags & ide::DEVICE_ATAPI) {
			auto iso = new file::ISO9660Provider();

			if (!iso->init(i)) {
				delete iso;
				continue;
			}

			ide[i]      = iso;
			bool mapped = vfs.mount("cdrom:", iso, true);

			if (mapped)
				LOG("mapped cdrom: -> %s", name);
		} else {
			auto fat = new file::FATProvider();

			if (!fat->init(i)) {
				delete fat;
				continue;
			}

			ide[i]      = fat;
			bool mapped = vfs.mount("hdd:", fat, true);

			if (mapped)
				LOG("mapped hdd: -> %s", name);
		}

		vfs.mount(name, ide[i], true);
	}
}

void FileIOManager::closeIDE(void) {
	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		if (!ide[i])
			continue;

		ide[i]->close();
		delete ide[i];
		ide[i] = nullptr;
	}
}

bool FileIOManager::loadResourceFile(const char *path) {
	closeResourceFile();

	_resourceFile = vfs.openFile(path, file::READ);

	if (!_resourceFile)
		return false;

	resource.close();
	return resource.init(_resourceFile);
}

void FileIOManager::closeResourceFile(void) {
	if (!_resourceFile)
		return;

	_resourceFile->close();
	delete _resourceFile;
	_resourceFile = nullptr;
}

void FileIOManager::close(void) {
	vfs.close();
	resource.close();
#ifndef NDEBUG
	host.close();
#endif

	closeResourceFile();
	closeIDE();
}

/* App class */

static constexpr size_t _WORKER_STACK_SIZE = 0x20000;

App::App(ui::Context &ctx)
#ifdef ENABLE_LOG_BUFFER
: _logOverlay(_logBuffer),
#else
:
#endif
_ctx(ctx), _cartDriver(nullptr), _cartParser(nullptr), _identified(nullptr) {}

App::~App(void) {
	_unloadCartData();
	_fileIO.close();
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

void App::_setupWorker(bool (App::*func)(void)) {
	LOG("restarting worker, func=0x%08x", func);

	auto enable = disableInterrupts();

	_workerStack.allocate(_WORKER_STACK_SIZE);
	_workerStatus.reset();

	_workerFunction  = func;
	auto stackBottom = _workerStack.as<uint8_t>();

	initThread(
		// This is not how you implement delegates in C++.
		&_workerThread, util::forcedCast<ArgFunction>(&App::_worker), this,
		&stackBottom[(_WORKER_STACK_SIZE - 1) & ~7]
	);
	if (enable)
		enableInterrupts();
}

void App::_setupInterrupts(void) {
	setInterruptHandler(
		util::forcedCast<ArgFunction>(&App::_interruptHandler), this
	);

	IRQ_MASK = 1 << IRQ_VSYNC;
	enableInterrupts();
}

static const char *const _UI_SOUND_PATHS[ui::NUM_UI_SOUNDS]{
	"assets/sounds/startup.vag",   // ui::SOUND_STARTUP
	"assets/sounds/about.vag",     // ui::SOUND_ABOUT_SCREEN
	"assets/sounds/alert.vag",     // ui::SOUND_ALERT
	"assets/sounds/move.vag",      // ui::SOUND_MOVE
	"assets/sounds/moveleft.vag",  // ui::SOUND_MOVE_LEFT
	"assets/sounds/moveright.vag", // ui::SOUND_MOVE_RIGHT
	"assets/sounds/enter.vag",     // ui::SOUND_ENTER
	"assets/sounds/exit.vag",      // ui::SOUND_EXIT
	"assets/sounds/click.vag",     // ui::SOUND_CLICK
	"assets/sounds/screenshot.vag" // ui::SOUND_SCREENSHOT
};

void App::_loadResources(void) {
	auto &res = _fileIO.resource;

	res.loadTIM(_background.tile,     "assets/textures/background.tim");
	res.loadTIM(_ctx.font.image,      "assets/textures/font.tim");
	res.loadStruct(_ctx.font.metrics, "assets/textures/font.metrics");
	res.loadStruct(_ctx.colors,       "assets/app.palette");
	res.loadData(_stringTable,        "assets/app.strings");

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

bool App::_getNumberedPath(char *output, size_t length, const char *path) {
	file::FileInfo info;
	int            index = 0;

	do {
		if (++index > 9999)
			return false;

		snprintf(output, length, path, index);
	} while (_fileIO.vfs.getFileInfo(info, output));

	return true;
}

bool App::_takeScreenshot(void) {
	char path[32];

	if (!_createDataDirectory())
		return false;
	if (!_getNumberedPath(path, sizeof(path), EXTERNAL_DATA_DIR "/shot%04d.bmp"))
		return false;

	gpu::RectWH clip;

	_ctx.gpuCtx.getVRAMClipRect(clip);
	if (!_fileIO.vfs.saveVRAMBMP(clip, path))
		return false;

	LOG("%s saved", path);
	return true;
}

void App::_worker(void) {
	if (_workerFunction) {
		(this->*_workerFunction)();
		_workerStatus.finish();
	}

	// Do nothing while waiting for vblank once the task is done.
	for (;;)
		__asm__ volatile("");
}

void App::_interruptHandler(void) {
	if (acknowledgeInterrupt(IRQ_VSYNC)) {
		_ctx.tick();

		if (_workerStatus.status != WORKER_REBOOT)
			io::clearWatchdog();
		if (gpu::isIdle() && (_workerStatus.status != WORKER_BUSY_SUSPEND))
			switchThread(nullptr);
	}
}

[[noreturn]] void App::run(const void *resourcePtr, size_t resourceLength) {
#ifdef ENABLE_LOG_BUFFER
	util::logger.setLogBuffer(&_logBuffer);
#endif

	LOG("build " VERSION_STRING " (" __DATE__ " " __TIME__ ")");
	LOG("(C) 2022-2024 spicyjpeg");

	_ctx.screenData = this;

	_fileIO.resource.init(resourcePtr, resourceLength);
	_loadResources();

#ifdef NDEBUG
	_workerStatus.setNextScreen(_warningScreen);
#else
	// Skip the warning screen in debug builds.
	_workerStatus.setNextScreen(_buttonMappingScreen);
#endif
	_setupWorker(&App::_ideInitWorker);
	_setupInterrupts();

	char dateString[24];

	_textOverlay.leftText       = dateString;
	_textOverlay.rightText      = "v" VERSION_STRING;
	_screenshotOverlay.callback = [](ui::Context &ctx) -> bool {
		return APP->_takeScreenshot();
	};

	_ctx.backgrounds[0] = &_background;
	_ctx.backgrounds[1] = &_textOverlay;
#ifdef ENABLE_LOG_BUFFER
	_ctx.overlays[0]    = &_logOverlay;
#endif
	_ctx.overlays[1]    = &_screenshotOverlay;
	_ctx.show(_workerStatusScreen);

	for (;;) {
		util::Date date;

		io::getRTCTime(date);
		date.toString(dateString);

		_ctx.update();
		_ctx.draw();

		switchThreadImmediate(&_workerThread);
		_ctx.gpuCtx.flip();
	}
}
