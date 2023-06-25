
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "app/app.hpp"
#include "ps1/system.h"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "file.hpp"
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

	_dump.chipType = cart::NONE;
	_dump.clearIdentifiers();
	_dump.clearData();

	_identified = nullptr;
	_db.destroy();
}

void App::_setupWorker(void (App::*func)(void)) {
	LOG("restarting worker, func=0x%08x", func);

	auto mask = setInterruptMask(0);

	_workerStatus.reset();
	_workerFunction = func;

	initThread(
		// This is not how you implement delegates in C++.
		&_workerThread, util::forcedCast<ArgFunction>(&App::_worker), this,
		&_workerStack[(WORKER_STACK_SIZE - 1) & ~7]
	);
	if (mask)
		setInterruptMask(mask);
}

void App::_setupInterrupts(void) {
	setInterruptHandler(
		util::forcedCast<ArgFunction>(&App::_interruptHandler), this
	);

	setInterruptMask(1 << IRQ_VBLANK);
}

/* Worker functions */

static const char *const _CARTDB_PATHS[cart::NUM_CHIP_TYPES]{
	nullptr,
	"data/x76f041.cartdb",
	"data/x76f100.cartdb",
	"data/zs01.cartdb"
};

void App::_cartDetectWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen);
	_workerStatus.update(0, 4, WSTR("App.cartDetectWorker.identifyCart"));
	_unloadCartData();

#ifdef ENABLE_DUMMY_DRIVER
	if (
		_resourceProvider->loadStruct(_dump, "data/test.573")
		== sizeof(cart::Dump)
	) {
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

		auto error = _driver->readCartID();

		if (error)
			LOG("cart ID error, code=%d", error);

		error = _driver->readPublicData();

		if (error)
			LOG("public data error, code=%d", error);
		else
			_parser = cart::newCartParser(_dump);

		LOG("cart parser @ 0x%08x", _parser);
		_workerStatus.update(2, 4, WSTR("App.cartDetectWorker.identifyGame"));

		if (!_resourceProvider->loadData(_db, _CARTDB_PATHS[_dump.chipType])) {
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
		util::Data bitstream;
		bool       ready;

		if (!_resourceProvider->loadData(bitstream, "data/fpga.bit")) {
			LOG("bitstream unavailable");
			return;
		}

		ready = io::loadBitstream(bitstream.as<uint8_t>(), bitstream.length);
		bitstream.destroy();

		if (!ready) {
			LOG("bitstream upload failed");
			return;
		}

		delayMicroseconds(5000); // Probably not necessary
		io::initKonamiBitstream();
	}

	// This must be outside of the if block above to make sure the system ID
	// gets read with the dummy driver.
	auto error = _driver->readSystemID();

	if (error)
		LOG("system ID error, code=%d", error);
}

void App::_cartUnlockWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen, true);
	_workerStatus.update(0, 2, WSTR("App.cartUnlockWorker.read"));

	auto error = _driver->readPrivateData();

	if (error) {
		LOG("private data error, code=%d", error);

		/*_errorScreen.setMessage(
			_cartInfoScreen, WSTR("App.cartUnlockWorker.error")
		);*/
		_workerStatus.setNextScreen(_errorScreen);
		return;
	}

	if (_parser)
		delete _parser;

	_parser = cart::newCartParser(_dump);

	if (!_parser)
		return;

	LOG("cart parser @ 0x%08x", _parser);
	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (_parser->getCode(code) && _parser->getRegion(region))
		_identified = _db.lookup(code, region);
}

void App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.setNextScreen(_qrCodeScreen);
	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_dump.toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);
}

void App::_hddDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.hddDumpWorker.save"));

	char code[8], region[8], path[32];

	if (_identified && _parser->getCode(code) && _parser->getRegion(region))
		snprintf(path, sizeof(path), "%s%s.573", code, region);
	else
		__builtin_strcpy(path, "unknown.573");

	LOG("saving dump as %s", path);

	if (_fileProvider->saveStruct(_dump, path) != sizeof(cart::Dump)) {
		_errorScreen.setMessage(
			_cartInfoScreen, WSTR("App.hddDumpWorker.error")
		);
		_workerStatus.setNextScreen(_errorScreen);
		return;
	}

	_workerStatus.setNextScreen(_cartInfoScreen);
}

