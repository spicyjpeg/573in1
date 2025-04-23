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
#include "common/blkdev/device.hpp"
#include "common/fs/file.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "common/rom.hpp"
#include "main/app/app.hpp"
#include "main/workers/miscworkers.hpp"
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

bool startupWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.startupWorker.ideInit"));

	sys573::resetIDEDevices();
	app._fileIO.init();

	fileInitWorker(app);

#ifdef ENABLE_AUTOBOOT
	// Only try to autoboot if DIP switch 1 is on.
	if (sys573::getDIPSwitch(0)) {
		app._workerStatusScreen.setMessage(WSTR("App.ideInitWorker.autoboot"));

		if (sys573::getDIPSwitch(3)) {
			for (auto region : _AUTOBOOT_REGIONS) {
				if (!region->getBootExecutableHeader())
					continue;

				app._nvramActionsScreen.selectedRegion = region;

				app._ctx.show(app._autobootScreen);
				return true;
			}
		}

		for (auto path : _AUTOBOOT_PATHS) {
			fs::FileInfo info;

			if (app._fileIO.getFileInfo(info, path[0]))
				continue;
			if (!app._fileIO.getFileInfo(info, path[1]))
				continue;

			app._nvramActionsScreen.selectedRegion = nullptr;
			__builtin_strncpy(
				app._fileBrowserScreen.selectedPath,
				path[1],
				sizeof(app._fileBrowserScreen.selectedPath)
			);

			app._ctx.show(app._autobootScreen);
			return true;
		}
	}
#endif

	app._ctx.show(app._warningScreen);
	return true;
}

bool fileInitWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.fileInitWorker.unmount"));
	app._fileIO.unmountAll();

	app._workerStatusScreen.setMessage(WSTR("App.fileInitWorker.mount"));
	app._fileIO.mountIDE();
	app._fileIO.mountPS1CDROM();
#if 0
	app._fileIO.mountMemoryCards();
#endif

	app._workerStatusScreen.setMessage(WSTR("App.fileInitWorker.loadResources"));
	if (app._fileIO.loadResourceFile(EXTERNAL_DATA_DIR "/resource.pkg"))
		app._loadResources();

	app._ctx.show(app._filePickerScreen);
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
		.path       = "res:/binaries/launcher801fc000.psexe",
		.loadOffset = 0x801fc000,
		.length     = 0x4000
	}, {
		.path       = "res:/binaries/launcher803fc000.psexe",
		.loadOffset = 0x803fc000,
		.length     = 0x4000
	}
};

static const char *const _DEVICE_TYPES[]{
	"none",  // blkdev::NONE
	"ata",   // blkdev::ATA
	"atapi", // blkdev::ATAPI
	"cdrom"  // blkdev::PS1_CDROM
};

