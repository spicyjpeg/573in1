
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/file/file.hpp"
#include "common/defs.hpp"
#include "common/rom.hpp"
#include "common/romdrivers.hpp"
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

	// Store all dumps in a subdirectory named "dumpNNNN" within the main data
	// folder.
	char dirPath[file::MAX_PATH_LENGTH], filePath[file::MAX_PATH_LENGTH];

	if (!_createDataDirectory())
		goto _initError;
	if (!_getNumberedPath(
		dirPath, sizeof(dirPath), EXTERNAL_DATA_DIR "/dump%04d"
	))
		goto _initError;
	if (!_fileIO.vfs.createDirectory(dirPath))
		goto _initError;

	LOG_APP("saving dumps to %s", dirPath);

	for (auto &entry : _REGION_INFO) {
		if (!entry.region.isPresent())
			continue;

		auto regionLength = entry.region.getActualLength();

		// Fall back to dumping the entire address space if the card's size
		// could not be reliably autodetected.
		if (!regionLength)
			regionLength = entry.region.regionLength;

		size_t chunkLength = util::min(regionLength, _DUMP_CHUNK_LENGTH);
		size_t numChunks   = regionLength / chunkLength;

		snprintf(filePath, sizeof(filePath), entry.path, dirPath);

		auto _file = _fileIO.vfs.openFile(
			filePath, file::WRITE | file::ALLOW_CREATE
		);

		if (!_file)
			goto _fileError;

		util::Data buffer;
		uint32_t   offset = 0;

		buffer.allocate(chunkLength);

		for (size_t i = 0; i < numChunks; i++) {
			_workerStatus.update(i, numChunks, WSTRH(entry.dumpPrompt));
			entry.region.read(buffer.ptr, offset, chunkLength);

			if (_file->write(buffer.ptr, chunkLength) < chunkLength) {
				buffer.destroy();
				_file->close();
				delete _file;

				goto _fileError;
			}

			offset += chunkLength;
		}

		buffer.destroy();
		_file->close();
		delete _file;

		LOG_APP("%s saved", filePath);
	}

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, WSTR("App.romDumpWorker.success"), dirPath
	);
	return true;

_initError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romDumpWorker.initError"), dirPath
	);
	return false;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romDumpWorker.fileError"), filePath
	);
	return false;
}

bool App::_romRestoreWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.romRestoreWorker.init"));

	const char *path = _fileBrowserScreen.selectedPath;
	auto       _file = _fileIO.vfs.openFile(path, file::READ);

	auto region       = _storageActionsScreen.selectedRegion;
	auto regionLength = _storageActionsScreen.selectedLength;

	size_t bytesWritten = 0;

	util::Data buffers, chunkLengths;

	if (!_file)
		goto _fileError;
	if (!_romEraseWorker())
		return false;

	rom::Driver *driver;
	size_t      chipLength, numChips, maxChunkLength;

	driver         = region->newDriver();
	chipLength     = driver->getChipSize().chipLength;
	numChips       = (regionLength + chipLength - 1) / chipLength;
	maxChunkLength = util::min(regionLength, _DUMP_CHUNK_LENGTH / numChips);

	LOG_APP("%d chips, buf=%d", numChips, maxChunkLength);

	rom::DriverError error;

	buffers.allocate(maxChunkLength * numChips);
	chunkLengths.allocate<size_t>(numChips);

	// Parallelize writing by buffering a chunk for each chip into RAM, then
	// writing all chunks to the respective chips at the same time.
	for (size_t i = 0; i < chipLength; i += maxChunkLength) {
		_workerStatus.update(i, chipLength, WSTR("App.romRestoreWorker.write"));

		auto   bufferPtr   = buffers.as<uint8_t>();
		auto   lengthPtr   = chunkLengths.as<size_t>();
		size_t offset      = i;
		size_t totalLength = 0;

		for (
			size_t j = numChips; j > 0; j--, bufferPtr += maxChunkLength,
			offset += chipLength
		) {
			_file->seek(offset);
			auto length = _file->read(bufferPtr, maxChunkLength);

			// Data is written 16 bits at a time, so the chunk must be padded to
			// an even number of bytes.
			if (length % 2)
				bufferPtr[length++] = 0xff;

			*(lengthPtr++) = length;
			totalLength   += length;
		}

		// Stop once there is no more data to write.
		if (!totalLength)
			break;

		bufferPtr = buffers.as<uint8_t>();
		offset    = i;

		for (
			size_t j = 0; j < maxChunkLength; j += 2, bufferPtr += 2,
			offset += 2
		) {
			auto chunkOffset = offset;
			auto chunkPtr    = bufferPtr;
			lengthPtr        = chunkLengths.as<size_t>();

			for (
				size_t k = numChips; k > 0;
				k--, chunkPtr += maxChunkLength, chunkOffset += chipLength
			) {
				if (j >= *(lengthPtr++))
					continue;

				auto value = *reinterpret_cast<const uint16_t *>(chunkPtr);

				driver->write(chunkOffset, value);
			}

			chunkOffset = offset;
			chunkPtr    = bufferPtr;
			lengthPtr   = chunkLengths.as<size_t>();

			for (
				size_t k = numChips; k > 0; k--, chunkPtr += maxChunkLength,
				chunkOffset += chipLength, bytesWritten += 2
			) {
				if (j >= *(lengthPtr++))
					continue;

				auto value = *reinterpret_cast<const uint16_t *>(chunkPtr);
				error      = driver->flushWrite(chunkOffset, value);

				if (error)
					goto _flashError;
			}
		}
	}

	util::Hash message;

	message = (_file->size > regionLength)
		? "App.romRestoreWorker.overflow"_h
		: "App.romRestoreWorker.success"_h;

	buffers.destroy();
	chunkLengths.destroy();
	_file->close();
	delete _file;
	delete driver;

	_messageScreen.setMessage(MESSAGE_SUCCESS, WSTRH(message), bytesWritten);
	return true;

