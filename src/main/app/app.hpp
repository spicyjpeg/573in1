/*
 * 573in1 - Copyright (C) 2022-2024 spicyjpeg
 *
 * 573in1 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * 573in1. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>
#include "common/fs/file.hpp"
#include "common/fs/misc.hpp"
#include "common/fs/zip.hpp"
#include "common/storage/device.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "main/app/cartactions.hpp"
#include "main/app/cartunlock.hpp"
#include "main/app/mainmenu.hpp"
#include "main/app/misc.hpp"
#include "main/app/modals.hpp"
#include "main/app/romactions.hpp"
#include "main/app/tests.hpp"
#include "main/app/threads.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartdata.hpp"
#include "main/cart/cartio.hpp"
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
	WorkerStatusType status;
	int              progress, progressTotal;

	const char *message;
	ui::Screen *nextScreen;
	bool       nextGoBack;

	void reset(ui::Screen &next, bool goBack = false);
	void update(int part, int total, const char *text = nullptr);
	ui::Screen &setNextScreen(ui::Screen &next, bool goBack = false);
	WorkerStatusType setStatus(WorkerStatusType value);
};

/* Filesystem manager class */

extern const char *const IDE_MOUNT_POINTS[];

class FileIOManager {
private:
	fs::File *_resourceFile;

public:
	const void *resourcePtr;
	size_t     resourceLength;

	fs::ZIPProvider  resource;
#ifdef ENABLE_PCDRV
	fs::HostProvider host;
#endif
	fs::VFSProvider  vfs;

	storage::Device *ideDevices[2];
	fs::Provider    *ideProviders[2];

	inline ~FileIOManager(void) {
		closeResourceFile();
		unmountIDE();
	}

	FileIOManager(void);

	void initIDE(void);
	void mountIDE(void);
	void unmountIDE(void);
	bool loadResourceFile(const char *path);
	void closeResourceFile(void);
};

/* App class */

class App {
private:
	friend void _appInterruptHandler(void *arg0, void *arg1);
	friend void _workerMain(void *arg0, void *arg1);

	// cartworkers.cpp
	friend bool cartDetectWorker(App &app);
	friend bool cartUnlockWorker(App &app);
	friend bool qrCodeWorker(App &app);
	friend bool cartDumpWorker(App &app);
	friend bool cartWriteWorker(App &app);
	friend bool cartRestoreWorker(App &app);
	friend bool cartReflashWorker(App &app);
	friend bool cartEraseWorker(App &app);

	// romworkers.cpp
	friend bool romChecksumWorker(App &app);
	friend bool romDumpWorker(App &app);
	friend bool romRestoreWorker(App &app);
	friend bool romEraseWorker(App &app);
	friend bool flashExecutableWriteWorker(App &app);
	friend bool flashHeaderWriteWorker(App &app);

	// miscworkers.cpp
	friend bool startupWorker(App &app);
	friend bool fileInitWorker(App &app);
	friend bool executableWorker(App &app);
	friend bool atapiEjectWorker(App &app);
	friend bool rebootWorker(App &app);

	// modals.cpp
	friend class WorkerStatusScreen;
	friend class MessageScreen;
	friend class ConfirmScreen;
	friend class FilePickerScreen;
	friend class FileBrowserScreen;

	WorkerStatusScreen _workerStatusScreen;
	MessageScreen      _messageScreen;
	ConfirmScreen      _confirmScreen;
	FilePickerScreen   _filePickerScreen;
	FileBrowserScreen  _fileBrowserScreen;

	// main.cpp
	friend class WarningScreen;
	friend class AutobootScreen;
	friend class ButtonMappingScreen;
	friend class MainMenuScreen;

	WarningScreen       _warningScreen;
	AutobootScreen      _autobootScreen;
	ButtonMappingScreen	_buttonMappingScreen;
	MainMenuScreen      _mainMenuScreen;

