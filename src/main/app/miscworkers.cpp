
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/ide.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "ps1/system.h"

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

struct Launcher {
public:
	const char *path;
	uintptr_t  loadOffset;
	size_t     length;
};

static const Launcher _LAUNCHERS[]{
	{
		.path       = "binaries/launcher801f8000.psexe",
		.loadOffset = 0x801f8000,
		.length     = 0x8000
	}, {
		.path       = "binaries/launcher803f8000.psexe",
		.loadOffset = 0x803f8000,
		.length     = 0x8000
	}
};

static const uint32_t _EXECUTABLE_OFFSETS[]{
	0,
	rom::FLASH_EXECUTABLE_OFFSET,
	util::EXECUTABLE_BODY_OFFSET
};

bool App::_executableWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.executableWorker.init"));

	const char *path = _filePickerScreen.selectedPath;
	auto       _file = _fileProvider.openFile(path, file::READ);

	if (!_file)
		goto _fileOpenError;

	util::ExecutableHeader header;

	// Check for the presence of an executable at several different offsets
	// within the file before giving up.
	for (auto offset : _EXECUTABLE_OFFSETS) {
		_file->seek(offset);
		size_t length = _file->read(&header, sizeof(header));

		if (length != sizeof(header))
			break;
		if (header.validateMagic())
			goto _validFile;
	}

	_file->close();
	delete _file;

_fileOpenError:
	_messageScreen.setMessage(
		MESSAGE_ERROR, _filePickerScreen,
		WSTR("App.executableWorker.fileError"), path
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;

_validFile:
	_file->close();
	delete _file;

	uintptr_t executableEnd, stackTop;

	executableEnd = header.textOffset + header.textLength;
	stackTop      = uintptr_t(header.getStackPtr());

	LOG("ptr=0x%08x, length=0x%x", header.textOffset, header.textLength);

	// Find a launcher that does not overlap the new executable and can thus be
	// used to load it. Note that this implicitly assumes that none of the
	// launchers overlap the main executable.
	for (auto &launcher : _LAUNCHERS) {
		uintptr_t launcherEnd = launcher.loadOffset + launcher.length;

		if (
			!(executableEnd <= launcher.loadOffset) &&
			!(launcherEnd <= header.textOffset)
		)
			continue;
		if (
			stackTop &&
			(stackTop >= launcher.loadOffset) &&
			(stackTop <= launcherEnd)
		)
			continue;

		// Load the launcher into memory, relocate it to the appropriate
		// location and pass it the path to the executable to be loaded.
		util::Data data;

		if (!_resourceProvider.loadData(data, launcher.path))
			continue;

		LOG("using %s", launcher.path);
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

		// All destructors must be invoked manually as we are not returning to
		// main() before starting the new executable.
		_unloadCartData();
		_resourceProvider.close();

		if (_resourceFile) {
			_resourceFile->close();
			delete _resourceFile;
		}

		_fileProvider.close();

		util::ExecutableLoader loader(
			header, reinterpret_cast<void *>(launcherEnd)
		);
		char arg[128];

		snprintf(
			arg, sizeof(arg), "launcher.drive=%s", _fileProvider.getDriveString()
		);
		loader.copyArgument(arg);
		snprintf(arg, sizeof(arg), "launcher.path=%s", path);
		loader.copyArgument(arg);

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

	if (_resourceFile) {
		_resourceFile->close();
		delete _resourceFile;
	}

	_fileProvider.close();
	_workerStatus.setStatus(WORKER_REBOOT);

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	softReset();

	return true;
}
