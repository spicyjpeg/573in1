
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "app/app.hpp"
#include "ps1/system.h"
#include "cart.hpp"
#include "cartdata.hpp"
#include "cartio.hpp"
#include "defs.hpp"
#include "file.hpp"
#include "ide.hpp"
#include "io.hpp"
#include "uibase.hpp"
#include "util.hpp"
#include "utilerror.hpp"

bool App::_startupWorker(void) {
#ifdef NDEBUG
	_workerStatus.setNextScreen(_warningScreen);
#else
	// Skip the warning screen in debug builds.
	_workerStatus.setNextScreen(_buttonMappingScreen);
#endif

	for (int i = 0; i < 2; i++) {
		_workerStatus.update(i, 4, WSTR("App.startupWorker.initIDE"));
		ide::devices[i].enumerate();
	}

	_workerStatus.update(2, 4, WSTR("App.startupWorker.initFAT"));

	// Attempt to mount the secondary drive first, then in case of failure try
	// mounting the primary drive instead.
	if (!_fileProvider.init("1:"))
		_fileProvider.init("0:");

	_workerStatus.update(3, 4, WSTR("App.startupWorker.loadResources"));

	_resourceFile = _fileProvider.openFile(
		EXTERNAL_DATA_DIR "/resource.zip", file::READ
	);

	if (_resourceFile) {
		_resourceProvider.close();
		if (_resourceProvider.init(_resourceFile))
			_loadResources();
	}

	_ctx.sounds[ui::SOUND_STARTUP].play();
	return true;
}

static const char *const _CARTDB_PATHS[cart::NUM_CHIP_TYPES]{
	nullptr,
	"data/x76f041.cartdb",
	"data/x76f100.cartdb",
	"data/zs01.cartdb"
};

bool App::_cartDetectWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen);
	_workerStatus.update(0, 3, WSTR("App.cartDetectWorker.readCart"));
	_unloadCartData();

#ifdef ENABLE_DUMMY_DRIVER
	if (!cart::dummyDriverDump.chipType)
		_resourceProvider.loadStruct(cart::dummyDriverDump, "data/test.573");

	if (cart::dummyDriverDump.chipType) {
		LOG("using dummy cart driver");
		_driver = new cart::DummyDriver(_dump);
		_driver->readSystemID();
	} else {
		_driver = cart::newCartDriver(_dump);
	}
#else
	_driver = cart::newCartDriver(_dump);
#endif

	if (_dump.chipType) {
		LOG("cart dump @ 0x%08x", &_dump);
		LOG("cart driver @ 0x%08x", _driver);

		auto error = _driver->readCartID();

		if (error)
			LOG("SID error [%s]", util::getErrorString(error));

		error = _driver->readPublicData();

		if (error)
			LOG("read error [%s]", util::getErrorString(error));
		else if (!_dump.isReadableDataEmpty())
			_parser = cart::newCartParser(_dump);

		LOG("cart parser @ 0x%08x", _parser);
		_workerStatus.update(1, 3, WSTR("App.cartDetectWorker.identifyGame"));

		if (!_db.ptr) {
			if (!_resourceProvider.loadData(
				_db, _CARTDB_PATHS[_dump.chipType])
			) {
				LOG("%s not found", _CARTDB_PATHS[_dump.chipType]);
				goto _cartInitDone;
			}
		}

		char code[8], region[8];

		if (!_parser)
			goto _cartInitDone;
		if (_parser->getCode(code) && _parser->getRegion(region))
			_identified = _db.lookup(code, region);
		if (!_identified)
			goto _cartInitDone;

		// Force the parser to use correct format for the game (to prevent
		// ambiguity between different formats).
		delete _parser;
		_parser = cart::newCartParser(
			_dump, _identified->formatType, _identified->flags
		);

		LOG("new cart parser @ 0x%08x", _parser);
	}

_cartInitDone:
	_workerStatus.update(2, 3, WSTR("App.cartDetectWorker.readDigitalIO"));

