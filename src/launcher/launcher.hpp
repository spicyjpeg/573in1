
#pragma once

#include <stdint.h>
#include "common/args.hpp"
#include "common/util.hpp"
#include "vendor/ff.h"

enum LauncherError {
	NO_ERROR     = 0,
	INVALID_ARGS = 1,
	DRIVE_ERROR  = 2,
	FAT_ERROR    = 3,
	FILE_ERROR   = 4,
	INVALID_FILE = 5
};

class ExecutableLauncher {
private:
	// Using the FatFs API directly (rather than through file::FATProvider)
	// yields a smaller executable as it avoids pulling in malloc.
	FATFS _fs;
	FIL   _file;

	util::ExecutableHeader _header;
	uint64_t               _bodyOffset;

public:
	args::ExecutableLauncherArgs args;

	inline ExecutableLauncher(void) {
		_fs.fs_type  = 0;
		_file.obj.fs = nullptr;
	}

	LauncherError openFile(void);
	LauncherError parseHeader(uint64_t offset = 0);
	LauncherError loadBody(void);
	void closeFile(void);

	[[noreturn]] void run(void);
};
