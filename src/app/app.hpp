
#pragma once

#include <stdint.h>
#include "app/actions.hpp"
#include "app/misc.hpp"
#include "app/unlock.hpp"
#include "ps1/system.h"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "file.hpp"
#include "uibase.hpp"

/* Worker status class */

enum WorkerStatusType {
	WORKER_IDLE         = 0,
	WORKER_BUSY         = 1,
	WORKER_BUSY_SUSPEND = 2, // Prevent main thread from running
	WORKER_NEXT         = 3, // Go to next screen (goBack=false)
	WORKER_NEXT_BACK    = 4  // Go to next screen (goBack=true)
};

// This class is used by the worker thread to report its current status back to
// the main thread and the WorkerStatusScreen.
class WorkerStatus {
private:
	volatile bool        _nextGoBack;
	ui::Screen *volatile _nextScreen;

public:
	volatile WorkerStatusType status;

	volatile int progress, progressTotal;

	const char *volatile message;
	ui::Screen *volatile nextScreen;

	inline void reset(void) {
		status        = WORKER_IDLE;
		progress      = 0;
		progressTotal = 1;
		message       = nullptr;
		nextScreen    = nullptr;
	}
	inline void update(int part, int total, const char *text = nullptr) {
		auto mask     = setInterruptMask(0);
		status        = WORKER_BUSY;
		progress      = part;
		progressTotal = total;

		if (text)
			message = text;
		if (mask)
			setInterruptMask(mask);
	}
	inline void suspendMainThread(void) {
		auto mask = setInterruptMask(0);
		status    = WORKER_BUSY_SUSPEND;

		if (mask)
			setInterruptMask(mask);
	}
	inline void setNextScreen(ui::Screen &next, bool goBack = false) {
		auto mask   = setInterruptMask(0);
		_nextGoBack = goBack;
		_nextScreen = &next;

		if (mask)
			setInterruptMask(mask);
	}
	inline void finish(void) {
		auto mask  = setInterruptMask(0);
		status     = _nextGoBack ? WORKER_NEXT_BACK : WORKER_NEXT;
		nextScreen = _nextScreen;

		if (mask)
			setInterruptMask(mask);
	}
};

/* App class */

static constexpr size_t WORKER_STACK_SIZE = 0x20000;

class App {
	friend class WorkerStatusScreen;
	friend class WarningScreen;
	friend class ButtonMappingScreen;
	friend class ErrorScreen;
	friend class ConfirmScreen;
	friend class CartInfoScreen;
	friend class UnlockKeyScreen;
	friend class KeyEntryScreen;
	friend class CartActionsScreen;
	friend class QRCodeScreen;
	friend class HexdumpScreen;
	friend class ReflashGameScreen;
	friend class SystemIDEntryScreen;

private:
	WorkerStatusScreen  _workerStatusScreen;
	WarningScreen       _warningScreen;
	ButtonMappingScreen	_buttonMappingScreen;
	ErrorScreen         _errorScreen;
	ConfirmScreen       _confirmScreen;
	CartInfoScreen      _cartInfoScreen;
	UnlockKeyScreen     _unlockKeyScreen;
	KeyEntryScreen      _keyEntryScreen;
	CartActionsScreen   _cartActionsScreen;
	QRCodeScreen        _qrCodeScreen;
	HexdumpScreen       _hexdumpScreen;
	ReflashGameScreen   _reflashGameScreen;
	SystemIDEntryScreen _systemIDEntryScreen;

	ui::Context       *_ctx;
	file::Provider    *_resourceProvider, *_fileProvider;
	file::StringTable *_stringTable;

	cart::Dump   _dump;
	cart::CartDB _db;
	Thread       _workerThread;
	WorkerStatus _workerStatus;
	void         (App::*_workerFunction)(void);

	uint8_t             *_workerStack;
	cart::Driver        *_driver;
	cart::Parser        *_parser;
	const cart::DBEntry *_identified, *_reflashEntry;

	void _unloadCartData(void);
	void _setupWorker(void (App::*func)(void));
	void _setupInterrupts(void);

	void _cartDetectWorker(void);
	void _cartUnlockWorker(void);
	void _qrCodeWorker(void);
	void _hddDumpWorker(void);
	void _cartWriteWorker(void);
	void _cartReflashWorker(void);
	void _cartEraseWorker(void);

	void _worker(void);
	void _interruptHandler(void);

public:
	inline App(void)
	: _driver(nullptr), _parser(nullptr), _identified(nullptr) {
		_workerStack = new uint8_t[WORKER_STACK_SIZE];
	}
	inline ~App(void) {
		_unloadCartData();

		delete[] _workerStack;
	}

	void run(
		ui::Context &ctx, file::Provider &resourceProvider,
		file::Provider &fileProvider, file::StringTable &stringTable
	);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_stringTable->get(id ## _h))
#define STRH(id) (APP->_stringTable->get(id))

#define WAPP      (reinterpret_cast<App *>(_ctx->screenData))
#define WSTR(id)  (WAPP->_stringTable->get(id ## _h))
#define WSTRH(id) (WAPP->_stringTable->get(id))