bool executableWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.executableWorker.init"));

	auto       region = app._nvramActionsScreen.selectedRegion;
	const char *path  = app._fileBrowserScreen.selectedPath;

	const char *deviceType;
	int        deviceIndex;

	util::ExecutableHeader header;

	if (region) {
		region->read(&header, rom::FLASH_EXECUTABLE_OFFSET, sizeof(header));

		deviceType  = "flash";
		deviceIndex = region->bank;
	} else {
		util::clear(header.magic);

		auto file = app._fileIO.openFile(path, fs::READ);

		if (file) {
			file->read(&header, sizeof(header));
			file->close();
			delete file;
		}

		if (!header.validateMagic()) {
			app._messageScreen.setMessage(
				MESSAGE_ERROR,
				WSTR("App.executableWorker.fileError"),
				path
			);
			app._ctx.show(app._messageScreen);
			return false;
		}

		auto mp     = app._fileIO.getMountPoint(path);
		deviceType  = _DEVICE_TYPES[mp->dev->type];
		deviceIndex = mp->dev->getDeviceIndex();
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

		if (!app._fileIO.loadData(binary, launcher.path))
			continue;

		app._workerStatusScreen.setMessage(WSTR("App.executableWorker.load"));
		auto launcherHeader = binary.as<const util::ExecutableHeader>();

		util::ExecutableLoader loader(
			launcherHeader->getEntryPoint(), launcherHeader->getInitialGP(),
			reinterpret_cast<void *>(launcherEnd)
		);

		launcherHeader->relocateText(
			binary.as<uint8_t>() + util::EXECUTABLE_BODY_OFFSET
		);
		binary.destroy();

		loader.formatArgument("load=%08x",      header.getTextPtr());
		loader.formatArgument("entry.pc=%08x",  header.getEntryPoint());
		loader.formatArgument("entry.gp=%08x",  header.getInitialGP());
		loader.formatArgument("entry.sp=%08x",  header.getStackPtr());
		loader.formatArgument("device.type=%s", deviceType);
		loader.formatArgument("device.id=%d",   deviceIndex);

		if (region) {
			uintptr_t ptr = 0
				+ region->ptr
				+ rom::FLASH_EXECUTABLE_OFFSET
				+ util::EXECUTABLE_BODY_OFFSET;

			loader.formatArgument("frag=%x,%x", ptr, header.textLength);
		} else {
			// Pass the list of LBAs taken up by the executable to the launcher
			// through the command line.
			fs::FileFragmentTable fragments;

			app._fileIO.getFileFragments(fragments, path);

			auto fragment = fragments.as<const fs::FileFragment>();
			auto count    = fragments.getNumFragments();

			for (size_t i = count; i; i--, fragment++) {
				if (loader.formatArgument(
					"frag=%llx,%llx",
					fragment->lba,
					fragment->length
				))
					continue;

				app._messageScreen.setMessage(
					MESSAGE_ERROR,
					WSTR("App.executableWorker.fragmentError"),
					path,
					count,
					i
				);
				app._ctx.show(app._messageScreen);
				return false;
			}

			fragments.destroy();
		}

		// All destructors must be invoked manually as we are not returning to
		// main() before starting the new executable.
		app._unloadCartData();
		app._fileIO.unmountAll();

		LOG_APP("jumping to launcher");
		uninstallExceptionHandler();
		sys573::clearWatchdog();

		loader.run();
	}

	app._messageScreen.setMessage(
		MESSAGE_ERROR,
		WSTR("App.executableWorker.addressError"),
		header.textOffset,
		executableEnd - 1,
		stackTop
	);
	app._ctx.show(app._messageScreen);
	return false;
}

bool atapiEjectWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.atapiEjectWorker.eject"));

	auto mp = app._fileIO.getMountPoint("cdrom:");

	if (mp) {
		if (!mp->dev)
			mp = nullptr;
		if (mp->dev->type != blkdev::ATAPI)
			mp = nullptr;
	}
	if (!mp) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.atapiEjectWorker.noDrive")
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	auto error = blkdev::DISC_CHANGED;

	while (error == blkdev::DISC_CHANGED)
		error = mp->dev->eject();

	if (error) {
		app._messageScreen.setMessage(
			MESSAGE_ERROR,
			WSTR("App.atapiEjectWorker.ejectError"),
			blkdev::getErrorString(error)
		);
		app._ctx.show(app._messageScreen);
		return false;
	}

	app._ctx.show(app._mainMenuScreen, true);
	return true;
}

bool rebootWorker(App &app) {
	app._workerStatusScreen.setMessage(WSTR("App.rebootWorker.reboot"));

	app._unloadCartData();
	app._fileIO.unmountAll();

	app._workerFlags |= WORKER_REBOOT;
	flushWriteQueue();

	// Fall back to a soft reboot if the watchdog fails to reset the system.
	delayMicroseconds(2000000);
	LOG_APP("WD reset failed, soft rebooting");
	uninstallExceptionHandler();
	softReset();

	return true;
}
