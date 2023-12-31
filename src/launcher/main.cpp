
#include <stdio.h>
#include <stdlib.h>
#include "common/ide.hpp"
#include "common/io.hpp"
#include "common/util.hpp"
#include "ps1/system.h"
#include "vendor/ff.h"

extern "C" uint8_t _textStart[];

class Settings {
public:
	int        baudRate, argCount;
	const char *drive, *path;
	const char *args[util::MAX_EXECUTABLE_ARGS];

	inline Settings(void)
	: baudRate(0), argCount(0), drive(nullptr), path(nullptr) {}

	bool parse(const char *arg);
};

bool Settings::parse(const char *arg) {
	if (!arg)
		return false;

	switch (util::hash(arg, '=')) {
		case "boot.rom"_h:
			//LOG("boot.rom=%s", &arg[9]);
			return true;

		case "boot.from"_h:
			//LOG("boot.from=%s", &arg[10]);
			return true;

		case "console"_h:
			baudRate = int(strtol(&arg[8], nullptr, 0));
			return true;

		case "launcher.drive"_h:
			drive = &arg[15];
			return true;

		case "launcher.path"_h:
			path = &arg[14];
			return true;

		case "launcher.arg"_h:
			if (argCount >= int(util::countOf(args)))
				return false;

			args[argCount++] = &arg[13];
			return true;

		default:
			return false;
	}
}

class Launcher {
private:
	Settings &_settings;

	// Using the FatFs API directly (rather than through file::FATProvider)
	// yields a smaller executable as it avoids pulling in malloc.
	FATFS _fs;
	FIL   _file;

	util::ExecutableHeader _header;

public:
	inline Launcher(Settings &settings)
	: _settings(settings) {
		_fs.fs_type  = 0;
		_file.obj.fs = nullptr;
	}
	inline ~Launcher(void) {
		exit();
	}

	bool openFile(void);
	bool readHeader(void);
	bool readBody(void);
	void exit(void);
	[[noreturn]] void run(void);
};

bool Launcher::openFile(void) {
	if (!_settings.drive || !_settings.path) {
		LOG("required arguments missing");
		return false;
	}

	// As long as it works...
	int drive = _settings.drive[0] - '0';

	if (drive < 0 || drive > 1) {
		LOG("invalid drive ID");
		return false;
	}
	if (ide::devices[drive].enumerate()) {
		LOG("IDE init failed, drive=%s", _settings.drive);
		return false;
	}
	if (f_mount(&_fs, _settings.drive, 1)) {
		LOG("FAT mount failed, drive=%s", _settings.drive);
		return false;
	}

	f_chdrive(_settings.drive);

	if (f_open(&_file, _settings.path, FA_READ)) {
		LOG("open failed, path=%s", _settings.path);
		return false;
	}

	return true;
}

bool Launcher::readHeader(void) {
	size_t length;

	if (f_read(&_file, &_header, sizeof(_header), &length)) {
		LOG("header read failed, path=%s", _settings.path);
		return false;
	}
	if (length != sizeof(_header)) {
		LOG("invalid header length %d", length);
		return false;
	}

	auto ptr = reinterpret_cast<void *>(_header.textOffset);

	if (ptr >= _textStart) {
		LOG("executable overlaps launcher");
		return false;
	}

	LOG("ptr=0x%08x, length=0x%08x", ptr, _header.textLength);
	return true;
}

bool Launcher::readBody(void) {
	auto   ptr = reinterpret_cast<void *>(_header.textOffset);
	size_t length;

	if (f_lseek(&_file, util::EXECUTABLE_BODY_OFFSET)) {
		LOG("seek to body failed, path=%s", _settings.path);
		return false;
	}
	if (f_read(&_file, ptr, _header.textLength, &length)) {
		LOG("body read failed, path=%s", _settings.path);
		return false;
	}
	if (length != _header.textLength) {
		LOG("invalid body length %d", length);
		return false;
	}

	return true;
}

void Launcher::exit(void) {
	if (_file.obj.fs)
		f_close(&_file);
	if (_fs.fs_type)
		f_unmount(_settings.drive);

	uninstallExceptionHandler();
}

[[noreturn]] void Launcher::run(void) {
	util::ExecutableLoader loader(_header, _textStart - 16);

	for (int i = 0; i < _settings.argCount; i++)
		loader.addArgument(_settings.args[i]);

	exit();
	loader.run();
}

int main(int argc, const char **argv) {
	installExceptionHandler();
	io::init();

	setInterruptHandler([](void *dummy) {
		if (acknowledgeInterrupt(IRQ_VSYNC))
			io::clearWatchdog();
	}, nullptr);

	IRQ_MASK = 1 << IRQ_VSYNC;
	enableInterrupts();

	Settings settings;
	Launcher launcher(settings);

#ifndef NDEBUG
	// Enable serial port logging by default in debug builds.
	settings.baudRate = 115200;
#endif

	for (; argc > 0; argc--)
		settings.parse(*(argv++));

	util::logger.setupSyslog(settings.baudRate);

	if (!launcher.openFile())
		return 1;
	if (!launcher.readHeader())
		return 2;
	if (!launcher.readBody())
		return 3;

	launcher.run();
	return 0;
}
