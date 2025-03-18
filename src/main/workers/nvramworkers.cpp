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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/fs/file.hpp"
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "common/defs.hpp"
#include "common/rom.hpp"
#include "common/romdrivers.hpp"
#include "main/app/app.hpp"
#include "main/app/nvramactions.hpp"
#include "main/workers/nvramworkers.hpp"

struct RegionInfo {
public:
	util::Hash        dumpPrompt, crcPrompt;
	const char        *path;
	const rom::Region &region;
	size_t            crcOffset;
};

static const RegionInfo _REGION_INFO[]{
	{
		.dumpPrompt = "App.nvramDumpWorker.dumpBIOS"_h,
		.crcPrompt  = "App.nvramChecksumWorker.hashBIOS"_h,
		.path       = "%s/bios.bin",
		.region     = rom::bios,
		.crcOffset  = offsetof(ChecksumValues, bios)
	}, {
		.dumpPrompt = "App.nvramDumpWorker.dumpRTC"_h,
		.crcPrompt  = "App.nvramChecksumWorker.hashRTC"_h,
		.path       = "%s/rtc.bin",
		.region     = rom::rtc,
		.crcOffset  = offsetof(ChecksumValues, rtc)
	}, {
		.dumpPrompt = "App.nvramDumpWorker.dumpFlash"_h,
		.crcPrompt  = "App.nvramChecksumWorker.hashFlash"_h,
		.path       = "%s/flash.bin",
		.region     = rom::flash,
		.crcOffset  = offsetof(ChecksumValues, flash)
	}, {
		.dumpPrompt = "App.nvramDumpWorker.dumpPCMCIA1"_h,
		.crcPrompt  = "App.nvramChecksumWorker.hashPCMCIA1"_h,
		.path       = "%s/pcmcia1.bin",
		.region     = rom::pcmcia[0],
		.crcOffset  = offsetof(ChecksumValues, pcmcia[0])
	}, {
		.dumpPrompt = "App.nvramDumpWorker.dumpPCMCIA2"_h,
		.crcPrompt  = "App.nvramChecksumWorker.hashPCMCIA2"_h,
		.path       = "%s/pcmcia2.bin",
		.region     = rom::pcmcia[1],
		.crcOffset  = offsetof(ChecksumValues, pcmcia[1])
	}
};

static constexpr size_t _DUMP_CHUNK_LENGTH   = 0x80000;
static constexpr size_t _DUMP_CHUNKS_PER_CRC = 32; // Save CRC32 every 16 MB

// TODO: all these *really* need a cleanup...

bool nvramChecksumWorker(App &app) {
	app._checksumScreen.valid = false;

	for (auto &entry : _REGION_INFO) {
		if (!entry.region.isPresent())
			continue;

		size_t chunkLength =
			util::min(entry.region.regionLength, _DUMP_CHUNK_LENGTH);
		size_t numChunks   = entry.region.regionLength / chunkLength;

		uint32_t offset = 0;
		uint32_t crc    = 0;
		auto     crcPtr = reinterpret_cast<uint32_t *>(
			reinterpret_cast<uintptr_t>(&app._checksumScreen.values) +
			entry.crcOffset
		);

		app._workerStatusScreen.setMessage(WSTRH(entry.crcPrompt));

		// Flash cards can be 16, 32 or 64 MB, so copies of the current CRC are
		// saved after the first 16, then 32, 48 and finally 64 MB are read.
		for (size_t i = 0; i < numChunks; i += _DUMP_CHUNKS_PER_CRC) {
			size_t end = util::min(i + _DUMP_CHUNKS_PER_CRC, numChunks);

			for (size_t j = i; j < end; j++) {
				app._workerStatusScreen.setProgress(app._ctx, j, numChunks);

				crc     = entry.region.zipCRC32(offset, chunkLength, crc);
				offset += chunkLength;
			}

			*(crcPtr++) = crc;
		}
	}

	app._checksumScreen.valid = true;
	app._ctx.show(app._checksumScreen);
	return true;
}

