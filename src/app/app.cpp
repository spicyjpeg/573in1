
#include <stddef.h>
#include <stdint.h>
#include "app/app.hpp"
#include "ps1/system.h"
#include "asset.hpp"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "io.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* App class */

void App::_unloadCartData(void) {
	if (_driver) {
		delete _driver;
		_driver = nullptr;
	}
	if (_parser) {
		delete _parser;
		_parser = nullptr;
	}

	_db.unload();
	_identified = nullptr;
}

void App::_setupWorker(void (App::* func)(void)) {
	LOG("starting thread, func=0x%08x", func);

	_workerStatus.reset();
	initThread(
		// This is not how you implement delegates in C++.
		&_workerThread, util::forcedCast<ArgFunction>(func), this,
		&_workerStack[(WORKER_STACK_SIZE - 1) & ~7]
	);
}

void App::_setupInterrupts(void) {
	setInterruptHandler(
		util::forcedCast<ArgFunction>(&App::_interruptHandler), this
	);

	setInterruptMask(1 << IRQ_VBLANK);
}

/* Worker functions */

void App::_dummyWorker(void) {
	for (;;)
		__asm__ volatile("");
}

static const char *const _CARTDB_PATHS[cart::NUM_CHIP_TYPES]{
	nullptr,
	"data/x76f041.cartdb",
	"data/x76f100.cartdb",
	"data/zs01.cartdb"
};

void App::_cartDetectWorker(void) {
	_workerStatus.update(0, 4, WSTR("App.cartDetectWorker.identifyCart"));
	_unloadCartData();

#ifndef NDEBUG
	if (_loader->loadStruct(_dump, "data/test.dump")) {
		LOG("using dummy cart driver");
		_driver = new cart::DummyDriver(_dump);
	} else {
		_driver = cart::newCartDriver(_dump);
	}
#else
	_driver = cart::newCartDriver(_dump);
#endif

	if (_dump.chipType) {
		LOG("cart dump @ 0x%08x", &_dump);
		LOG("cart driver @ 0x%08x", _driver);
		_workerStatus.update(1, 4, WSTR("App.cartDetectWorker.readCart"));

		_driver->readCartID();
		if (!_driver->readPublicData())
			_parser = cart::newCartParser(_dump);

		LOG("cart parser @ 0x%08x", _parser);
		_workerStatus.update(2, 4, WSTR("App.cartDetectWorker.identifyGame"));

		if (!_loader->loadAsset(_db, _CARTDB_PATHS[_dump.chipType])) {
			LOG("%s not found", _CARTDB_PATHS[_dump.chipType]);
			goto _cartInitDone;
		}

		char code[8], region[8];

		if (!_parser)
			goto _cartInitDone;
		if (_parser->getCode(code) && _parser->getRegion(region))
			_identified = _db.lookup(code, region);
	}

_cartInitDone:
	_workerStatus.update(3, 4, WSTR("App.cartDetectWorker.readDigitalIO"));

	if (io::isDigitalIOPresent()) {
		asset::Asset file;
		bool         ready;

		if (!_loader->loadAsset(file, "data/fpga.bit")) {
			LOG("failed to load data/fpga.bit");
			goto _initDone;
		}

		ready = io::loadBitstream(
			reinterpret_cast<const uint8_t *>(file.ptr), file.length
		);
		file.unload();

		if (!ready) {
			LOG("bitstream upload failed");
			goto _initDone;
		}

		delayMicroseconds(5000); // Probably not necessary
		io::initKonamiBitstream();
		_driver->readSystemID();
	}

_initDone:
	_workerStatus.finish(_cartInfoScreen);
	_dummyWorker();
}

void App::_cartUnlockWorker(void) {
	_workerStatus.update(0, 2, WSTR("App.cartUnlockWorker.read"));

	if (_driver->readPrivateData()) {
		_workerStatus.finish(_unlockErrorScreen);
		_dummyWorker();
		return;
	}

	if (_parser)
		delete _parser;

	_parser = cart::newCartParser(_dump);
	if (!_parser)
		goto _unlockDone;

	LOG("cart parser @ 0x%08x", _parser);
	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (_parser->getCode(code) && _parser->getRegion(region))
		_identified = _db.lookup(code, region);

_unlockDone:
	_workerStatus.finish(_cartInfoScreen, true);
	_dummyWorker();
}

void App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_dump.toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);

	_workerStatus.finish(_qrCodeScreen);
	_dummyWorker();
}

/* Misc. functions */

void App::_interruptHandler(void) {
	if (acknowledgeInterrupt(IRQ_VBLANK)) {
		_ctx->tick();
		io::clearWatchdog();

		if (gpu::isIdle())
			switchThread(nullptr);
	}
}

void App::run(
	ui::Context &ctx, asset::AssetLoader &loader, asset::StringTable &strings
) {
	LOG("starting app @ 0x%08x", this);

	_ctx     = &ctx;
	_loader  = &loader;
	_strings = &strings;

	ctx.screenData = this;
	ctx.show(_warningScreen);
	ctx.sounds[ui::SOUND_STARTUP].play();

	_setupWorker(&App::_dummyWorker);
	_setupInterrupts();

	for (;;) {
		ctx.update();
		ctx.draw();

		switchThreadImmediate(&_workerThread);
		ctx.gpuCtx.flip();
	}
}
