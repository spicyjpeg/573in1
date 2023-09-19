
#pragma once

#include <stdint.h>
#include "app/cartactions.hpp"
#include "app/main.hpp"
#include "app/misc.hpp"
#include "app/cartunlock.hpp"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "file.hpp"
#include "uibase.hpp"

/* Worker status class */

enum WorkerStatusType {
	WORKER_IDLE         = 0,
	WORKER_REBOOT       = 1,
	WORKER_BUSY         = 2,
	WORKER_BUSY_SUSPEND = 3, // Prevent main thread from running
	WORKER_NEXT         = 4, // Go to next screen (goBack=false)
	WORKER_NEXT_BACK    = 5  // Go to next screen (goBack=true)
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

	void reset(void);
	void update(int part, int total, const char *text = nullptr);
	void setStatus(WorkerStatusType value);
	void setNextScreen(ui::Screen &next, bool goBack = false);
	void finish(void);
};

/* App class */

static constexpr size_t WORKER_STACK_SIZE = 0x20000;

class App {
	friend class WorkerStatusScreen;
	friend class MessageScreen;
	friend class ConfirmScreen;
	friend class WarningScreen;
	friend class ButtonMappingScreen;
	friend class MainMenuScreen;
	friend class AboutScreen;
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
	MessageScreen       _messageScreen;
	ConfirmScreen       _confirmScreen;
	WarningScreen       _warningScreen;
	ButtonMappingScreen	_buttonMappingScreen;
	MainMenuScreen      _mainMenuScreen;
	AboutScreen         _aboutScreen;
	CartInfoScreen      _cartInfoScreen;
	UnlockKeyScreen     _unlockKeyScreen;
	KeyEntryScreen      _keyEntryScreen;
	CartActionsScreen   _cartActionsScreen;
	QRCodeScreen        _qrCodeScreen;
	HexdumpScreen       _hexdumpScreen;
	ReflashGameScreen   _reflashGameScreen;
	SystemIDEntryScreen _systemIDEntryScreen;

	ui::TiledBackground _backgroundLayer;
	ui::LogOverlay      _overlayLayer;

	ui::Context       &_ctx;
	file::ZIPProvider &_resourceProvider;
	file::File        *_resourceFile;
	file::FATProvider _fileProvider;
	file::StringTable _stringTable;

	cart::Dump   _dump;
	cart::CartDB _db;
	Thread       _workerThread;
	WorkerStatus _workerStatus;
	bool         (App::*_workerFunction)(void);

	uint8_t             *_workerStack;
	cart::Driver        *_driver;
	cart::Parser        *_parser;
	const cart::DBEntry *_identified, *_selectedEntry;

	void _unloadCartData(void);
	void _setupWorker(bool (App::*func)(void));
	void _setupInterrupts(void);
	void _loadResources(void);

	bool _startupWorker(void);
	bool _cartDetectWorker(void);
	bool _cartUnlockWorker(void);
	bool _qrCodeWorker(void);
	bool _cartDumpWorker(void);
	bool _cartWriteWorker(void);
	bool _cartReflashWorker(void);
	bool _cartEraseWorker(void);
	bool _romDumpWorker(void);
	bool _atapiEjectWorker(void);
	bool _rebootWorker(void);

	void _worker(void);
	void _interruptHandler(void);

public:
	inline App(ui::Context &ctx, file::ZIPProvider &resourceProvider)
	: _overlayLayer(util::logger), _ctx(ctx),
	_resourceProvider(resourceProvider), _resourceFile(nullptr),
	_driver(nullptr), _parser(nullptr), _identified(nullptr) {
		_workerStack = new uint8_t[WORKER_STACK_SIZE];
	}

	~App(void);
	void run(void);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_stringTable.get(id ## _h))
#define STRH(id) (APP->_stringTable.get(id))

#define WAPP      (reinterpret_cast<App *>(_ctx.screenData))
#define WSTR(id)  (WAPP->_stringTable.get(id ## _h))
#define WSTRH(id) (WAPP->_stringTable.get(id))
