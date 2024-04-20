
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/io.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/cart.hpp"
#include "main/cartdata.hpp"
#include "main/cartio.hpp"

static const char *const _CARTDB_PATHS[cart::NUM_CHIP_TYPES]{
	nullptr,
	"data/x76f041.db",
	"data/x76f100.db",
	"data/zs01.db"
};

bool App::_cartDetectWorker(void) {
	_workerStatus.setNextScreen(_cartInfoScreen);
	_workerStatus.update(0, 3, WSTR("App.cartDetectWorker.readCart"));

	_unloadCartData();
	_qrCodeScreen.valid = false;

#ifdef ENABLE_DUMMY_DRIVER
	if (!cart::dummyDriverDump.chipType)
		_resourceProvider.loadStruct(cart::dummyDriverDump, "data/test.573");

	if (cart::dummyDriverDump.chipType) {
		LOG("using dummy cart driver");
		_cartDriver = new cart::DummyDriver(_cartDump);
		_cartDriver->readSystemID();
	} else {
		_cartDriver = cart::newCartDriver(_cartDump);
	}
#else
	_cartDriver = cart::newCartDriver(_cartDump);
#endif

	if (_cartDump.chipType) {
		LOG("cart dump @ 0x%08x", &_cartDump);
		LOG("cart driver @ 0x%08x", _cartDriver);

		auto error = _cartDriver->readCartID();

		if (error)
			LOG("SID error [%s]", cart::getErrorString(error));

		error = _cartDriver->readPublicData();

		if (error)
			LOG("read error [%s]", cart::getErrorString(error));
		else if (!_cartDump.isReadableDataEmpty())
			_cartParser = cart::newCartParser(_cartDump);

		LOG("cart parser @ 0x%08x", _cartParser);
		_workerStatus.update(1, 3, WSTR("App.cartDetectWorker.identifyGame"));

		if (!_cartDB.ptr) {
			if (!_resourceProvider.loadData(
				_cartDB, _CARTDB_PATHS[_cartDump.chipType])
			) {
				LOG("%s not found", _CARTDB_PATHS[_cartDump.chipType]);
				goto _cartInitDone;
			}
		}

		char code[8], region[8];

		if (!_cartParser)
			goto _cartInitDone;
		if (_cartParser->getCode(code) && _cartParser->getRegion(region))
			_identified = _cartDB.lookup(code, region);
		if (!_identified)
			goto _cartInitDone;

		// Force the parser to use correct format for the game (to prevent
		// ambiguity between different formats).
		delete _cartParser;
		_cartParser = cart::newCartParser(
			_cartDump, _identified->formatType, _identified->flags
		);

		LOG("new cart parser @ 0x%08x", _cartParser);
	}

_cartInitDone:
	_workerStatus.update(2, 3, WSTR("App.cartDetectWorker.readDigitalIO"));

	if (
#ifdef ENABLE_DUMMY_DRIVER
		!(_cartDump.flags & cart::DUMP_SYSTEM_ID_OK) &&
#endif
		io::isDigitalIOPresent()
	) {
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

		auto error = _cartDriver->readSystemID();

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

	_qrCodeScreen.valid = false;

	auto error = _cartDriver->readPrivateData();

	if (error) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTRH(_UNLOCK_ERRORS[_cartDump.chipType]),
			cart::getErrorString(error)
		);

		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	if (_cartParser)
		delete _cartParser;

	_cartParser = cart::newCartParser(_cartDump);

	if (!_cartParser)
		return true;

	LOG("cart parser @ 0x%08x", _cartParser);
	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (_cartParser->getCode(code) && _cartParser->getRegion(region))
		_identified = _cartDB.lookup(code, region);

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

	delete _cartParser;
	_cartParser = cart::newCartParser(
		_cartDump, _identified->formatType, _identified->flags
	);

	LOG("new cart parser @ 0x%08x", _cartParser);
	return true;
}

bool App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.setNextScreen(_qrCodeScreen);
	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_cartDump.toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);

	return true;
}

