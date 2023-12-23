
#include "app/app.hpp"
#include "ps1/system.h"
#include "cart.hpp"
#include "defs.hpp"
#include "file.hpp"
#include "io.hpp"
#include "uibase.hpp"
#include "util.hpp"

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

/* App class */

App::~App(void) {
	_unloadCartData();

	delete[] _workerStack;

	if (_resourceFile) {
		//_resourceFile->close();
		delete _resourceFile;
	}
}

void App::_unloadCartData(void) {
	if (_driver) {
		delete _driver;
		_driver = nullptr;
	}
	if (_parser) {
		delete _parser;
		_parser = nullptr;
	}

	_dump.chipType = cart::NONE;
	_dump.flags    = 0;
	_dump.clearIdentifiers();
	_dump.clearData();

	_identified    = nullptr;
	//_selectedEntry = nullptr;
}

void App::_setupWorker(bool (App::*func)(void)) {
	LOG("restarting worker, func=0x%08x", func);

	auto enable = disableInterrupts();

	_workerStatus.reset();
	_workerFunction = func;

	initThread(
		// This is not how you implement delegates in C++.
		&_workerThread, util::forcedCast<ArgFunction>(&App::_worker), this,
		&_workerStack[(WORKER_STACK_SIZE - 1) & ~7]
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
	"assets/sounds/startup.vag", // ui::SOUND_STARTUP
	"assets/sounds/alert.vag",   // ui::SOUND_ALERT
	"assets/sounds/move.vag",    // ui::SOUND_MOVE
	"assets/sounds/enter.vag",   // ui::SOUND_ENTER
	"assets/sounds/exit.vag",    // ui::SOUND_EXIT
	"assets/sounds/click.vag"    // ui::SOUND_CLICK
};

void App::_loadResources(void) {
	_resourceProvider.loadTIM(_backgroundLayer.tile, "assets/textures/background.tim");
	_resourceProvider.loadTIM(_ctx.font.image,       "assets/textures/font.tim");
	_resourceProvider.loadStruct(_ctx.font.metrics,  "assets/textures/font.metrics");
	_resourceProvider.loadStruct(_ctx.colors,        "assets/app.palette");
	_resourceProvider.loadData(_stringTable,         "assets/app.strings");

	for (int i = 0; i < ui::NUM_UI_SOUNDS; i++)
		_resourceProvider.loadVAG(_ctx.sounds[i], _UI_SOUND_PATHS[i]);
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

[[noreturn]] void App::run(void) {
	LOG("starting app @ 0x%08x", this);

	_ctx.screenData = this;
	_setupWorker(&App::_startupWorker);
	_setupInterrupts();
	_loadResources();

	_backgroundLayer.text = "v" VERSION_STRING;
	_ctx.background       = &_backgroundLayer;
#ifdef ENABLE_LOGGING
	_ctx.overlay          = &_overlayLayer;
#endif
	_ctx.show(_workerStatusScreen);

	for (;;) {
		_ctx.update();
		_ctx.draw();

		switchThreadImmediate(&_workerThread);
		_ctx.gpuCtx.flip();
	}
}