	// cartunlock.cpp
	friend class CartInfoScreen;
	friend class UnlockKeyScreen;
	friend class KeyEntryScreen;

	CartInfoScreen  _cartInfoScreen;
	UnlockKeyScreen _unlockKeyScreen;
	KeyEntryScreen  _keyEntryScreen;

	// cartactions.cpp
	friend class CartActionsScreen;
	friend class QRCodeScreen;
	friend class HexdumpScreen;
	friend class ReflashGameScreen;
	friend class SystemIDEntryScreen;

	CartActionsScreen   _cartActionsScreen;
	QRCodeScreen        _qrCodeScreen;
	HexdumpScreen       _hexdumpScreen;
	ReflashGameScreen   _reflashGameScreen;
	SystemIDEntryScreen _systemIDEntryScreen;

	// romactions.cpp
	friend class StorageInfoScreen;
	friend class StorageActionsScreen;
	friend class CardSizeScreen;
	friend class ChecksumScreen;

	StorageInfoScreen    _storageInfoScreen;
	StorageActionsScreen _storageActionsScreen;
	CardSizeScreen       _cardSizeScreen;
	ChecksumScreen       _checksumScreen;

	// tests.cpp
	friend class TestMenuScreen;
	friend class JAMMATestScreen;
	friend class AudioTestScreen;
	friend class TestPatternScreen;
	friend class ColorIntensityScreen;
	friend class GeometryScreen;

	TestMenuScreen       _testMenuScreen;
	JAMMATestScreen      _jammaTestScreen;
	AudioTestScreen      _audioTestScreen;
	ColorIntensityScreen _colorIntensityScreen;
	GeometryScreen       _geometryScreen;

	// misc.cpp
	friend class IDEInfoScreen;
	friend class RTCTimeScreen;
	friend class LanguageScreen;
	friend class ResolutionScreen;
	friend class AboutScreen;

	IDEInfoScreen    _ideInfoScreen;
	RTCTimeScreen    _rtcTimeScreen;
	LanguageScreen   _languageScreen;
	ResolutionScreen _resolutionScreen;
	AboutScreen      _aboutScreen;

	ui::TiledBackground   _background;
	ui::TextOverlay       _textOverlay;
	ui::SplashOverlay     _splashOverlay;
#ifdef ENABLE_LOG_BUFFER
	util::LogBuffer       _logBuffer;
	ui::LogOverlay        _logOverlay;
#endif
	ui::ScreenshotOverlay _screenshotOverlay;

	ui::Context        &_ctx;
	fs::StringTable    _stringTable;
	FileIOManager      _fileIO;
	AudioStreamManager _audioStream;

	Thread       _workerThread;
	util::Data   _workerStack;
	WorkerStatus _workerStatus;

	cart::CartDump      _cartDump;
	cart::ROMHeaderDump _romHeaderDump;
	cart::CartDB        _cartDB;
	cart::ROMHeaderDB   _romHeaderDB;

	cart::Driver            *_cartDriver;
	cart::CartParser        *_cartParser;
	const cart::CartDBEntry *_identified, *_selectedEntry;

	void _unloadCartData(void);
	void _updateOverlays(void);

	void _loadResources(void);
	bool _createDataDirectory(void);
	bool _getNumberedPath(
		char *output, size_t length, const char *path, int maxIndex = 9999
	);
	bool _takeScreenshot(void);

	void _setupInterrupts(void);
	void _runWorker(
		bool (*func)(App &app), ui::Screen &next, bool goBack = false,
		bool playSound = false
	);

public:
	App(ui::Context &ctx);
	~App(void);

	[[noreturn]] void run(const void *resourcePtr, size_t resourceLength);
};

#define APP      (reinterpret_cast<App *>(ctx.screenData))
#define STR(id)  (APP->_stringTable.get(id ## _h))
#define STRH(id) (APP->_stringTable.get(id))

#define WSTR(id)  (app._stringTable.get(id ## _h))
#define WSTRH(id) (app._stringTable.get(id))
