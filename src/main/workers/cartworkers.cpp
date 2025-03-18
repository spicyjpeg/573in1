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
#include "common/bus.hpp"
#include "common/fs/file.hpp"
#include "common/sys573/ioboard.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "main/app/app.hpp"
#include "main/cart/cart.hpp"
#include "main/cart/cartdata.hpp"
#include "main/cart/cartio.hpp"
#include "main/workers/cartworkers.hpp"

static const char *const _CARTDB_PATHS[cart::NUM_CHIP_TYPES]{
	nullptr,
	"res:/data/x76f041.db",
	"res:/data/x76f100.db",
	"res:/data/zs01.db"
};

bool cartDetectWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartDetectWorker.readCart"));

	app._unloadCartData();
	app._qrCodeScreen.valid = false;

#ifdef ENABLE_DUMMY_CART_DRIVER
	if (!cart::dummyDriverDump.chipType)
		app._fileIO.loadStruct(cart::dummyDriverDump, "res:/data/dummy.dmp");

	if (cart::dummyDriverDump.chipType) {
		LOG_APP("using dummy cart driver");
		app._cartDriver = new cart::DummyDriver(app._cartDump);
	} else {
		app._cartDriver = cart::newCartDriver(app._cartDump);
	}
#else
	app._cartDriver = cart::newCartDriver(app._cartDump);
#endif

	if (app._cartDump.chipType) {
		auto error = app._cartDriver->readCartID();

		if (error)
			LOG_APP("SID error [%s]", cart::getErrorString(error));

		error = app._cartDriver->readPublicData();

		if (error)
			LOG_APP("read error [%s]", cart::getErrorString(error));
		else if (!app._cartDump.isReadableDataEmpty())
			app._cartParser = cart::newCartParser(app._cartDump);

		app._workerStatusScreen.setMessage(WSTR("App.cartDetectWorker.identifyGame"));

		if (!app._cartDB.ptr) {
			if (!app._fileIO.loadData(
				app._cartDB,
				_CARTDB_PATHS[app._cartDump.chipType]
			)) {
				LOG_APP("%s not found", _CARTDB_PATHS[app._cartDump.chipType]);
				goto _cartInitDone;
			}
		}

		char code[8], region[8];

		if (!app._cartParser)
			goto _cartInitDone;
		if (app._cartParser->getCode(code) && app._cartParser->getRegion(region))
			app._identified = app._cartDB.lookup(code, region);
		if (!app._identified)
			goto _cartInitDone;

		// Force the parser to use correct format for the game (to prevent
		// ambiguity between different formats).
		delete app._cartParser;
		app._cartParser = cart::newCartParser(
			app._cartDump,
			app._identified->formatType,
			app._identified->flags
		);
	}

_cartInitDone:
	app._workerStatusScreen.setMessage(WSTR("App.cartDetectWorker.readDigitalIO"));

	if (app._ioBoard->type == sys573::IO_DIGITAL) {
		util::Data bitstream;

		if (!app._fileIO.loadData(bitstream, "data/fpga.bit"))
			goto _done;

		bool ready = app._ioBoard->loadBitstream(
			bitstream.as<uint8_t>(),
			bitstream.length
		);
		bitstream.destroy();

		if (!ready)
			goto _done;

		auto id = reinterpret_cast<bus::OneWireID *>(&app._cartDump.systemID);

		if (!app._ioBoard->ds2401->readID(*id))
			LOG_APP("XID error");
	}

_done:
	app._ctx.show(app._cartInfoScreen);
	return true;
}

static const util::Hash _UNLOCK_ERRORS[cart::NUM_CHIP_TYPES]{
	0,
	"App.cartUnlockWorker.x76f041Error"_h,
	"App.cartUnlockWorker.x76f100Error"_h,
	"App.cartUnlockWorker.zs01Error"_h
};