void App::_cartWriteWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartWriteWorker.write"));

	auto error = _driver->writeData();

	_cartDetectWorker();

	if (!error && _identified) {
		_identified->copyKeyTo(_dump.dataKey);
		_cartUnlockWorker();
	}

	if (error) {
		LOG("write error, code=%d", error);

		_errorScreen.setMessage(
			_cartInfoScreen, WSTR("App.cartWriteWorker.error")
		);
		_workerStatus.setNextScreen(_errorScreen);
	}
}

void App::_cartReflashWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartWriteWorker.flash"));

	if (_parser)
		delete _parser;

	_parser  = cart::newCartParser(
		_dump, _reflashEntry->formatType, _reflashEntry->flags
	);
	auto pri = _parser->getIdentifiers();
	auto pub = _parser->getPublicIdentifiers();

	_dump.clearData();

	if (pri) {
		if (_reflashEntry->flags & cart::DATA_HAS_CART_ID)
			pri->cartID.copyFrom(_dump.cartID.data);
		if (_reflashEntry->flags & cart::DATA_HAS_TRACE_ID)
			pri->updateTraceID(
				_reflashEntry->traceIDType, _reflashEntry->traceIDParam
			);
		if (_reflashEntry->flags & cart::DATA_HAS_INSTALL_ID) {
			// The private installation ID seems to be unused on carts with a
			// public data section.
			if (pub)
				pub->setInstallID(_reflashEntry->installIDPrefix);
			else
				pri->setInstallID(_reflashEntry->installIDPrefix);
		}
	}

	_parser->setCode(_reflashEntry->code);
	_parser->setRegion(_reflashEntry->region);
	_parser->setYear(_reflashEntry->year);
	_parser->flush();

	uint8_t key[8];
	auto    error = _driver->writeData();

	if (!error) {
		_reflashEntry->copyKeyTo(key);
		error = _driver->setDataKey(key);
	}

	_cartDetectWorker();

	if (!error) {
		_dump.copyKeyFrom(key);
		_cartUnlockWorker();
	}

	if (error) {
		LOG("write error, code=%d", error);

		_errorScreen.setMessage(
			_cartInfoScreen, WSTR("App.cartReflashWorker.error")
		);
		_workerStatus.setNextScreen(_errorScreen);
	}
}

void App::_cartEraseWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartEraseWorker.erase"));

	auto error = _driver->erase();

	_cartDetectWorker();

	if (!error) {
		_dump.clearKey();
		_cartUnlockWorker();
	}

	if (error) {
		LOG("erase error, code=%d", error);

		_errorScreen.setMessage(
			_cartInfoScreen, WSTR("App.cartEraseWorker.error")
		);
		_workerStatus.setNextScreen(_errorScreen);
	}
}

/* Misc. functions */

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
	if (acknowledgeInterrupt(IRQ_VBLANK)) {
		_ctx->tick();
		io::clearWatchdog();

		if (gpu::isIdle() && (_workerStatus.status != WORKER_BUSY_SUSPEND))
			switchThread(nullptr);
	}
}

void App::run(
	ui::Context &ctx, file::Provider &resourceProvider,
	file::Provider &fileProvider, file::StringTable &stringTable
) {
	LOG("starting app @ 0x%08x", this);

	_ctx              = &ctx;
	_resourceProvider = &resourceProvider;
	_fileProvider     = &fileProvider;
	_stringTable      = &stringTable;

	ctx.screenData = this;

#ifdef NDEBUG
	ctx.show(_warningScreen);
	ctx.sounds[ui::SOUND_STARTUP].play();
#else
	// Skip the warning screen in debug builds.
	ctx.show(_buttonMappingScreen);
#endif

	_setupWorker(nullptr);
	_setupInterrupts();

	for (;;) {
		ctx.update();
		ctx.draw();

		switchThreadImmediate(&_workerThread);
		ctx.gpuCtx.flip();
	}
}
