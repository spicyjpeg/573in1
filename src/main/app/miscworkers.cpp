
#include <stddef.h>
#include <stdint.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/ide.hpp"
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

		// Spin down all drives by default.
		if (dev.enumerate())
			dev.goIdle();
	}

	_workerStatus.update(2, 4, WSTR("App.startupWorker.initFileIO"));
	_fileIO.initIDE();

	_workerStatus.update(3, 4, WSTR("App.startupWorker.loadResources"));
	if (_fileIO.loadResourceFile(EXTERNAL_DATA_DIR "/resource.zip"))
		_loadResources();

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

bool App::_executableWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.executableWorker.init"));

	const char *path = _fileBrowserScreen.selectedPath;
	auto       _file = _fileIO.vfs.openFile(path, file::READ);

	util::ExecutableHeader header;

	__builtin_memset(header.magic, 0, sizeof(header.magic));

	if (_file) {
		_file->read(&header, sizeof(header));
		_file->close();
		delete _file;
	}

	if (!header.validateMagic()) {
		_messageScreen.setMessage(
			MESSAGE_ERROR, _fileBrowserScreen,
			WSTR("App.executableWorker.fileError"), path
		);
		_workerStatus.setNextScreen(_messageScreen);
		return false;
	}

	auto executableEnd = header.textOffset + header.textLength;
	auto stackTop      = uintptr_t(header.getStackPtr());

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

		// Decompress the launcher into memory and relocate it to the
		// appropriate location.
		util::Data binary;

		if (!_fileIO.resource.loadData(binary, launcher.path))
			continue;

		_workerStatus.update(0, 1, WSTR("App.executableWorker.load"));
		auto launcherHeader = binary.as<const util::ExecutableHeader>();

		util::ExecutableLoader loader(
			launcherHeader->getEntryPoint(), launcherHeader->getInitialGP(),
			reinterpret_cast<void *>(launcherEnd)
		);

		launcherHeader->relocateText(
			binary.as<uint8_t>() + util::EXECUTABLE_BODY_OFFSET
		);
		binary.destroy();

		// Pass the list of LBAs taken up by the executable to the launcher
		// through the command line.
		loader.formatArgument("entry.pc=%08x", header.getEntryPoint());
		loader.formatArgument("entry.gp=%08x", header.getInitialGP());
		loader.formatArgument("entry.sp=%08x", header.getStackPtr());
		loader.formatArgument("load=%08x",     header.getTextPtr());
		loader.formatArgument("drive=%c",      path[3]); // ide#:...

		file::FileFragmentTable fragments;

		_fileIO.vfs.getFileFragments(fragments, path);

		auto fragment = fragments.as<const file::FileFragment>();

		for (size_t i = fragments.getNumFragments(); i; i--, fragment++)
			loader.formatArgument(
				"frag=%llx,%llx", fragment->lba, fragment->length
			);

		fragments.destroy();

		// All destructors must be invoked manually as we are not returning to
		// main() before starting the new executable.
		_unloadCartData();
		_fileIO.close();

		uninstallExceptionHandler();
		loader.run();
	}

	_messageScreen.setMessage(
		MESSAGE_ERROR, _fileBrowserScreen,
		WSTR("App.executableWorker.addressError"), path, header.textOffset,
		executableEnd - 1, stackTop
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_atapiEjectWorker(void) {
	_workerStatus.setNextScreen(_mainMenuScreen, true);
	_workerStatus.update(0, 1, WSTR("App.atapiEjectWorker.eject"));

	for (auto &dev : ide::devices) {
		if (!(dev.flags & ide::DEVICE_ATAPI))
			continue;

		ide::Packet packet;

		packet.setStartStopUnit(ide::START_STOP_MODE_OPEN_TRAY);
		auto error = ide::devices[0].atapiPacket(packet);

		if (error) {
			_messageScreen.setMessage(
				MESSAGE_ERROR, _mainMenuScreen,
				WSTR("App.atapiEjectWorker.ejectError"),
				ide::getErrorString(error)
			);
			_workerStatus.setNextScreen(_messageScreen);
			return false;
		}

		return true;
	}

	_messageScreen.setMessage(
		MESSAGE_ERROR, _mainMenuScreen, WSTR("App.atapiEjectWorker.noDrive")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_rebootWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.rebootWorker.reboot"));

	_unloadCartData();
	_fileIO.close();
	_workerStatus.setStatus(WORKER_REBOOT);

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	softReset();

	return true;
}