bool cartUnlockWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartUnlockWorker.read"));

	app._qrCodeScreen.valid = false;

	auto error = app._cartDriver->readPrivateData();

	if (error) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTRH(_UNLOCK_ERRORS[app._cartDump.chipType]),
			cart::getErrorString(error)
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	if (app._cartParser)
		delete app._cartParser;

	app._cartParser = cart::newCartParser(app._cartDump);

	if (!app._cartParser)
		goto _done;

	app._workerStatusScreen.setMessage(WSTR("App.cartUnlockWorker.identifyGame"));

	char code[8], region[8];

	if (app._cartParser->getCode(code) && app._cartParser->getRegion(region))
		app._identified = app._cartDB.lookup(code, region);

	// If auto-identification failed (e.g. because the format has no game code),
	// use the game whose unlocking key was selected as a hint.
	if (!app._identified) {
		if (app._selectedEntry) {
			LOG_APP("identify failed, using key as hint");
			app._identified = app._selectedEntry;
		} else {
			goto _done;
		}
	}

	delete app._cartParser;
	app._cartParser = cart::newCartParser(
		app._cartDump,
		app._identified->formatType,
		app._identified->flags
	);

_done:
	app._ctx.show(app._cartInfoScreen, true);
	return true;
}

bool qrCodeWorker(App &app) {
	char qrString[cart::MAX_QR_STRING_LENGTH];

	app._workerStatusScreen.setMessage(WSTR("App.qrCodeWorker.compress"));
	app._cartDump.toQRString(qrString);

	app._workerStatusScreen.setMessage(WSTR("App.qrCodeWorker.generate"));
	app._qrCodeScreen.generateCode(qrString);

	app._ctx.show(app._qrCodeScreen);
	return true;
}

bool cartDumpWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartDumpWorker.save"));

	char   path[fs::MAX_PATH_LENGTH], code[8], region[8];
	size_t length = app._cartDump.getDumpLength();

	if (!app._createDataDirectory())
		goto _error;

	if (
		app._identified &&
		app._cartParser->getCode(code) &&
		app._cartParser->getRegion(region)
	) {
		snprintf(
			path,
			sizeof(path),
			EXTERNAL_DATA_DIR "/%s%s.dmp",
			code,
			region
		);
	} else {
		if (!app._fileIO.getNumberedPath(
			path,
			sizeof(path),
			EXTERNAL_DATA_DIR "/cart%04d.dmp"
		))
			goto _error;
	}

	LOG_APP("saving %s, length=%d", path, length);

	if (app._fileIO.saveData(&app._cartDump, length, path) != length)
		goto _error;

	app._messageScreen.setMessage(
		MESSAGE_SUCCESS,
		WSTR("App.cartDumpWorker.success"),
		path
	);
	app._ctx.show(app._messageScreen);
	return true;

_error:
	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.cartDumpWorker.error"),
		path
	);
	app._ctx.show(app._messageScreen);
	return false;
}

bool cartWriteWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartWriteWorker.write"));

	uint8_t key[8];
	auto    error = app._cartDriver->writeData();

	if (!error)
		app._identified->copyKeyTo(key);

	cartDetectWorker(app);

	if (error) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.cartWriteWorker.error"),
			cart::getErrorString(error)
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	app._cartDump.copyKeyFrom(key);
	return cartUnlockWorker(app);
}

