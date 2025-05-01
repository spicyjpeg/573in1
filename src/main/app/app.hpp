/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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
#include <stdint.h>
#include "common/util/containers.hpp"
#include "common/util/log.hpp"
#include "main/app/cartactions.hpp"
#include "main/app/cartunlock.hpp"
#include "main/app/fileio.hpp"
#include "main/app/mainmenu.hpp"
#include "main/app/misc.hpp"
#include "main/app/modals.hpp"
#include "main/app/nvramactions.hpp"
#include "main/app/tests.hpp"
#include "main/app/threads.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartdata.hpp"
#include "main/formats.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

/* App class */

enum WorkerFlag : uint32_t {
	WORKER_BUSY         = 1 << 0,
	WORKER_REBOOT       = 1 << 1,
	WORKER_SUSPEND_MAIN = 1 << 2
};

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

	// nvramworkers.cpp
	friend bool nvramChecksumWorker(App &app);
	friend bool nvramDumpWorker(App &app);
	friend bool nvramRestoreWorker(App &app);
	friend bool nvramEraseWorker(App &app);
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

	// nvramactions.cpp
	friend class NVRAMInfoScreen;
	friend class NVRAMActionsScreen;
	friend class CardSizeScreen;
	friend class ChecksumScreen;

	NVRAMInfoScreen    _nvramInfoScreen;
	NVRAMActionsScreen _nvramActionsScreen;
	CardSizeScreen     _cardSizeScreen;
	ChecksumScreen     _checksumScreen;

	// tests.cpp
	friend class TestMenuScreen;
	friend class JAMMATestScreen;
	friend class AnalogTestScreen;
	friend class AudioTestScreen;
	friend class TestPatternScreen;
	friend class ColorIntensityScreen;
	friend class GeometryScreen;

	TestMenuScreen       _testMenuScreen;
	JAMMATestScreen      _jammaTestScreen;
	AnalogTestScreen     _analogTestScreen;
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
	FileIOManager      _fileIO;
	AudioStreamManager _audioStream;

	formats::GameDB      _gameDB;
	formats::StringTable _stringTable;

	Thread     _workerThread;
	util::Data _workerStack;
	uint32_t   _workerFlags;

	cart::CartDump      _cartDump;
	cart::ROMHeaderDump _romHeaderDump;
	cart::CartDB        _cartDB;
	cart::ROMHeaderDB   _romHeaderDB;

	cart::CartParser        *_cartParser;
	const cart::CartDBEntry *_identified, *_selectedEntry;

	void _unloadCartData(void);
	void _updateOverlays(void);

	void _loadResources(void);
	bool _createDataDirectory(void);
	bool _takeScreenshot(void);

	void _setupInterrupts(void);
	void _runWorker(bool (*func)(App &app), bool playSound = false);

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
