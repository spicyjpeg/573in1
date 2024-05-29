
#pragma once

#include <stddef.h>
#include "common/file.hpp"
#include "common/filemisc.hpp"
#include "common/filezip.hpp"
#include "common/ide.hpp"
#include "main/app/cartactions.hpp"
#include "main/app/cartunlock.hpp"
#include "main/app/main.hpp"
#include "main/app/misc.hpp"
#include "main/app/modals.hpp"
#include "main/app/romactions.hpp"
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
	WORKER_DONE         = 4  // Go to next screen
};

// This class is used by the worker thread to report its current status back to
// the main thread and the WorkerStatusScreen.
class WorkerStatus {
public:
	volatile WorkerStatusType status;

	volatile int progress, progressTotal;

	const char *volatile message;
	ui::Screen *volatile nextScreen;
	volatile bool        nextGoBack;

	void reset(ui::Screen &next, bool goBack = false);
	void update(int part, int total, const char *text = nullptr);
	ui::Screen &setNextScreen(ui::Screen &next, bool goBack = false);
	WorkerStatusType setStatus(WorkerStatusType value);
};

/* Filesystem manager class */

class FileIOManager {
private:
	file::File *_resourceFile;

public:
	file::Provider     *ide[util::countOf(ide::devices)];
	file::ZIPProvider  resource;
#ifndef NDEBUG
	file::HostProvider host;
#endif
	file::VFSProvider  vfs;

	inline ~FileIOManager(void) {
		close();
	}

	FileIOManager(void);

	void initIDE(void);
	void closeIDE(void);
	bool loadResourceFile(const char *path);
	void closeResourceFile(void);
	void close(void);
};

/* App class */

class App {
	friend class WorkerStatusScreen;
	friend class MessageScreen;
	friend class ConfirmScreen;
	friend class FilePickerScreen;
	friend class FileBrowserScreen;
	friend class AutobootScreen;
	friend class WarningScreen;
	friend class ButtonMappingScreen;
	friend class MainMenuScreen;
	friend class StorageInfoScreen;
	friend class StorageActionsScreen;
	friend class IDEInfoScreen;
	friend class RTCTimeScreen;
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
	friend class CardSizeScreen;
	friend class ChecksumScreen;

private:
	WorkerStatusScreen   _workerStatusScreen;
	MessageScreen        _messageScreen;
	ConfirmScreen        _confirmScreen;
	FilePickerScreen     _filePickerScreen;
	FileBrowserScreen    _fileBrowserScreen;
	AutobootScreen       _autobootScreen;
	WarningScreen        _warningScreen;
	ButtonMappingScreen	 _buttonMappingScreen;
	MainMenuScreen       _mainMenuScreen;
	StorageInfoScreen    _storageInfoScreen;
	StorageActionsScreen _storageActionsScreen;
	IDEInfoScreen        _ideInfoScreen;
	RTCTimeScreen        _rtcTimeScreen;
	ResolutionScreen     _resolutionScreen;
	AboutScreen          _aboutScreen;
	CartInfoScreen       _cartInfoScreen;
	UnlockKeyScreen      _unlockKeyScreen;
	KeyEntryScreen       _keyEntryScreen;
	CartActionsScreen    _cartActionsScreen;
	QRCodeScreen         _qrCodeScreen;
	HexdumpScreen        _hexdumpScreen;
	ReflashGameScreen    _reflashGameScreen;
	SystemIDEntryScreen  _systemIDEntryScreen;
	CardSizeScreen       _cardSizeScreen;
	ChecksumScreen       _checksumScreen;

#ifdef ENABLE_LOG_BUFFER
	util::LogBuffer       _logBuffer;
	ui::LogOverlay        _logOverlay;
#endif
	ui::TiledBackground   _background;
	ui::TextOverlay       _textOverlay;
	ui::ScreenshotOverlay _screenshotOverlay;

	ui::Context       &_ctx;
	file::StringTable _stringTable;
	FileIOManager     _fileIO;

	cart::CartDump      _cartDump;
	cart::ROMHeaderDump _romHeaderDump;
	cart::CartDB        _cartDB;
	cart::ROMHeaderDB   _romHeaderDB;

	Thread       _workerThread;
	util::Data   _workerStack;
	WorkerStatus _workerStatus;
	bool         (App::*_workerFunction)(void);

	cart::Driver            *_cartDriver;
	cart::CartParser        *_cartParser;
	const cart::CartDBEntry *_identified, *_selectedEntry;

	void _unloadCartData(void);
	void _setupInterrupts(void);
	void _loadResources(void);
	bool _createDataDirectory(void);
	bool _getNumberedPath(char *output, size_t length, const char *path);
	bool _takeScreenshot(void);
	void _runWorker(
		bool (App::*func)(void), ui::Screen &next, bool goBack = false,
		bool playSound = false
	);

	// cartworkers.cpp
	bool _cartDetectWorker(void);
	bool _cartUnlockWorker(void);
	bool _qrCodeWorker(void);
	bool _cartDumpWorker(void);
	bool _cartWriteWorker(void);
	bool _cartRestoreWorker(void);
	bool _cartReflashWorker(void);
	bool _cartEraseWorker(void);

	// romworkers.cpp
	bool _romChecksumWorker(void);
	bool _romDumpWorker(void);
	bool _romRestoreWorker(void);
	bool _romEraseWorker(void);
	bool _flashHeaderWriteWorker(void);

	// miscworkers.cpp
	bool _ideInitWorker(void);
	bool _executableWorker(void);
	bool _atapiEjectWorker(void);
	bool _rebootWorker(void);

	void _worker(void);
	void _interruptHandler(void);

public:
	App(ui::Context &ctx);
	~App(void);

	[[noreturn]] void run(const void *resourcePtr, size_t resourceLength);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_stringTable.get(id ## _h))
#define STRH(id) (APP->_stringTable.get(id))

#define WSTR(id)  (_stringTable.get(id ## _h))
#define WSTRH(id) (_stringTable.get(id))