bool cartRestoreWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartRestoreWorker.init"));

	const char *path = app._fileBrowserScreen.selectedPath;
	auto       file  = app._fileIO.openFile(path, fs::READ);

	cart::CartDump newDump;

	if (file) {
		auto length = file->read(&newDump, sizeof(newDump));

		file->close();
		delete file;

		if (length < (sizeof(newDump) - sizeof(newDump.data)))
			goto _fileError;
		if (!newDump.validateMagic())
			goto _fileError;
		if (length != newDump.getDumpLength())
			goto _fileError;
	}

	if (app._cartDump.chipType != newDump.chipType) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.cartRestoreWorker.typeError"),
			path
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	{
		app._workerStatusScreen.setMessage(WSTR("App.cartRestoreWorker.setDataKey"));
		auto error = app._cartDriver->setDataKey(newDump.dataKey);

		if (error) {
			LOG_APP("key error [%s]", cart::getErrorString(error));
		} else {
			if (newDump.flags & (
				cart::DUMP_PUBLIC_DATA_OK | cart::DUMP_PRIVATE_DATA_OK
			))
				app._cartDump.copyDataFrom(newDump.data);
			if (newDump.flags & cart::DUMP_CONFIG_OK)
				app._cartDump.copyConfigFrom(newDump.config);

			app._workerStatusScreen.setMessage(WSTR("App.cartRestoreWorker.write"));
			error = app._cartDriver->writeData();
		}

		cartDetectWorker(app);

		if (error) {
			app._messageScreen.setMessage(
				MESSAGE_ERROR,
				WSTR("App.cartRestoreWorker.writeError"),
				cart::getErrorString(error)
			);
			app._ctx.show(app._messageScreen);
			return false;
		}
	}

	return cartUnlockWorker(app);

_fileError:
	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.cartRestoreWorker.fileError"),
		path
	);
	app._ctx.show(app._messageScreen);
	return false;
}

bool cartReflashWorker(App &app) {
	// Make sure a valid cart ID is present if required by the new data.
	if (
		app._selectedEntry->requiresCartID() &&
		!(app._cartDump.flags & cart::DUMP_CART_ID_OK)
	) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.cartReflashWorker.idError")
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	app._workerStatusScreen.setMessage(WSTR("App.cartReflashWorker.init"));

	// TODO: preserve 0x81 traceid if possible
#if 0
	uint8_t traceID[8];
	_cartParser->getIdentifiers()->traceID.copyTo(traceID);
#endif

	if (!cartEraseWorker(app))
		return false;
	if (app._cartParser)
		delete app._cartParser;

	app._cartParser = cart::newCartParser(
		app._cartDump,
		app._selectedEntry->formatType,
		app._selectedEntry->flags
	);
	auto pri = app._cartParser->getIdentifiers();
	auto pub = app._cartParser->getPublicIdentifiers();

	util::clear(app._cartDump.data);
	app._cartDump.initConfig(
		9, app._selectedEntry->flags & cart::DATA_HAS_PUBLIC_SECTION
	);

	if (pri) {
		if (app._selectedEntry->flags & cart::DATA_HAS_CART_ID)
			pri->cartID.copyFrom(app._cartDump.cartID.data);
		if (app._selectedEntry->flags & cart::DATA_HAS_TRACE_ID)
			pri->updateTraceID(
				app._selectedEntry->traceIDType,
				app._selectedEntry->traceIDParam,
				&app._cartDump.cartID
			);
		if (app._selectedEntry->flags & cart::DATA_HAS_INSTALL_ID) {
			// The private installation ID seems to be unused on carts with a
			// public data section.
			if (pub)
				pub->setInstallID(app._selectedEntry->installIDPrefix);
			else
				pri->setInstallID(app._selectedEntry->installIDPrefix);
		}
	}

	app._cartParser->setCode(app._selectedEntry->code);
	app._cartParser->setRegion(app._selectedEntry->region);
	app._cartParser->setYear(app._selectedEntry->year);
	app._cartParser->flush();

	app._workerStatusScreen.setMessage(WSTR("App.cartReflashWorker.setDataKey"));
	auto error = app._cartDriver->setDataKey(app._selectedEntry->dataKey);

	if (error) {
		LOG_APP("key error [%s]", cart::getErrorString(error));
	} else {
		app._workerStatusScreen.setMessage(WSTR("App.cartReflashWorker.write"));
		error = app._cartDriver->writeData();
	}

	cartDetectWorker(app);

	if (error) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.cartReflashWorker.writeError"),
			cart::getErrorString(error)
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	return cartUnlockWorker(app);
}

bool cartEraseWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.cartEraseWorker.erase"));

	auto error = app._cartDriver->erase();

	cartDetectWorker(app);

	if (error) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.cartEraseWorker.error"),
			cart::getErrorString(error)
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	return cartUnlockWorker(app);
}