_fileError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romRestoreWorker.fileError"), path
	);
	return false;

_flashError:
	buffers.destroy();
	chunkLengths.destroy();
	_file->close();
	delete _file;
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romRestoreWorker.flashError"),
		rom::getErrorString(error), bytesWritten
	);
	return false;
}

bool App::_romEraseWorker(void) {
	auto region       = _storageActionsScreen.selectedRegion;
	auto regionLength = _storageActionsScreen.selectedLength;

	auto   driver       = region->newDriver();
	size_t chipLength   = driver->getChipSize().chipLength;
	size_t sectorLength = driver->getChipSize().eraseSectorLength;
	//size_t numChips     = (regionLength + chipLength - 1) / chipLength;

	size_t sectorsErased = 0;

	if (!chipLength)
		goto _unsupported;

	rom::DriverError error;

	_checksumScreen.valid = false;

	// Parallelize erasing by sending the same sector erase command to all chips
	// at the same time.
	for (size_t i = 0; i < chipLength; i += sectorLength) {
		_workerStatus.update(i, chipLength, WSTR("App.romEraseWorker.erase"));

		for (size_t j = 0; j < regionLength; j += chipLength)
			driver->eraseSector(i + j);

		for (size_t j = 0; j < regionLength; j += chipLength, sectorsErased++) {
			error = driver->flushErase(i + j);

			if (error)
				goto _flashError;
		}
	}

	delete driver;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, WSTR("App.romEraseWorker.success"), sectorsErased
	);
	return true;

_flashError:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romEraseWorker.flashError"),
		rom::getErrorString(error), sectorsErased
	);
	return false;

_unsupported:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.romEraseWorker.unsupported")
	);
	return false;
}

bool App::_flashHeaderWriteWorker(void) {
	auto   driver       = rom::flash.newDriver();
	size_t sectorLength = driver->getChipSize().eraseSectorLength;

	util::Data buffer;

	// This should never happen since the flash chips are soldered to the 573,
	// but whatever.
	if (!sectorLength)
		goto _unsupported;

	_checksumScreen.valid = false;
	_workerStatus.update(0, 2, WSTR("App.flashHeaderWriteWorker.erase"));

	// The flash can only be erased with sector granularity, so all data in the
	// first sector other than the header must be backed up and rewritten.
	rom::DriverError error;
	const uint16_t   *ptr;

	buffer.allocate(sectorLength);
	rom::flash.read(buffer.ptr, 0, sectorLength);

	driver->eraseSector(0);
	error = driver->flushErase(0);

	if (error)
		goto _flashError;

	_workerStatus.update(1, 2, WSTR("App.flashHeaderWriteWorker.write"));

	// Write the new header (if any).
	if (!_romHeaderDump.isDataEmpty()) {
		ptr = reinterpret_cast<const uint16_t *>(_romHeaderDump.data);

		for (
			uint32_t offset = rom::FLASH_HEADER_OFFSET;
			offset < rom::FLASH_CRC_OFFSET; offset += 2
		) {
			auto value = *(ptr++);

			driver->write(offset, value);
			error = driver->flushWrite(offset, value);

			if (error)
				goto _flashError;
		}
	}

	// Restore the rest of the sector that was erased.
	ptr = &buffer.as<const uint16_t>()[rom::FLASH_CRC_OFFSET / 2];

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

	buffer.destroy();
	delete driver;
	return true;

_flashError:
	buffer.destroy();
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.flashHeaderWriteWorker.flashError"),
		rom::getErrorString(error)
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_unsupported:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.flashHeaderWriteWorker.unsupported")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}
