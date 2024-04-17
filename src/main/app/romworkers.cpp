
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/romactions.hpp"

struct RegionInfo {
public:
	util::Hash        dumpPrompt, crcPrompt;
	const char        *path;
	const rom::Region &region;
	size_t            crcOffset;
};

static const RegionInfo _REGION_INFO[]{
	{
		.dumpPrompt = "App.romDumpWorker.dumpBIOS"_h,
		.crcPrompt  = "App.romChecksumWorker.hashBIOS"_h,
		.path       = "%s/bios.bin",
		.region     = rom::bios,
		.crcOffset  = offsetof(ChecksumValues, bios)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpRTC"_h,
		.crcPrompt  = "App.romChecksumWorker.hashRTC"_h,
		.path       = "%s/rtc.bin",
		.region     = rom::rtc,
		.crcOffset  = offsetof(ChecksumValues, rtc)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpFlash"_h,
		.crcPrompt  = "App.romChecksumWorker.hashFlash"_h,
		.path       = "%s/flash.bin",
		.region     = rom::flash,
		.crcOffset  = offsetof(ChecksumValues, flash)
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpPCMCIA1"_h,
		.crcPrompt  = "App.romChecksumWorker.hashPCMCIA1"_h,
		.path       = "%s/pcmcia1.bin",
		.region     = rom::pcmcia[0],
		.crcOffset  = offsetof(ChecksumValues, pcmcia[0])
	}, {
		.dumpPrompt = "App.romDumpWorker.dumpPCMCIA2"_h,
		.crcPrompt  = "App.romChecksumWorker.hashPCMCIA2"_h,
		.path       = "%s/pcmcia2.bin",
		.region     = rom::pcmcia[1],
		.crcOffset  = offsetof(ChecksumValues, pcmcia[1])
	}
};

static constexpr size_t _DUMP_CHUNK_LENGTH   = 0x80000;
static constexpr size_t _DUMP_CHUNKS_PER_CRC = 32; // Save CRC32 every 16 MB

// TODO: all these *really* need a cleanup...

bool App::_romChecksumWorker(void) {
	_workerStatus.setNextScreen(_checksumScreen);
	_checksumScreen.valid = false;

	for (auto &entry : _REGION_INFO) {
		if (!entry.region.isPresent())
			continue;

		size_t chunkLength =
			util::min(entry.region.regionLength, _DUMP_CHUNK_LENGTH);
		size_t numChunks   = entry.region.regionLength / chunkLength;

		uint32_t offset = 0;
		uint32_t crc    = 0;
		auto     crcPtr = reinterpret_cast<uint32_t *>(
			reinterpret_cast<uintptr_t>(&_checksumScreen.values) +
			entry.crcOffset
		);

		// Flash cards can be 16, 32 or 64 MB, so copies of the current CRC are
		// saved after the first 16, then 32, 48 and finally 64 MB are read.
		for (size_t i = 0; i < numChunks; i += _DUMP_CHUNKS_PER_CRC) {
			size_t end = util::min(i + _DUMP_CHUNKS_PER_CRC, numChunks);

			for (size_t j = i; j < end; j++) {
				_workerStatus.update(j, numChunks, WSTRH(entry.crcPrompt));

				crc     = entry.region.zipCRC32(offset, chunkLength, crc);
				offset += chunkLength;
			}

			*(crcPtr++) = crc;
		}
	}

	_checksumScreen.valid = true;
	return true;
}

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

	for (auto &entry : _REGION_INFO) {
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
				_file->close();
				delete   _file;
				delete[] buffer;

				goto _fileError;
			}

			offset += chunkLength;
		}

		_file->close();
		delete   _file;
		delete[] buffer;

		LOG("%s saved", filePath);
	}

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageInfoScreen, WSTR("App.romDumpWorker.success"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_initError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen, WSTR("App.romDumpWorker.initError"),
		dirPath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen, WSTR("App.romDumpWorker.fileError"),
		filePath
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_romRestoreWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romRestoreWorker.init"));

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	auto &region = _storageActionsScreen.getSelectedRegion();

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

	rom::DriverError error;
	uint8_t          *buffer;
	uint32_t         offset;

	buffer = new uint8_t[sectorLength];
	offset = 0;

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

	_file->close();
	delete   _file;
	delete[] buffer;
	delete   driver;

	util::Hash message;

	message = (fileLength > dataLength)
		? "App.romRestoreWorker.overflow"_h
		: "App.romRestoreWorker.success"_h;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageInfoScreen, WSTRH(message), offset
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.romRestoreWorker.fileError"), path
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_flashError:
	_file->close();
	delete   _file;
	delete[] buffer;
	delete   driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.romRestoreWorker.flashError"), rom::getErrorString(error),
		offset
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_romEraseWorker(void) {
	auto &region = _storageActionsScreen.getSelectedRegion();
	auto driver  = region.newDriver();

	size_t chipLength    = driver->getChipSize().chipLength;
	size_t sectorLength  = driver->getChipSize().eraseSectorLength;
	size_t sectorsErased = 0;

	if (!chipLength)
		goto _unsupported;

	rom::DriverError error;

	_checksumScreen.valid = false;

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
		MESSAGE_SUCCESS, _storageInfoScreen, WSTR("App.romEraseWorker.success"),
		sectorsErased
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_flashError:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.romEraseWorker.flashError"), rom::getErrorString(error),
		sectorsErased
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_unsupported:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.romEraseWorker.unsupported")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_flashHeaderWriteWorker(void) {
	if (!_flashHeaderEraseWorker())
		return false;

	auto driver = rom::flash.newDriver();

	_workerStatus.update(1, 2, WSTR("App.flashHeaderWriteWorker.write"));

	rom::DriverError error;

	// TODO: implement

	delete driver;

	_workerStatus.setNextScreen(_storageInfoScreen);
	return true;

_flashError:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.flashHeaderWriteWorker.flashError"),
		rom::getErrorString(error)
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_flashHeaderEraseWorker(void) {
	auto   driver       = rom::flash.newDriver();
	size_t sectorLength = driver->getChipSize().eraseSectorLength;

	// This should never happen since the flash chips are soldered to the 573,
	// but whatever.
	if (!sectorLength)
		goto _unsupported;

	_checksumScreen.valid = false;
	_workerStatus.update(0, 1, WSTR("App.flashHeaderEraseWorker.erase"));

	// The flash can only be erased with sector granularity, so all data in the
	// first sector other than the header must be backed up and rewritten.
	rom::DriverError error;
	uint8_t          *buffer;
	const uint16_t   *ptr;

	buffer = new uint8_t[sectorLength];

	//assert(buffer);

	rom::flash.read(buffer, 0, sectorLength);

	driver->eraseSector(0);
	error = driver->flushErase(0);

	if (error)
		goto _flashError;

	ptr = reinterpret_cast<const uint16_t *>(&buffer[rom::FLASH_CRC_OFFSET]);

	for (
		uint32_t offset = rom::FLASH_CRC_OFFSET; offset < sectorLength;
		offset += 2
	) {
		auto value = *(ptr++);

		driver->write(offset, value);
		error = driver->flushWrite(offset, value);

		if (error)
			goto _flashError;
	}

	delete[] buffer;
	delete   driver;

	_workerStatus.setNextScreen(_storageInfoScreen);
	return true;

_flashError:
	delete[] buffer;
	delete   driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.flashHeaderEraseWorker.flashError"),
		rom::getErrorString(error)
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_unsupported:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageInfoScreen,
		WSTR("App.flashHeaderEraseWorker.unsupported")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}
