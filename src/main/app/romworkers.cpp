
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"

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

// TODO: all these *really* need a cleanup...

bool App::_romDumpWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romDumpWorker.init"));

	// Store all dumps in a subdirectory named "dumpN" within the main data
	// folder.
	file::FileInfo info;
	char           dirPath[32];

	__builtin_strcpy(dirPath, EXTERNAL_DATA_DIR);

	if (!_fileProvider.getFileInfo(info, dirPath)) {
		if (!_fileProvider.createDirectory(dirPath))
			goto _initError;
	}

	int  index;
	char filePath[32];

	index = 0;

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

		size_t chunkLength =
			util::min(entry.region.regionLength, _DUMP_CHUNK_LENGTH);
		size_t numChunks   = entry.region.regionLength / chunkLength;

		snprintf(filePath, sizeof(filePath), entry.path, dirPath);

		auto _file = _fileProvider.openFile(
			filePath, file::WRITE | file::ALLOW_CREATE
		);

		if (!_file)
			goto _fileError;

		auto     buffer = new uint8_t[chunkLength];
		uint32_t offset = 0;

		//assert(buffer);

		for (size_t i = 0; i < numChunks; i++) {
			_workerStatus.update(i, numChunks, WSTRH(entry.dumpPrompt));
			entry.region.read(buffer, offset, chunkLength);

			if (_file->write(buffer, chunkLength) < chunkLength) {
				delete   _file;
				delete[] buffer;

				goto _fileError;
			}

			offset += chunkLength;
		}

		delete   _file;
		delete[] buffer;

		LOG("%s saved", filePath);
	}

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageMenuScreen, WSTR("App.romDumpWorker.success"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_initError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen, WSTR("App.romDumpWorker.initError"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen, WSTR("App.romDumpWorker.fileError"),
		filePath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_romRestoreWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romRestoreWorker.init"));

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	auto &region = _storageMenuScreen.getSelectedRegion();

	if (!_file)
		goto _fileError;
	if (!_romEraseWorker())
		return false;

	size_t fileLength, dataLength;

	fileLength = size_t(_file->length);
	dataLength = util::min(fileLength, region.regionLength);

	rom::Driver *driver;
	size_t      sectorLength, numSectors;

	driver       = region.newDriver();
	sectorLength = driver->getChipSize().eraseSectorLength;
	numSectors   = (dataLength + sectorLength - 1) / sectorLength;

	uint8_t  *buffer;
	uint32_t offset;

	buffer = new uint8_t[sectorLength];
	offset = 0;

	rom::DriverError error;

	//assert(buffer);

	for (size_t i = 0; i < numSectors; i++) {
		_workerStatus.update(i, numSectors, WSTR("App.romRestoreWorker.write"));

		auto length = _file->read(buffer, sectorLength);
		auto ptr    = reinterpret_cast<const uint16_t *>(buffer);

		// Data is written 16 bits at a time, so the buffer must be padded to an
		// even number of bytes.
		if (length % 2)
			buffer[length++] = 0xff;

		for (uint32_t end = offset + length; offset < end; offset += 2) {
			auto value = *(ptr++);

			driver->write(offset, value);
			error = driver->flushWrite(offset, value);

			if (error)
				goto _flashError;
		}
	}

	delete   _file;
	delete[] buffer;
	delete   driver;

	util::Hash message;

	message = (fileLength > dataLength)
		? "App.romRestoreWorker.overflow"_h
		: "App.romRestoreWorker.success"_h;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageMenuScreen, WSTRH(message), offset
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romRestoreWorker.fileError"), path
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_flashError:
	delete   _file;
	delete[] buffer;
	delete   driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romRestoreWorker.flashError"), rom::getErrorString(error),
		offset
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_romEraseWorker(void) {
	auto &region = _storageMenuScreen.getSelectedRegion();
	auto driver  = region.newDriver();

	size_t chipLength    = driver->getChipSize().chipLength;
	size_t sectorLength  = driver->getChipSize().eraseSectorLength;
	size_t sectorsErased = 0;

	if (!chipLength)
		goto _unsupported;

	_systemInfo.flags = 0;

	rom::DriverError error;

	// Erase one sector at a time on each chip.
	for (size_t i = 0; i < chipLength; i += sectorLength) {
		_workerStatus.update(i, chipLength, WSTR("App.romEraseWorker.erase"));

		for (size_t j = 0; j < region.regionLength; j += chipLength)
			driver->eraseSector(i + j);

		for (
			size_t j = 0; j < region.regionLength; j += chipLength,
			sectorsErased++
		) {
			error = driver->flushErase(i + j);

			if (error)
				goto _flashError;
		}
	}

	delete driver;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageMenuScreen, WSTR("App.romEraseWorker.success"),
		sectorsErased
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_flashError:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romEraseWorker.flashError"), rom::getErrorString(error),
		sectorsErased
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_unsupported:
	auto id = region.getJEDECID();
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romEraseWorker.unsupported"),
		(id >>  0) & 0xff,
		(id >>  8) & 0xff,
		(id >> 16) & 0xff,
		(id >> 24) & 0xff
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_systemInfoWorker(void) {
	// This is necessary to ensure the digital I/O ID is read at least once.
	if (!_driver)
		_cartDetectWorker();

	_workerStatus.setNextScreen(_systemInfoScreen);
	_systemInfo.flags = 0;

	for (auto &entry : _DUMP_ENTRIES) {
		if (!entry.region.isPresent())
			continue;

		size_t chunkLength =
			util::min(entry.region.regionLength, _DUMP_CHUNK_LENGTH);
		size_t numChunks   = entry.region.regionLength / chunkLength;

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

	_systemInfo.flash.jedecID  = rom::flash.getJEDECID();
	_systemInfo.flash.bootable = rom::flash.hasBootExecutable();

	for (int i = 0; i < 2; i++) {
		auto &region = rom::pcmcia[i];
		auto &card   = _systemInfo.pcmcia[i];

		if (region.isPresent()) {
			card.jedecID  = region.getJEDECID();
			card.bootable = region.hasBootExecutable();
		}
	}

	return true;
}
