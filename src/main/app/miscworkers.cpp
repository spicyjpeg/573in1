
#include <stddef.h>
#include <stdint.h>
#include "common/file/file.hpp"
#include "common/defs.hpp"
#include "common/ide.hpp"
#include "common/idedefs.hpp"
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "ps1/system.h"

static const rom::Region *const _AUTOBOOT_REGIONS[]{
	&rom::pcmcia[1],
	&rom::pcmcia[0],
	&rom::flash
};

static const char *const _AUTOBOOT_PATHS[][2]{
	{ "cdrom:/noboot.txt", "cdrom:/psx.exe" },
	{ "cdrom:/noboot.txt", "cdrom:/qsy.dxd" },
	{ "cdrom:/noboot.txt", "cdrom:/ssw.bxf" },
	{ "cdrom:/noboot.txt", "cdrom:/tsv.axg" },
	{ "cdrom:/noboot.txt", "cdrom:/gse.nxx" },
	{ "cdrom:/noboot.txt", "cdrom:/nse.gxx" },
	{ "hdd:/noboot.txt",   "hdd:/psx.exe"   }
};

bool App::_ideInitWorker(void) {
	for (size_t i = 0; i < util::countOf(ide::devices); i++) {
		auto &dev = ide::devices[i];

		if (dev.flags & ide::DEVICE_READY)
			continue;

		_workerStatus.update(
			i, util::countOf(ide::devices), WSTR("App.ideInitWorker.init")
		);

		auto error = dev.enumerate();

		LOG_APP("drive %d: %s", i, ide::getErrorString(error));
	}

	_fileInitWorker();

#ifdef ENABLE_AUTOBOOT
	// Only try to autoboot if DIP switch 1 is on.
	if (io::getDIPSwitch(0)) {
		_workerStatus.update(3, 4, WSTR("App.ideInitWorker.autoboot"));

		if (io::getDIPSwitch(3)) {
			for (auto region : _AUTOBOOT_REGIONS) {
				if (!region->getBootExecutableHeader())
					continue;

				_storageActionsScreen.selectedRegion = region;

				_workerStatus.setNextScreen(_autobootScreen);
				break;
			}
		}

		for (auto path : _AUTOBOOT_PATHS) {
			file::FileInfo info;

			if (_fileIO.vfs.getFileInfo(info, path[0]))
				continue;
			if (!_fileIO.vfs.getFileInfo(info, path[1]))
				continue;

			_storageActionsScreen.selectedRegion = nullptr;

			__builtin_strncpy(
				_fileBrowserScreen.selectedPath, path[1],
				sizeof(_fileBrowserScreen.selectedPath)
			);
			_workerStatus.setNextScreen(_autobootScreen);
			break;
		}
	}
#endif

	return true;
}

bool App::_fileInitWorker(void) {
	_workerStatus.update(0, 3, WSTR("App.fileInitWorker.unmount"));
	_fileIO.closeResourceFile();
	_fileIO.closeIDE();

	_workerStatus.update(1, 3, WSTR("App.fileInitWorker.mount"));
	_fileIO.initIDE();

	_workerStatus.update(2, 3, WSTR("App.fileInitWorker.loadResources"));
	if (_fileIO.loadResourceFile(EXTERNAL_DATA_DIR "/resource.zip"))
		_loadResources();

	return true;
}

struct Launcher {
public:
	const char *path;
	uintptr_t  loadOffset;
	size_t     length;
};

// When loading an executable, a launcher that does not overlap the target
// binary is picked from the list below. Note that this implicitly assumes that
// none of the launchers overlap the main binary.
static const Launcher _LAUNCHERS[]{
	{
		.path       = "binaries/launcher801fd000.psexe",
		.loadOffset = 0x801fd000,
		.length     = 0x3000
	}, {
		.path       = "binaries/launcher803fd000.psexe",
		.loadOffset = 0x803fd000,
		.length     = 0x3000
	}
};

