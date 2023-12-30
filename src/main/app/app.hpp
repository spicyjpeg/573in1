
#pragma once

#include <stdint.h>
#include "common/file.hpp"
#include "common/rom.hpp"
#include "main/app/cartactions.hpp"
#include "main/app/cartunlock.hpp"
#include "main/app/main.hpp"
#include "main/app/misc.hpp"
#include "main/cart.hpp"
#include "main/cartdata.hpp"
#include "main/cartio.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

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

/* System information buffer */

enum FlashRegionInfoFlag : uint16_t {
	FLASH_REGION_INFO_PRESENT  = 1 << 0,
	FLASH_REGION_INFO_BOOTABLE = 1 << 1
};

class FlashRegionInfo {
public:
	uint16_t flags, jedecID;
	uint32_t crc[4];

	inline void clearFlags(void) {
		flags = 0;
	}
};

enum SystemInfoFlag : uint32_t {
	SYSTEM_INFO_VALID           = 1 << 0,
	SYSTEM_INFO_RTC_BATTERY_LOW = 1 << 1
};

class SystemInfo {
public:
	uint32_t flags;
	uint32_t biosCRC, rtcCRC;

	const rom::ShellInfo *shell;
	FlashRegionInfo      flash, pcmcia[2];

	inline SystemInfo(void) {
		clearFlags();
	}

	inline void clearFlags(void) {
		flags       = 0;
		flash.flags = 0;
		pcmcia[0].clearFlags();
		pcmcia[1].clearFlags();
	}
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
	friend class SystemInfoScreen;
	friend class ResolutionScreen;
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
	SystemInfoScreen    _systemInfoScreen;
	ResolutionScreen    _resolutionScreen;
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
	SystemInfo   _systemInfo;
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
	bool _systemInfoWorker(void);
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
	[[noreturn]] void run(void);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_stringTable.get(id ## _h))
#define STRH(id) (APP->_stringTable.get(id))

#define WAPP      (reinterpret_cast<App *>(_ctx.screenData))
#define WSTR(id)  (WAPP->_stringTable.get(id ## _h))
#define WSTRH(id) (WAPP->_stringTable.get(id))
