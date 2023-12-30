
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/cart.hpp"
#include "main/cartdata.hpp"
#include "main/cartio.hpp"
#include "main/uibase.hpp"
#include "ps1/system.h"

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
			LOG("SID error [%s]", cart::getErrorString(error));

		error = _driver->readPublicData();

		if (error)
			LOG("read error [%s]", cart::getErrorString(error));
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
			LOG("XID error [%s]", cart::getErrorString(error));
	}

	return true;
}

static const util::Hash _UNLOCK_ERRORS[cart::NUM_CHIP_TYPES]{
	0,
	"App.cartUnlockWorker.x76f041Error"_h,
	"App.cartUnlockWorker.x76f100Error"_h,
	"App.cartUnlockWorker.zs01Error"_h
};

bool App::_cartUnlockWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen, true);
	_workerStatus.update(0, 2, WSTR("App.cartUnlockWorker.read"));

	auto error = _driver->readPrivateData();

	if (error) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTRH(_UNLOCK_ERRORS[_dump.chipType]), cart::getErrorString(error)
		);

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

	file::FileInfo info;

	if (!_fileProvider.getFileInfo(info, EXTERNAL_DATA_DIR)) {
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
		MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartDumpWorker.error"), path
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
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartWriteWorker.error"),
			cart::getErrorString(error)
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
		LOG("key error [%s]", cart::getErrorString(error));
	} else {
		_workerStatus.update(2, 3, WSTR("App.cartReflashWorker.write"));
		error = _driver->writeData();
	}

	_cartDetectWorker();

	if (error) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTR("App.cartReflashWorker.writeError"),
			cart::getErrorString(error)
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
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen, WSTR("App.cartEraseWorker.error"),
			cart::getErrorString(error)
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	return _cartUnlockWorker();
}

struct DumpEntry {
public:
	util::Hash        dumpPrompt, hashPrompt;
	const char        *path;
	const rom::Region &region;
	size_t            hashOffset;
};

static const DumpEntry _DUMP_ENTRIES[]{
	{
		.dumpPrompt = "App.romDumpWorker.dumpBIOS"_h,
		.hashPrompt = "App.systemInfoWorker.hashBIOS"_h,
		.path       = "%s/bios.bin",
		.region     = rom::bios,
		.hashOffset = offsetof(SystemInfo, biosCRC)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpRTC"_h,
		.hashPrompt = "App.systemInfoWorker.hashRTC"_h,
		.path       = "%s/rtc.bin",
		.region     = rom::rtc,
		.hashOffset = offsetof(SystemInfo, rtcCRC)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpFlash"_h,
		.hashPrompt = "App.systemInfoWorker.hashFlash"_h,
		.path       = "%s/flash.bin",
		.region     = rom::flash,
		.hashOffset = offsetof(SystemInfo, flash.crc)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpPCMCIA1"_h,
		.hashPrompt = "App.systemInfoWorker.hashPCMCIA1"_h,
		.path       = "%s/pcmcia1.bin",
		.region     = rom::pcmcia[0],
		.hashOffset = offsetof(SystemInfo, pcmcia[0].crc)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpPCMCIA2"_h,
		.hashPrompt = "App.systemInfoWorker.hashPCMCIA2"_h,
		.path       = "%s/pcmcia2.bin",
		.region     = rom::pcmcia[1],
		.hashOffset = offsetof(SystemInfo, pcmcia[1].crc)
	}
};

static constexpr size_t _DUMP_CHUNK_LENGTH   = 0x80000;
static constexpr size_t _DUMP_CHUNKS_PER_CRC = 32; // Save CRC32 every 16 MB