bool App::_executableWorker(void) {
	_workerStatus.update(0, 2, WSTR("App.executableWorker.init"));

	auto       region = _storageActionsScreen.selectedRegion;
	const char *path  = _fileBrowserScreen.selectedPath;

	int device;
	util::ExecutableHeader header;

	if (region) {
		device = region->bank;

		region->read(&header, rom::FLASH_EXECUTABLE_OFFSET, sizeof(header));
	} else {
		__builtin_memset(header.magic, 0, sizeof(header.magic));

		auto _file = _fileIO.vfs.openFile(path, file::READ);
		device     = -(path[3] - '0' + 1); // ide#: -> -1 or -2

		if (_file) {
			_file->read(&header, sizeof(header));
			_file->close();
			delete _file;
		}

		if (!header.validateMagic()) {
			_messageScreen.setMessage(
				MESSAGE_ERROR, WSTR("App.executableWorker.fileError"), path
			);
			_workerStatus.setNextScreen(_messageScreen);
			return false;
		}
	}

	auto executableEnd = header.textOffset + header.textLength;
	auto stackTop      = uintptr_t(header.getStackPtr());

	LOG_APP(".text: 0x%08x-0x%08x", header.textOffset, executableEnd - 1);

	// Find a launcher that does not overlap the new executable and can thus be
	// used to load it.
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

		_workerStatus.update(1, 2, WSTR("App.executableWorker.load"));
		auto launcherHeader = binary.as<const util::ExecutableHeader>();

		util::ExecutableLoader loader(
			launcherHeader->getEntryPoint(), launcherHeader->getInitialGP(),
			reinterpret_cast<void *>(launcherEnd)
		);

		launcherHeader->relocateText(
			binary.as<uint8_t>() + util::EXECUTABLE_BODY_OFFSET
		);
		binary.destroy();

		loader.formatArgument("entry.pc=%08x", header.getEntryPoint());
		loader.formatArgument("entry.gp=%08x", header.getInitialGP());
		loader.formatArgument("entry.sp=%08x", header.getStackPtr());
		loader.formatArgument("load=%08x",     header.getTextPtr());
		loader.formatArgument("device=%d",     device);

		if (region) {
			uintptr_t ptr = 0
				+ region->ptr
				+ rom::FLASH_EXECUTABLE_OFFSET
				+ util::EXECUTABLE_BODY_OFFSET;

			loader.formatArgument("frag=%x,%x", ptr, header.textLength);
		} else {
			// Pass the list of LBAs taken up by the executable to the launcher
			// through the command line.
			file::FileFragmentTable fragments;

			_fileIO.vfs.getFileFragments(fragments, path);

			auto fragment = fragments.as<const file::FileFragment>();
			auto count    = fragments.getNumFragments();

			for (size_t i = count; i; i--, fragment++) {
				if (loader.formatArgument(
					"frag=%llx,%llx", fragment->lba, fragment->length
				))
					continue;

				fragments.destroy();

				_messageScreen.setMessage(
					MESSAGE_ERROR, WSTR("App.executableWorker.fragmentError"),
					path, count, i
				);
				_workerStatus.setNextScreen(_messageScreen);
				return false;
			}

			fragments.destroy();
		}

		// All destructors must be invoked manually as we are not returning to
		// main() before starting the new executable.
		_unloadCartData();
		_fileIO.closeResourceFile();
		_fileIO.closeIDE();

		uninstallExceptionHandler();
		io::clearWatchdog();

		loader.run();
	}

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.executableWorker.addressError"),
		header.textOffset, executableEnd - 1, stackTop
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_atapiEjectWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.atapiEjectWorker.eject"));

	for (auto &dev : ide::devices) {
		if (!(dev.flags & ide::DEVICE_ATAPI))
			continue;

		auto error =
			ide::devices[0].startStopUnit(ide::START_STOP_MODE_OPEN_TRAY);

		if (error) {
			_messageScreen.setMessage(
				MESSAGE_ERROR, WSTR("App.atapiEjectWorker.ejectError"),
				ide::getErrorString(error)
			);
			_workerStatus.setNextScreen(_messageScreen);
			return false;
		}

		return true;
	}

	_messageScreen.setMessage(
		MESSAGE_ERROR, WSTR("App.atapiEjectWorker.noDrive")
	);
	_workerStatus.setNextScreen(_messageScreen);
	return false;
}

bool App::_rebootWorker(void) {
	_workerStatus.update(0, 1, WSTR("App.rebootWorker.reboot"));

	_unloadCartData();
	_fileIO.closeResourceFile();
	_fileIO.closeIDE();
	_workerStatus.setStatus(WORKER_REBOOT);

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	softReset();

	return true;
}