bool nvramDumpWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.nvramDumpWorker.init"));

	// Store all dumps in a subdirectory named "dumpNNNN" within the main data
	// folder.
	char dirPath[fs::MAX_PATH_LENGTH], filePath[fs::MAX_PATH_LENGTH];

	if (!app._createDataDirectory())
		goto _initError;
	if (!app._fileIO.getNumberedPath(
		dirPath,
		sizeof(dirPath),
		EXTERNAL_DATA_DIR "/dump%04d"
	))
		goto _initError;
	if (!app._fileIO.createDirectory(dirPath))
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

		app._workerStatusScreen.setMessage(WSTRH(entry.dumpPrompt));
		snprintf(filePath, sizeof(filePath), entry.path, dirPath);

		auto file =
			app._fileIO.openFile(filePath, fs::WRITE | fs::ALLOW_CREATE);

		if (!file)
			goto _fileError;

		util::Data buffer;
		uint32_t   offset = 0;

		buffer.allocate(chunkLength);

		for (size_t i = 0; i < numChunks; i++) {
			app._workerStatusScreen.setProgress(app._ctx, i, numChunks);

			entry.region.read(buffer.ptr, offset, chunkLength);

			if (file->write(buffer.ptr, chunkLength) < chunkLength) {
				file->close();
				delete file;

				goto _fileError;
			}

			offset += chunkLength;
		}

		file->close();
		delete file;

		LOG_APP("%s saved", filePath);
	}

	app._messageScreen.setMessage(
		MESSAGE_SUCCESS,
		WSTR("App.nvramDumpWorker.success"),
		dirPath
	);
	app._ctx.show(app._messageScreen);
	return true;

_initError:
	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.nvramDumpWorker.initError"),
		dirPath
	);
	app._ctx.show(app._messageScreen);
	return false;

_fileError:
	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.nvramDumpWorker.fileError"),
		filePath
	);
	app._ctx.show(app._messageScreen);
	return false;
}

bool nvramRestoreWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.nvramRestoreWorker.init"));

	const char *path = app._fileBrowserScreen.selectedPath;
	auto       file  = app._fileIO.openFile(path, fs::READ);

	if (!file) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.nvramRestoreWorker.fileError"),
			path
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	if (!nvramEraseWorker(app))
		return false;

	auto region       = app._nvramActionsScreen.selectedRegion;
	auto regionLength = app._nvramActionsScreen.selectedLength;

	auto driver         = region->newDriver();
	auto chipLength     = driver->getChipSize().chipLength;
	auto numChips       = (regionLength + chipLength - 1) / chipLength;
	auto maxChunkLength = util::min(regionLength, _DUMP_CHUNK_LENGTH / numChips);

	app._workerStatusScreen.setMessage(WSTR("App.nvramRestoreWorker.write"));

	LOG_APP("%d chips, buf=%d", numChips, maxChunkLength);

	util::Data buffers, chunkLengths;
	size_t     bytesWritten = 0;

	buffers.allocate(maxChunkLength * numChips);
	chunkLengths.allocate<size_t>(numChips);

	// Parallelize writing by buffering a chunk for each chip into RAM, then
	// writing all chunks to the respective chips at the same time.
	for (size_t i = 0; i < chipLength; i += maxChunkLength) {
		app._workerStatusScreen.setProgress(app._ctx, i, chipLength);

		auto   bufferPtr   = buffers.as<uint8_t>();
		auto   lengthPtr   = chunkLengths.as<size_t>();
		size_t offset      = i;
		size_t totalLength = 0;

		for (
			size_t j = numChips;
			j > 0;
			j--,
			bufferPtr += maxChunkLength,
			offset    += chipLength
		) {
			file->seek(offset);
			auto length = file->read(bufferPtr, maxChunkLength);

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
			size_t j = 0;
			j < maxChunkLength;
			j         += 2,
			bufferPtr += 2,
			offset    += 2
		) {
			auto chunkOffset = offset;
			auto chunkPtr    = bufferPtr;
			lengthPtr        = chunkLengths.as<size_t>();

			for (
				size_t k = numChips;
				k > 0;
				k--,
				chunkPtr    += maxChunkLength,
				chunkOffset += chipLength
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
				size_t k = numChips;
				k > 0;
				k--,
				chunkPtr     += maxChunkLength,
				chunkOffset  += chipLength,
				bytesWritten += 2
			) {
				if (j >= *(lengthPtr++))
					continue;

				auto value = *reinterpret_cast<const uint16_t *>(chunkPtr);
				auto error = driver->flushWrite(chunkOffset, value);

				if (!error)
					continue;

				file->close();
				delete file;
				delete driver;

				app._messageScreen.setMessage(
					MESSAGE_ERROR,
					WSTR("App.nvramRestoreWorker.flashError"),
					rom::getErrorString(error),
					bytesWritten
				);
				app._ctx.show(app._messageScreen);
				return false;
			}
		}
	}

	util::Hash message;

	message = (file->size > regionLength)
		? "App.nvramRestoreWorker.overflow"_h
		: "App.nvramRestoreWorker.success"_h;

	file->close();
	delete file;
	delete driver;

	app._messageScreen.setMessage(MESSAGE_SUCCESS, WSTRH(message), bytesWritten);
	app._ctx.show(app._messageScreen);
	return true;
}

