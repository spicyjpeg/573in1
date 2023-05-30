
#include <stddef.h>
#include <stdint.h>
#include "app/app.hpp"
#include "ps1/system.h"
#include "asset.hpp"
#include "cart.hpp"
#include "cartdb.hpp"
#include "io.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* App class */

App::App(void)
: _cart(nullptr), _identified(nullptr), _identifyResult(cartdb::UNIDENTIFIED) {
	_workerStack = new uint8_t[WORKER_STACK_SIZE];
}

App::~App(void) {
	//_dbFile.unload();
	delete[] _workerStack;

	if (_cart)
		delete _cart;
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

	setInterruptMask((1 << IRQ_VBLANK) | (1 << IRQ_GPU));
}

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

	_db.data.unload();
	_cart = cart::createCart();

	if (_cart->chipType) {
		LOG("cart object @ 0x%08x", _cart);
		_workerStatus.update(1, 4, WSTR("App.cartDetectWorker.readCart"));

		_cart->readCartID();
		_cart->readPublicData();

		if (!_loader->loadAsset(_db.data, _CARTDB_PATHS[_cart->chipType])) {
			LOG("failed to load cartdb, type=%d", _cart->chipType);
			goto _cartInitDone;
		}
		if (!_db.init()) {
			_db.data.unload();
			goto _cartInitDone;
		}

		_workerStatus.update(2, 4, WSTR("App.cartDetectWorker.identifyGame"));

		if (_cart->flags & cart::PUBLIC_DATA_OK)
			_identifyResult = _db.identifyCart(*_cart);
		else
			LOG("no public data available");
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
		_cart->readSystemID();
	}

_initDone:
	_workerStatus.finish(_cartInfoScreen);
	_dummyWorker();
}

void App::_cartUnlockWorker(void) {
	_workerStatus.update(0, 2, WSTR("App.cartUnlockWorker.read"));

	//_cart->readPrivateData(); // TODO: implement this

	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identify"));

	if (_cart->flags & cart::PRIVATE_DATA_OK)
		_identifyResult = _db.identifyCart(*_cart);

	_workerStatus.finish(_cartInfoScreen, true);
	_dummyWorker();
}

void App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_cart->toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);

	_workerStatus.finish(_qrCodeScreen);
	_dummyWorker();
}

void App::_interruptHandler(void) {
	if (acknowledgeInterrupt(IRQ_VBLANK)) {
		_ctx->tick();
		io::clearWatchdog();
		switchThread(nullptr);
	}

	if (acknowledgeInterrupt(IRQ_GPU))
		_ctx->gpuCtx.drawNextLayer();
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
