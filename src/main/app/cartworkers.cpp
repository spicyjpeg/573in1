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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/fs/file.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "common/ioboard.hpp"
#include "main/app/app.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartdata.hpp"
#include "main/cart/cartio.hpp"

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

#ifdef ENABLE_DUMMY_CART_DRIVER
	if (!cart::dummyDriverDump.chipType)
		_fileIO.resource.loadStruct(cart::dummyDriverDump, "data/test.573");

	if (cart::dummyDriverDump.chipType) {
		LOG_APP("using dummy cart driver");
		_cartDriver = new cart::DummyDriver(_cartDump);
		_cartDriver->readSystemID();
	} else {
		_cartDriver = cart::newCartDriver(_cartDump);
	}
#else
	_cartDriver = cart::newCartDriver(_cartDump);
#endif

	if (_cartDump.chipType) {
		auto error = _cartDriver->readCartID();

		if (error)
			LOG_APP("SID error [%s]", cart::getErrorString(error));

		error = _cartDriver->readPublicData();

		if (error)
			LOG_APP("read error [%s]", cart::getErrorString(error));
		else if (!_cartDump.isReadableDataEmpty())
			_cartParser = cart::newCartParser(_cartDump);

		_workerStatus.update(1, 3, WSTR("App.cartDetectWorker.identifyGame"));

		if (!_cartDB.ptr) {
			if (!_fileIO.resource.loadData(
				_cartDB, _CARTDB_PATHS[_cartDump.chipType])
			) {
				LOG_APP("%s not found", _CARTDB_PATHS[_cartDump.chipType]);
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
	}

_cartInitDone:
	_workerStatus.update(2, 3, WSTR("App.cartDetectWorker.readDigitalIO"));

	if (
#ifdef ENABLE_DUMMY_CART_DRIVER
		!(_cartDump.flags & cart::DUMP_SYSTEM_ID_OK) &&
#endif
		io::isDigitalIOPresent()
	) {
		util::Data bitstream;

		if (!_fileIO.resource.loadData(bitstream, "data/fpga.bit"))
			return true;

		bool ready = io::loadDigitalIOBitstream(
			bitstream.as<uint8_t>(), bitstream.length
		);
		bitstream.destroy();

		if (!ready)
			return true;

		io::initDigitalIOFPGA();
		auto error = _cartDriver->readSystemID();

		if (error)
			LOG_APP("XID error [%s]", cart::getErrorString(error));
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
			MESSAGE_ERROR, WSTRH(_UNLOCK_ERRORS[_cartDump.chipType]),
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

	_workerStatus.update(1, 2, WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (_cartParser->getCode(code) && _cartParser->getRegion(region))
		_identified = _cartDB.lookup(code, region);

	// If auto-identification failed (e.g. because the format has no game code),
	// use the game whose unlocking key was selected as a hint.
	if (!_identified) {
		if (_selectedEntry) {
			LOG_APP("identify failed, using key as hint");
			_identified = _selectedEntry;
		} else {
			return true;
		}
	}

	delete _cartParser;
	_cartParser = cart::newCartParser(
		_cartDump, _identified->formatType, _identified->flags
	);
	return true;
}

bool App::_qrCodeWorker(void) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	_workerStatus.update(0, 2, WSTR("App.qrCodeWorker.compress"));
	_cartDump.toQRString(qrString);

	_workerStatus.update(1, 2, WSTR("App.qrCodeWorker.generate"));
	_qrCodeScreen.generateCode(qrString);

	return true;
}

bool App::_cartDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.cartDumpWorker.save"));

	char   path[fs::MAX_PATH_LENGTH], code[8], region[8];
	size_t length = _cartDump.getDumpLength();

	if (!_createDataDirectory())
		goto _error;

	if (
		_identified && _cartParser->getCode(code) &&
		_cartParser->getRegion(region)
	) {
		snprintf(
			path, sizeof(path), EXTERNAL_DATA_DIR "/%s%s.573", code, region
		);
	} else {
		if (!_getNumberedPath(
			path, sizeof(path), EXTERNAL_DATA_DIR "/cart%04d.573"
		))
			goto _error;
	}

	LOG_APP("saving %s, length=%d", path, length);

	if (_fileIO.vfs.saveData(&_cartDump, length, path) != length)
		goto _error;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, WSTR("App.cartDumpWorker.success"), path
	);
	return true;

_error:
	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.cartDumpWorker.error"), path
	);
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
			MESSAGE_ERROR, WSTR("App.cartWriteWorker.error"),
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

	const char *path = _fileBrowserScreen.selectedPath;
	auto       _file = _fileIO.vfs.openFile(path, fs::READ);

	cart::CartDump newDump;

	if (_file) {
		auto length = _file->read(&newDump, sizeof(newDump));

		_file->close();
		delete _file;

		if (length < (sizeof(newDump) - sizeof(newDump.data)))
			goto _fileError;
		if (!newDump.validateMagic())
			goto _fileError;
		if (length != newDump.getDumpLength())
			goto _fileError;
	}

	if (_cartDump.chipType != newDump.chipType) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, WSTR("App.cartRestoreWorker.typeError"), path
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	{
		_workerStatus.update(1, 3, WSTR("App.cartRestoreWorker.setDataKey"));
		auto error = _cartDriver->setDataKey(newDump.dataKey);

		if (error) {
			LOG_APP("key error [%s]", cart::getErrorString(error));
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
				MESSAGE_ERROR, WSTR("App.cartRestoreWorker.writeError"),
				cart::getErrorString(error)
			);
			_workerStatus.setNextScreen(_messageScreen);
			return false;
		}
	}

	return _cartUnlockWorker();

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.cartRestoreWorker.fileError"), path
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
			MESSAGE_ERROR, WSTR("App.cartReflashWorker.idError")
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	_workerStatus.update(0, 3, WSTR("App.cartReflashWorker.init"));

	// TODO: preserve 0x81 traceid if possible
#if 0
	uint8_t traceID[8];
	_cartParser->getIdentifiers()->traceID.copyTo(traceID);
#endif

	if (!_cartEraseWorker())
		return false;
	if (_cartParser)
		delete _cartParser;

	_cartParser = cart::newCartParser(
		_cartDump, _selectedEntry->formatType, _selectedEntry->flags
	);
	auto pri = _cartParser->getIdentifiers();
	auto pub = _cartParser->getPublicIdentifiers();

	util::clear(_cartDump.data);
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
		LOG_APP("key error [%s]", cart::getErrorString(error));
	} else {
		_workerStatus.update(2, 3, WSTR("App.cartReflashWorker.write"));
		error = _cartDriver->writeData();
	}

	_cartDetectWorker();

	if (error) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, WSTR("App.cartReflashWorker.writeError"),
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
			MESSAGE_ERROR, WSTR("App.cartEraseWorker.error"),
			cart::getErrorString(error)
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	return _cartUnlockWorker();
}