#ifdef ENABLE_DUMMY_DRIVER
	if (io::isDigitalIOPresent() && !(_dump.flags & cart::DUMP_SYSTEM_ID_OK)) {
#else
	if (io::isDigitalIOPresent()) {
#endif
		util::Data bitstream;
		bool       ready;

		if (!_resourceProvider.loadData(bitstream, "data/fpga.bit")) {
			LOG("bitstream unavailable");
			return true;
		}

		ready = io::loadBitstream(bitstream.as<uint8_t>(), bitstream.length);
		bitstream.destroy();

		if (!ready) {
			LOG("bitstream upload failed");
			return true;
		}

		delayMicroseconds(5000); // Probably not necessary
		io::initKonamiBitstream();

		auto error = _driver->readSystemID();

		if (error)
			LOG("XID error [%s]", util::getErrorString(error));
	}

	return true;
}

bool App::_cartUnlockWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen, true);
	_workerStatus.update(0, 2, WSTR("App.cartUnlockWorker.read"));

	auto error = _driver->readPrivateData();

	if (error) {
		LOG("read error [%s]", util::getErrorString(error));

		/*_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartUnlockWorker.error")
		);*/
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	if (_parser)
		delete _parser;

	_parser = cart::newCartParser(_dump);

	if (!_parser)
		return true;

	LOG("cart parser @ 0x%08x", _parser);
	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (_parser->getCode(code) && _parser->getRegion(region))
		_identified = _db.lookup(code, region);

	// If auto-identification failed (e.g. because the format has no game code),
	// use the game whose unlocking key was selected as a hint.
	if (!_identified) {
		if (_selectedEntry) {
			LOG("identify failed, using key as hint");
			_identified = _selectedEntry;
		} else {
			return true;
		}
	}

	delete _parser;
	_parser = cart::newCartParser(
		_dump, _identified->formatType, _identified->flags
	);

	LOG("new cart parser @ 0x%08x", _parser);
	return true;
}

bool App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.setNextScreen(_qrCodeScreen);
	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_dump.toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);

	return true;
}

bool App::_cartDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartDumpWorker.save"));

	char   code[8], region[8], path[32];
	size_t length = _dump.getDumpLength();

	if (!_fileProvider.fileExists(EXTERNAL_DATA_DIR)) {
		if (!_fileProvider.createDirectory(EXTERNAL_DATA_DIR))
			goto _error;
	}

	if (_identified && _parser->getCode(code) && _parser->getRegion(region))
		snprintf(path, sizeof(path), EXTERNAL_DATA_DIR "/%s%s.573", code, region);
	else
		__builtin_strcpy(path, EXTERNAL_DATA_DIR "/unknown.573");

	LOG("saving %s, length=%d", path, length);

	if (_fileProvider.saveData(&_dump, length, path) != length)
		goto _error;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _cartInfoScreen, WSTR("App.cartDumpWorker.success"),
		path
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_error:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartDumpWorker.error")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_cartWriteWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartWriteWorker.write"));

	uint8_t key[8];
	auto    error = _driver->writeData();

	if (!error)
		_identified->copyKeyTo(key);

	_cartDetectWorker();

	if (error) {
		LOG("write error [%s]", util::getErrorString(error));

		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartWriteWorker.error")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	_dump.copyKeyFrom(key);
	return _cartUnlockWorker();
}

