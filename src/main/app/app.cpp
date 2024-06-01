
#include <stddef.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/file9660.hpp"
#include "common/filefat.hpp"
#include "common/filemisc.hpp"
#include "common/filezip.hpp"
#include "common/gpu.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/cart.hpp"
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

ui::Screen &WorkerStatus::setNextScreen(ui::Screen &next, bool goBack) {
	auto enable  = disableInterrupts();
	auto oldNext = nextScreen;
	nextScreen   = &next;
	nextGoBack   = goBack;

	if (enable)
		enableInterrupts();

	return *oldNext;
}

WorkerStatusType WorkerStatus::setStatus(WorkerStatusType value) {
	auto enable    = disableInterrupts();
	auto oldStatus = status;
	status         = value;

	if (enable)
		enableInterrupts();

	return oldStatus;
}

/* Filesystem manager class */

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

	char name[6]{ "ide#:" };

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		auto &dev = ide::devices[i];
		name[3]   = i + '0';

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

		vfs.mount(name, ide[i], true);
	}
}

void FileIOManager::closeIDE(void) {
	char name[6]{ "ide#:" };

	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		if (ide[i]) {
			ide[i]->close();
			delete ide[i];
			ide[i] = nullptr;
		}

		name[3] = i + '0';
		vfs.unmount(name);
	}
}

bool FileIOManager::loadResourceFile(const char *path) {
	closeResourceFile();

	if (path)
		_resourceFile = vfs.openFile(path, file::READ);

	// Fall back to the default in-memory resource archive in case of failure.
	if (_resourceFile) {
		if (resource.init(_resourceFile))
			return true;
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

void FileIOManager::close(void) {
	vfs.close();
	resource.close();
#ifdef ENABLE_PCDRV
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
	char path[file::MAX_PATH_LENGTH];

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

void App::_runWorker(
	bool (App::*func)(void), ui::Screen &next, bool goBack, bool playSound
) {
	auto enable = disableInterrupts();

	_workerStatus.reset(next, goBack);
	_workerStack.allocate(_WORKER_STACK_SIZE);

	_workerFunction  = func;
	auto stackBottom = _workerStack.as<uint8_t>();

	initThread(
		&_workerThread, util::forcedCast<ArgFunction>(&App::_worker), this,
		&stackBottom[(_WORKER_STACK_SIZE - 1) & ~7]
	);
	if (enable)
		enableInterrupts();

	_ctx.show(_workerStatusScreen, false, playSound);
}

void App::_worker(void) {
	if (_workerFunction) {
		(this->*_workerFunction)();
		_workerStatus.setStatus(WORKER_DONE);
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

	_ctx.screenData        = this;
	_fileIO.resourcePtr    = resourcePtr;
	_fileIO.resourceLength = resourceLength;

	_fileIO.loadResourceFile(nullptr);
	_loadResources();

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

	_runWorker(&App::_ideInitWorker, _warningScreen);
	_setupInterrupts();
	_ctx.sounds[ui::SOUND_STARTUP].play();

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