bool App::_romDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romDumpWorker.init"));

	// Store all dumps in a subdirectory named "dumpN" within the main data
	// folder.
	int  index = 0;
	char dirPath[32], filePath[32];

	file::FileInfo info;

	if (!_fileProvider.getFileInfo(info, EXTERNAL_DATA_DIR)) {
		if (!_fileProvider.createDirectory(EXTERNAL_DATA_DIR))
			goto _initError;
	}

	do {
		index++;
		snprintf(dirPath, sizeof(dirPath), EXTERNAL_DATA_DIR "/dump%d", index);
	} while (_fileProvider.getFileInfo(info, dirPath));

	LOG("saving dumps to %s", dirPath);

	if (!_fileProvider.createDirectory(dirPath))
		goto _initError;

	for (auto &entry : _DUMP_ENTRIES) {
		if (!entry.region.isPresent())
			continue;

		size_t chunkLength, numChunks;

		if (entry.region.regionLength < _DUMP_CHUNK_LENGTH) {
			chunkLength = entry.region.regionLength;
			numChunks   = 1;
		} else {
			chunkLength = _DUMP_CHUNK_LENGTH;
			numChunks   = entry.region.regionLength / _DUMP_CHUNK_LENGTH;
		}

		snprintf(filePath, sizeof(filePath), entry.path, dirPath);

		auto _file = _fileProvider.openFile(
			filePath, file::WRITE | file::ALLOW_CREATE
		);

		if (!_file)
			goto _writeError;

		auto     buffer = new uint8_t[chunkLength];
		uint32_t offset = 0;

		//assert(buffer);

		for (size_t i = 0; i < numChunks; i++) {
			_workerStatus.update(i, numChunks, WSTRH(entry.dumpPrompt));
			entry.region.read(buffer, offset, chunkLength);

			if (_file->write(buffer, chunkLength) < chunkLength) {
				_file->close();
				delete   _file;
				delete[] buffer;

				goto _writeError;
			}

			offset += chunkLength;
		}

		_file->close();
		delete   _file;
		delete[] buffer;

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
		MESSAGE_ERROR, _mainMenuScreen, WSTR("App.romDumpWorker.initError"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_writeError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _mainMenuScreen, WSTR("App.romDumpWorker.dumpError"),
		filePath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_systemInfoWorker(void) {
	// This is necessary to ensure the digital I/O ID is read at least once.
	if (!_driver)
		_cartDetectWorker();

	_workerStatus.setNextScreen(_systemInfoScreen);
	_systemInfo.clearFlags();

	for (auto &entry : _DUMP_ENTRIES) {
		if (!entry.region.isPresent())
			continue;

		size_t chunkLength, numChunks;

		if (entry.region.regionLength < _DUMP_CHUNK_LENGTH) {
			chunkLength = entry.region.regionLength;
			numChunks   = 1;
		} else {
			chunkLength = _DUMP_CHUNK_LENGTH;
			numChunks   = entry.region.regionLength / _DUMP_CHUNK_LENGTH;
		}

		uint32_t offset = 0;
		uint32_t crc    = 0;
		auto     crcPtr = reinterpret_cast<uint32_t *>(
			reinterpret_cast<uintptr_t>(&_systemInfo) + entry.hashOffset
		);

		// Flash cards can be 16, 32 or 64 MB, so copies of the current CRC are
		// saved after the first 16, then 32, 48 and finally 64 MB are read.
		for (size_t i = 0; i < numChunks; i += _DUMP_CHUNKS_PER_CRC) {
			size_t end = util::min(i + _DUMP_CHUNKS_PER_CRC, numChunks);

			for (size_t j = i; j < end; j++) {
				_workerStatus.update(j, numChunks, WSTRH(entry.hashPrompt));

				crc     = entry.region.zipCRC32(offset, chunkLength, crc);
				offset += chunkLength;
			}

			*(crcPtr++) = crc;
		}
	}

	_systemInfo.flags = SYSTEM_INFO_VALID;
	_systemInfo.shell = rom::getShellInfo();

	if (io::isRTCBatteryLow())
		_systemInfo.flags |= SYSTEM_INFO_RTC_BATTERY_LOW;

	_systemInfo.flash.jedecID = rom::flash.getJEDECID();
	_systemInfo.flash.flags   = FLASH_REGION_INFO_PRESENT;

	if (rom::flash.hasBootExecutable())
		_systemInfo.flash.flags |= FLASH_REGION_INFO_BOOTABLE;

	for (int i = 0; i < 2; i++) {
		auto &region = rom::pcmcia[i];
		auto &card   = _systemInfo.pcmcia[i];

		if (region.isPresent()) {
			card.jedecID = region.getJEDECID();
			card.flags   = FLASH_REGION_INFO_PRESENT;

			if (region.hasBootExecutable())
				card.flags |= FLASH_REGION_INFO_BOOTABLE;
		}
	}

	return true;
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
		_messageScreen.setMessage(
			MESSAGE_ERROR, _mainMenuScreen,
			WSTR("App.atapiEjectWorker.ejectError"), ide::getErrorString(error)
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
