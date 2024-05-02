
#include <stddef.h>
#include <stdint.h>
#include "common/ide.hpp"
#include "common/util.hpp"
#include "launcher/launcher.hpp"
#include "vendor/ff.h"

LauncherError ExecutableLauncher::openFile(void) {
#if 0
	if (!args.drive || !args.path) {
		LOG("required arguments missing");
		return INVALID_ARGS;
	}
#else
	if (!args.drive)
		args.drive = "1:";
	if (!args.path)
		args.path = "psx.exe";
#endif

	// The drive index is always a single digit, so there is no need to pull in
	// strtol() here.
	int drive = args.drive[0] - '0';

	if (drive < 0 || drive > 1) {
		LOG("invalid drive ID: %d", drive);
		return INVALID_ARGS;
	}
	if (ide::devices[drive].enumerate()) {
		LOG("IDE init failed, drive=%s", args.drive);
		return DRIVE_ERROR;
	}

	auto error = f_mount(&_fs, args.drive, 1);

	if (error) {
		LOG("FAT mount failed, code=%d, drive=%s", error, args.drive);
		return FAT_ERROR;
	}

	f_chdrive(args.drive);
	error = f_open(&_file, args.path, FA_READ);

	if (error) {
		LOG("open failed, code=%d, path=%s", error, args.path);
		return FILE_ERROR;
	}

	return NO_ERROR;
}

LauncherError ExecutableLauncher::parseHeader(uint64_t offset) {
	LOG("parsing header, offset=0x%lx", offset);

	auto error = f_lseek(&_file, offset);

	if (error) {
		LOG("seek to header failed, code=%d, path=%s", error, args.path);
		return FILE_ERROR;
	}

	size_t length;
	error = f_read(&_file, &_header, sizeof(_header), &length);

	if (error) {
		LOG("header read failed, code=%d, path=%s", error, args.path);
		return FILE_ERROR;
	}
	if (length != sizeof(_header)) {
		LOG("invalid header length: %d", length);
		return INVALID_FILE;
	}
	if (!_header.validateMagic()) {
		LOG("invalid executable magic");
		return INVALID_FILE;
	}

	_bodyOffset = offset + util::EXECUTABLE_BODY_OFFSET;
	return NO_ERROR;
}

LauncherError ExecutableLauncher::loadBody(void) {
	auto error = f_lseek(&_file, _bodyOffset);

	if (error) {
		LOG("seek to body failed, code=%d, path=%s", error, args.path);
		return FILE_ERROR;
	}

	size_t length;
	error = f_read(&_file, _header.getTextPtr(), _header.textLength, &length);

	if (error) {
		LOG("body read failed, code=%d, path=%s", error, args.path);
		return FILE_ERROR;
	}
	if (length != _header.textLength) {
		LOG("invalid body length: %d", length);
		return INVALID_FILE;
	}

	return NO_ERROR;
}

void ExecutableLauncher::closeFile(void) {
	if (_file.obj.fs)
		f_close(&_file);
	if (_fs.fs_type)
		f_unmount(args.drive);
}

extern "C" uint8_t _textStart[];

[[noreturn]] void ExecutableLauncher::run(void) {
	util::ExecutableLoader loader(_header, _textStart - 16);

	for (int i = 0; i < args.argCount; i++)
		loader.copyArgument(args.executableArgs[i]);

	loader.run();
}
