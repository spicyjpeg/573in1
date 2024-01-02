
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

	if (_identified && _parser->getCode(code) && _parser->getRegion(region)) {
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