bool nvramEraseWorker(App &app) {
	auto region       = app._nvramActionsScreen.selectedRegion;
	auto regionLength = app._nvramActionsScreen.selectedLength;

	auto   driver       = region->newDriver();
	size_t chipLength   = driver->getChipSize().chipLength;
	size_t sectorLength = driver->getChipSize().eraseSectorLength;

	size_t sectorsErased = 0;

	if (!chipLength) {
		delete driver;

		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.nvramEraseWorker.unsupported")
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	app._checksumScreen.valid = false;

	app._workerStatusScreen.setMessage(WSTR("App.nvramEraseWorker.erase"));

	// Parallelize erasing by sending the same sector erase command to all chips
	// at the same time.
	for (size_t i = 0; i < chipLength; i += sectorLength) {
		app._workerStatusScreen.setProgress(app._ctx, i, chipLength);

		for (size_t j = 0; j < regionLength; j += chipLength)
			driver->eraseSector(i + j);

		for (size_t j = 0; j < regionLength; j += chipLength, sectorsErased++) {
			auto error = driver->flushErase(i + j);

			if (!error)
				continue;

			delete driver;

			app._messageScreen.setMessage(
				MESSAGE_ERROR,
				WSTR("App.nvramEraseWorker.flashError"),
				rom::getErrorString(error),
				sectorsErased
			);
			app._ctx.show(app._messageScreen);
			return false;
		}
	}

	delete driver;

	app._messageScreen.setMessage(
		MESSAGE_SUCCESS,
		WSTR("App.nvramEraseWorker.success"),
		sectorsErased
	);
	app._ctx.show(app._messageScreen);
	return true;
}

bool flashExecutableWriteWorker(App &app) {
	// TODO: implement
	return false;
}

bool flashHeaderWriteWorker(App &app) {
	auto   driver       = rom::flash.newDriver();
	size_t sectorLength = driver->getChipSize().eraseSectorLength;

	// This should never happen since the flash chips are soldered to the 573,
	// but whatever.
	if (!sectorLength) {
		delete driver;

		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.flashHeaderWriteWorker.unsupported")
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	app._checksumScreen.valid = false;
	app._workerStatusScreen.setMessage(WSTR("App.flashHeaderWriteWorker.erase"));

	// The flash can only be erased with sector granularity, so all data in the
	// first sector other than the header must be backed up and rewritten.
	util::Data buffer;

	buffer.allocate(sectorLength);
	rom::flash.read(buffer.ptr, 0, sectorLength);

	driver->eraseSector(0);
	auto error = driver->flushErase(0);

	if (error)
		goto _flashError;

	app._workerStatusScreen.setMessage(WSTR("App.flashHeaderWriteWorker.write"));

	// Write the new header (if any).
	if (!app._romHeaderDump.isDataEmpty()) {
		auto ptr = reinterpret_cast<const uint16_t *>(app._romHeaderDump.data);

		for (
			uint32_t offset = rom::FLASH_HEADER_OFFSET;
			offset < rom::FLASH_CRC_OFFSET;
			offset += 2
		) {
			auto value = *(ptr++);

			driver->write(offset, value);
			error = driver->flushWrite(offset, value);

			if (error)
				goto _flashError;
		}
	}

	// Restore the rest of the sector that was erased.
	{
		auto ptr = buffer.as<const uint16_t>() + (rom::FLASH_CRC_OFFSET / 2);

		for (
			uint32_t offset = rom::FLASH_CRC_OFFSET;
			offset < sectorLength;
			offset += 2
		) {
			auto value = *(ptr++);

			driver->write(offset, value);
			error = driver->flushWrite(offset, value);

			if (error)
				goto _flashError;
		}
	}

	delete driver;

	app._ctx.show(app._nvramInfoScreen, true);
	return true;

_flashError:
	delete driver;

	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.flashHeaderWriteWorker.flashError"),
		rom::getErrorString(error)
	);
	app._ctx.show(app._messageScreen);
	return false;
}
