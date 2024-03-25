
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
#include "ps1/system.h"

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
	file::FileInfo info;

	if (!_fileProvider.getFileInfo(info, EXTERNAL_DATA_DIR)) {
		if (!_fileProvider.createDirectory(EXTERNAL_DATA_DIR))
			goto _initError;
	}

	int  index;
	char dirPath[32], filePath[32];

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

	auto &region = _storageMenuScreen.getSelectedRegion();
	auto driver  = region.newDriver();

	size_t chipLength    = driver->getChipSize().chipLength;
	size_t sectorLength  = driver->getChipSize().eraseSectorLength;
	size_t sectorsErased = 0;
	size_t bytesWritten  = 0;

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	if (!_file)
		goto _fileError;
	if (!chipLength)
		goto _unsupported;

	rom::DriverError error;
	_systemInfo.flags = 0;

	// TODO: actually implement flash writing

	delete _file;
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_SUCCESS, _storageMenuScreen,
		WSTR("App.romRestoreWorker.success"), sectorsErased, bytesWritten
	);
	_workerStatus.setNextScreen(_messageScreen);
	return true;

_fileError:
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romRestoreWorker.fileError"), path, sectorsErased,
		bytesWritten
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_flashError:
	delete _file;
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romRestoreWorker.flashError"), rom::getErrorString(error),
		sectorsErased, bytesWritten
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_unsupported:
	auto id = region.getJEDECID();
	delete _file;
	delete driver;

	_messageScreen.setMessage(
		MESSAGE_ERROR, _storageMenuScreen,
		WSTR("App.romRestoreWorker.unsupported"),
		(id >>  0) & 0xff,
		(id >>  8) & 0xff,
		(id >> 16) & 0xff,
		(id >> 24) & 0xff
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

	rom::DriverError error;
	_systemInfo.flags = 0;

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

bool App::_startupWorker(void) {
#ifdef NDEBUG
	_workerStatus.setNextScreen(_warningScreen);
#else
	// Skip the warning screen in debug builds.
	_workerStatus.setNextScreen(_buttonMappingScreen);
#endif

	for (int i = 0; i < 2; i++) {
		auto &dev = ide::devices[i];

		_workerStatus.update(i, 4, WSTR("App.startupWorker.initIDE"));

		if (dev.enumerate())
			continue;
		if (!(dev.flags & ide::DEVICE_ATAPI))
			continue;

		// Try to prevent the disc from keeping spinning unnecessarily.
		ide::Packet packet;
		packet.setStartStopUnit(ide::START_STOP_MODE_STOP_DISC);

		dev.atapiPacket(packet);
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

bool App::_systemInfoWorker(void) {
	// This is necessary to ensure the digital I/O ID is read at least once.
	if (!_driver)
		_cartDetectWorker();

	_workerStatus.setNextScreen(_systemInfoScreen);
	_systemInfo.flags = 0;

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

struct LauncherEntry {
public:
	const char *path;
	uintptr_t  loadOffset;
	size_t     length;
};

static const LauncherEntry _LAUNCHERS[]{
	{
		.path       = "launchers/801f8000.psexe",
		.loadOffset = 0x801f8000,
		.length     = 0x8000
	}, {
		.path       = "launchers/803f8000.psexe",
		.loadOffset = 0x803f8000,
		.length     = 0x8000
	}
};

bool App::_executableWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.executableWorker.init"));

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	if (!_file)
		goto _fileOpenError;

	util::ExecutableHeader header;
	size_t                 length;

	length = _file->read(&header, sizeof(header));

	if (length != sizeof(header))
		goto _fileError;

	if (!header.validateMagic()) {
		// If the file is not an executable, check if it is a flash image that
		// contains an executable. Note that the CRC32 is not validated.
		_file->seek(rom::FLASH_EXE_OFFSET);
		length = _file->read(&header, sizeof(header));

		if (length != sizeof(header))
			goto _fileError;
		if (!header.validateMagic())
			goto _fileError;
	}

	delete _file;

	uintptr_t executableEnd, stackTop;

	executableEnd = header.textOffset + header.textLength;
	stackTop      = uintptr_t(header.getStackPtr());

	LOG("ptr=0x%08x, length=0x%x", header.textOffset, header.textLength);

	// Find a launcher that does not overlap the new executable and can thus be
	// used to load it. Note that this implicitly assumes that none of the
	// launchers overlap the main executable.
	for (auto &entry : _LAUNCHERS) {
		uintptr_t launcherEnd = entry.loadOffset + entry.length;

		if (
			!(executableEnd <= entry.loadOffset) &&
			!(launcherEnd <= header.textOffset)
		)
			continue;
		if (
			stackTop &&
			(stackTop >= entry.loadOffset) &&
			(stackTop <= launcherEnd)
		)
			continue;

		// Load the launcher into memory, relocate it to the appropriate
		// location and pass it the path to the executable to be loaded.
		util::Data data;

		if (!_resourceProvider.loadData(data, entry.path))
			continue;

		LOG("using %s", entry.path);
		_workerStatus.update(0, 1, WSTR("App.executableWorker.load"));

		header.copyFrom(data.ptr);
		__builtin_memcpy(
			header.getTextPtr(),
			reinterpret_cast<void *>(
				uintptr_t(data.ptr) + util::EXECUTABLE_BODY_OFFSET
			),
			header.textLength
		);
		data.destroy();

		util::ExecutableLoader loader(
			header, reinterpret_cast<void *>(launcherEnd)
		);
		char arg[128];

		snprintf(
			arg, sizeof(arg), "launcher.drive=%s", _fileProvider.getDriveString()
		);
		loader.addArgument(arg);
		snprintf(arg, sizeof(arg), "launcher.path=%s", path);
		loader.addArgument(arg);

		// All destructors must be invoked manually as we are not returning to
		// main() before starting the new executable.
		_unloadCartData();
		_resourceProvider.close();

		if (_resourceFile)
			delete _resourceFile;

		_fileProvider.close();

		uninstallExceptionHandler();
		loader.run();
	}

	_messageScreen.setMessage(
		MESSAGE_ERROR, _filePickerScreen,
		WSTR("App.executableWorker.addressError"), path, header.textOffset,
		executableEnd - 1, stackTop
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_fileError:
	delete _file;

_fileOpenError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _filePickerScreen,
		WSTR("App.executableWorker.fileError"), path
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

	_unloadCartData();
	_resourceProvider.close();

	if (_resourceFile)
		delete _resourceFile;

	_fileProvider.close();
	_workerStatus.setStatus(WORKER_REBOOT);

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	softReset();

	return true;
}