bool App::_cartReflashWorker(void) {
	// Make sure a valid cart ID is present if required by the new data.
	if (
		_selectedEntry->requiresCartID() &&
		!(_dump.flags & cart::DUMP_CART_ID_OK)
	) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTR("App.cartReflashWorker.idError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	// TODO: preserve 0x81 traceid if possible
	//uint8_t traceID[8];
	//_parser->getIdentifiers()->traceID.copyTo(traceID);

	if (!_cartEraseWorker())
		return false;
	if (_parser)
		delete _parser;

	_parser  = cart::newCartParser(
		_dump, _selectedEntry->formatType, _selectedEntry->flags
	);
	auto pri = _parser->getIdentifiers();
	auto pub = _parser->getPublicIdentifiers();

	_dump.clearData();
	_dump.initConfig(9, _selectedEntry->flags & cart::DATA_HAS_PUBLIC_SECTION);

	if (pri) {
		if (_selectedEntry->flags & cart::DATA_HAS_CART_ID)
			pri->cartID.copyFrom(_dump.cartID.data);
		if (_selectedEntry->flags & cart::DATA_HAS_TRACE_ID)
			pri->updateTraceID(
				_selectedEntry->traceIDType, _selectedEntry->traceIDParam,
				&_dump.cartID
			);
		if (_selectedEntry->flags & cart::DATA_HAS_INSTALL_ID) {
			// The private installation ID seems to be unused on carts with a
			// public data section.
			if (pub)
				pub->setInstallID(_selectedEntry->installIDPrefix);
			else
				pri->setInstallID(_selectedEntry->installIDPrefix);
		}
	}

	_parser->setCode(_selectedEntry->code);
	_parser->setRegion(_selectedEntry->region);
	_parser->setYear(_selectedEntry->year);
	_parser->flush();

	_workerStatus.update(1, 3, WSTR("App.cartReflashWorker.setDataKey"));
	auto error = _driver->setDataKey(_selectedEntry->dataKey);

	if (error) {
		LOG("key error [%s]", util::getErrorString(error));
	} else {
		_workerStatus.update(2, 3, WSTR("App.cartReflashWorker.write"));
		error = _driver->writeData();
	}

	_cartDetectWorker();

	if (error) {
		LOG("write error [%s]", util::getErrorString(error));

		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTR("App.cartReflashWorker.writeError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	return _cartUnlockWorker();
}

bool App::_cartEraseWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartEraseWorker.erase"));

	auto error = _driver->erase();
	_cartDetectWorker();

	if (error) {
		LOG("erase error [%s]", util::getErrorString(error));

		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartEraseWorker.error")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	return _cartUnlockWorker();
}

struct DumpRegion {
public:
	util::Hash     prompt;
	const char     *path;
	const uint16_t *ptr;
	size_t         length;
	int            bank;
	uint32_t       inputs;
};

enum DumpBank {
	BANK_NONE_8BIT  = -1,
	BANK_NONE_16BIT = -2
};

static constexpr int _NUM_DUMP_REGIONS = 5;

static const DumpRegion _DUMP_REGIONS[_NUM_DUMP_REGIONS]{
	{
		.prompt = "App.romDumpWorker.dumpBIOS"_h,
		.path   = EXTERNAL_DATA_DIR "/dump%d/bios.bin",
		.ptr    = reinterpret_cast<const uint16_t *>(DEV2_BASE),
		.length = 0x80000,
		.bank   = BANK_NONE_16BIT,
		.inputs = 0
	}, {
		.prompt = "App.romDumpWorker.dumpRTC"_h,
		.path   = EXTERNAL_DATA_DIR "/dump%d/rtc.bin",
		.ptr    = reinterpret_cast<const uint16_t *>(DEV0_BASE | 0x620000),
		.length = 0x2000,
		.bank   = BANK_NONE_8BIT,
		.inputs = 0
	}, {
		.prompt = "App.romDumpWorker.dumpFlash"_h,
		.path   = EXTERNAL_DATA_DIR "/dump%d/flash.bin",
		.ptr    = reinterpret_cast<const uint16_t *>(DEV0_BASE),
		.length = 0x1000000,
		.bank   = SYS573_BANK_FLASH,
		.inputs = 0
	}, {
		.prompt = "App.romDumpWorker.dumpPCMCIA1"_h,
		.path   = EXTERNAL_DATA_DIR "/dump%d/pcmcia1.bin",
		.ptr    = reinterpret_cast<const uint16_t *>(DEV0_BASE),
		.length = 0x4000000,
		.bank   = SYS573_BANK_PCMCIA1,
		.inputs = io::JAMMA_PCMCIA_CD1
	}, {
		.prompt = "App.romDumpWorker.dumpPCMCIA2"_h,
		.path   = EXTERNAL_DATA_DIR "/dump%d/pcmcia2.bin",
		.ptr    = reinterpret_cast<const uint16_t *>(DEV0_BASE),
		.length = 0x4000000,
		.bank   = SYS573_BANK_PCMCIA2,
		.inputs = io::JAMMA_PCMCIA_CD2
	}
};

bool App::_romDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romDumpWorker.init"));

	uint32_t inputs = io::getJAMMAInputs();

	// Store all dumps in a subdirectory named "dumpN" within the main data
	// folder.
	int  index = 0;
	char dirPath[32], filePath[32];

	if (!_fileProvider.fileExists(EXTERNAL_DATA_DIR)) {
		if (!_fileProvider.createDirectory(EXTERNAL_DATA_DIR))
			goto _initError;
	}

	do {
		index++;
		snprintf(dirPath, sizeof(dirPath), EXTERNAL_DATA_DIR "/dump%d", index);
	} while (_fileProvider.fileExists(dirPath));

	LOG("saving dumps to %s", dirPath);

	if (!_fileProvider.createDirectory(dirPath))
		goto _initError;

	for (int i = 0; i < _NUM_DUMP_REGIONS; i++) {
		auto &region = _DUMP_REGIONS[i];

		// Skip PCMCIA slots if a card is not inserted.
		if (region.inputs && !(inputs & region.inputs))
			continue;

		snprintf(filePath, sizeof(filePath), region.path, index);

		auto _file = _fileProvider.openFile(
			filePath, file::WRITE | file::ALLOW_CREATE
		);

		if (!_file)
			goto _writeError;

		// The buffer has to be 8 KB to match the size of RTC RAM.
		uint8_t buffer[0x2000];

		const uint16_t *ptr  = region.ptr;
		int            count = region.length / sizeof(buffer);
		int            bank  = region.bank;

		if (bank >= 0)
			io::setFlashBank(bank++);

		for (int j = 0; j < count; j++) {
			_workerStatus.update(j, count, WSTRH(region.prompt));

			// The RTC is an 8-bit device connected to a 16-bit bus, i.e. each
			// byte must be read as a 16-bit value and then the upper 8 bits
			// must be discarded.
			if (bank == BANK_NONE_8BIT) {
				uint8_t *output = buffer;

				for (size_t k = sizeof(buffer); k; k--)
					*(output++) = static_cast<uint8_t>(*(ptr++));
			} else {
				uint16_t *output = reinterpret_cast<uint16_t *>(buffer);

				for (size_t k = sizeof(buffer); k; k -= 2)
					*(output++) = *(ptr++);
			}

			if (
				(bank >= 0) &&
				(ptr  >= reinterpret_cast<const void *>(DEV0_BASE | 0x400000))
			) {
				ptr = region.ptr;
				io::setFlashBank(bank++);
			}

			if (_file->write(buffer, sizeof(buffer)) < sizeof(buffer)) {
				_file->close();
				goto _writeError;
			}
		}

		_file->close();
		LOG("%s saved", filePath);
	}

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _mainMenuScreen, WSTR("App.romDumpWorker.success"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_initError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _mainMenuScreen, WSTR("App.romDumpWorker.initError")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_writeError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _mainMenuScreen, WSTR("App.romDumpWorker.dumpError")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_atapiEjectWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.atapiEjectWorker.eject"));

	if (!(ide::devices[0].flags & ide::DEVICE_ATAPI)) {
		LOG("primary drive is not ATAPI");

		_messageScreen.setMessage(
			MESSAGE_ERROR, _mainMenuScreen,
			WSTR("App.atapiEjectWorker.atapiError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	ide::Packet packet;
	packet.setStartStopUnit(ide::START_STOP_MODE_OPEN_TRAY);

	auto error = ide::devices[0].atapiPacket(packet);

	if (error) {
		LOG("eject error [%s]", util::getErrorString(error));

		_messageScreen.setMessage(
			MESSAGE_ERROR, _mainMenuScreen,
			WSTR("App.atapiEjectWorker.ejectError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _mainMenuScreen, WSTR("App.atapiEjectWorker.success")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;
}

bool App::_rebootWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.rebootWorker.reboot"));
	_workerStatus.setStatus(WORKER_REBOOT);

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	softReset();

	return true;
}
