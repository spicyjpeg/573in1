
#pragma once

#include <stdint.h>
#include "app/actions.hpp"
#include "app/misc.hpp"
#include "app/unlock.hpp"
#include "ps1/system.h"
#include "asset.hpp"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "uibase.hpp"

/* Worker status class */

// This class is used by the worker thread to report its current status back to
// the main thread and the WorkerStatusScreen.
class WorkerStatus {
public:
	volatile int  progress, progressTotal;
	volatile bool nextGoBack;

	const char *volatile message;
	ui::Screen *volatile nextScreen;

	inline void update(int part, int total) {
		auto mask     = setInterruptMask(0);
		progress      = part;
		progressTotal = total;

		if (mask)
			setInterruptMask(mask);
	}
	inline void update(int part, int total, const char *text) {
		auto mask     = setInterruptMask(0);
		progress      = part;
		progressTotal = total;
		message       = text;

		if (mask)
			setInterruptMask(mask);
	}
	inline void finish(ui::Screen &next, bool goBack = false) {
		auto mask     = setInterruptMask(0);
		nextScreen = &next;
		nextGoBack = goBack;

		if (mask)
			setInterruptMask(mask);
	}
	inline void reset(void) {
		progress      = 0;
		progressTotal = 1;
		nextScreen    = nullptr;
	}
};

/* App class */

static constexpr size_t WORKER_STACK_SIZE = 0x10000;

class App {
	friend class WorkerStatusScreen;
	friend class WarningScreen;
	friend class ButtonMappingScreen;
	friend class CartInfoScreen;
	friend class UnlockKeyScreen;
	friend class UnlockConfirmScreen;
	friend class UnlockErrorScreen;
	friend class CartActionsScreen;
	friend class QRCodeScreen;

private:
	WorkerStatusScreen  _workerStatusScreen;
	WarningScreen       _warningScreen;
	ButtonMappingScreen	_buttonMappingScreen;
	CartInfoScreen      _cartInfoScreen;
	UnlockKeyScreen     _unlockKeyScreen;
	UnlockConfirmScreen _unlockConfirmScreen;
	UnlockErrorScreen   _unlockErrorScreen;
	CartActionsScreen   _cartActionsScreen;
	QRCodeScreen        _qrCodeScreen;

	ui::Context        *_ctx;
	asset::AssetLoader *_loader;
	asset::StringTable *_strings;

	cart::Dump   _dump;
	cart::CartDB _db;
	Thread       _workerThread;
	WorkerStatus _workerStatus;

	uint8_t             *_workerStack;
	cart::Driver        *_driver;
	cart::Parser        *_parser;
	const cart::DBEntry *_identified;

	bool _allowWatchdogClear;

	void _unloadCartData(void);
	void _setupWorker(void (App::* func)(void));
	void _setupInterrupts(void);

	void _dummyWorker(void);
	void _cartDetectWorker(void);
	void _cartUnlockWorker(void);
	void _qrCodeWorker(void);
	void _rebootWorker(void);

	void _interruptHandler(void);

public:
	inline App(void)
	: _driver(nullptr), _parser(nullptr), _identified(nullptr),
	_allowWatchdogClear(true) {
		_workerStack = new uint8_t[WORKER_STACK_SIZE];
	}
	inline ~App(void) {
		_unloadCartData();

		delete[] _workerStack;
	}

	void run(
		ui::Context &ctx, asset::AssetLoader &loader,
		asset::StringTable &strings
	);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_strings->get(id ## _h))
#define STRH(id) (APP->_strings->get(id))

#define WAPP      (reinterpret_cast<App *>(_ctx->screenData))
#define WSTR(id)  (WAPP->_strings->get(id ## _h))
#define WSTRH(id) (WAPP->_strings->get(id))