bool App::_cartDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartDumpWorker.save"));

	file::FileInfo info;
	char           path[32];

	__builtin_strcpy(path, EXTERNAL_DATA_DIR);

	if (!_fileProvider.getFileInfo(info, path)) {
		if (!_fileProvider.createDirectory(path))
			goto _error;
	}

	char   code[8], region[8];
	size_t length;

	length = _cartDump.getDumpLength();

	if (
		_identified && _cartParser->getCode(code) &&
		_cartParser->getRegion(region)
	) {
		snprintf(
			path, sizeof(path), EXTERNAL_DATA_DIR "/%s%s.573", code, region
		);
	} else {
		int index = 0;

		do {
			index++;
			snprintf(
				path, sizeof(path), EXTERNAL_DATA_DIR "/cart%d.573", index
			);
		} while (_fileProvider.getFileInfo(info, path));
	}

	LOG("saving %s, length=%d", path, length);

	if (_fileProvider.saveData(&_cartDump, length, path) != length)
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
	auto    error = _cartDriver->writeData();

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

	_cartDump.copyKeyFrom(key);
	return _cartUnlockWorker();
}

bool App::_cartRestoreWorker(void) {
	_workerStatus.update(0, 3, WSTR("App.cartRestoreWorker.init"));

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	cart::CartDump newDump;
	size_t         length;

	if (!_file)
		goto _fileOpenError;

	length = _file->read(&newDump, sizeof(newDump));

	if (length < (sizeof(newDump) - sizeof(newDump.data)))
		goto _fileError;
	if (!newDump.validateMagic())
		goto _fileError;
	if (length != newDump.getDumpLength())
		goto _fileError;

	_file->close();
	delete _file;

	if (_cartDump.chipType != newDump.chipType) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _filePickerScreen,
			WSTR("App.cartRestoreWorker.typeError"), path
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	cart::DriverError error;

	_workerStatus.update(1, 3, WSTR("App.cartRestoreWorker.setDataKey"));
	error = _cartDriver->setDataKey(newDump.dataKey);

	if (error) {
		LOG("key error [%s]", cart::getErrorString(error));
	} else {
		if (newDump.flags & (
			cart::DUMP_PUBLIC_DATA_OK | cart::DUMP_PRIVATE_DATA_OK
		))
			_cartDump.copyDataFrom(newDump.data);
		if (newDump.flags & cart::DUMP_CONFIG_OK)
			_cartDump.copyConfigFrom(newDump.config);

		_workerStatus.update(2, 3, WSTR("App.cartRestoreWorker.write"));
		error = _cartDriver->writeData();
	}

	_cartDetectWorker();

	if (error) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _filePickerScreen,
			WSTR("App.cartRestoreWorker.writeError"),
			cart::getErrorString(error)
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	return _cartUnlockWorker();

_fileError:
	_file->close();
	delete _file;

_fileOpenError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _filePickerScreen,
		WSTR("App.cartRestoreWorker.fileError"), path
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_cartReflashWorker(void) {
	// Make sure a valid cart ID is present if required by the new data.
	if (
		_selectedEntry->requiresCartID() &&
		!(_cartDump.flags & cart::DUMP_CART_ID_OK)
	) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _cartInfoScreen,
			WSTR("App.cartReflashWorker.idError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	_workerStatus.update(0, 3, WSTR("App.cartReflashWorker.init"));

	// TODO: preserve 0x81 traceid if possible
	//uint8_t traceID[8];
	//_cartParser->getIdentifiers()->traceID.copyTo(traceID);

	if (!_cartEraseWorker())
		return false;
	if (_cartParser)
		delete _cartParser;

	_cartParser = cart::newCartParser(
		_cartDump, _selectedEntry->formatType, _selectedEntry->flags
	);
	auto pri = _cartParser->getIdentifiers();
	auto pub = _cartParser->getPublicIdentifiers();

	_cartDump.clearData();
	_cartDump.initConfig(
		9, _selectedEntry->flags & cart::DATA_HAS_PUBLIC_SECTION
	);

	if (pri) {
		if (_selectedEntry->flags & cart::DATA_HAS_CART_ID)
			pri->cartID.copyFrom(_cartDump.cartID.data);
		if (_selectedEntry->flags & cart::DATA_HAS_TRACE_ID)
			pri->updateTraceID(
				_selectedEntry->traceIDType, _selectedEntry->traceIDParam,
				&_cartDump.cartID
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

	_cartParser->setCode(_selectedEntry->code);
	_cartParser->setRegion(_selectedEntry->region);
	_cartParser->setYear(_selectedEntry->year);
	_cartParser->flush();

	_workerStatus.update(1, 3, WSTR("App.cartReflashWorker.setDataKey"));
	auto error = _cartDriver->setDataKey(_selectedEntry->dataKey);

	if (error) {
		LOG("key error [%s]", cart::getErrorString(error));
	} else {
		_workerStatus.update(2, 3, WSTR("App.cartReflashWorker.write"));
		error = _cartDriver->writeData();
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

	auto error = _cartDriver->erase();
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
