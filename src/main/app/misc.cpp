
#include <stdint.h>
#include "common/defs.hpp"
#include "common/file.hpp"
#include "common/ide.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/misc.hpp"
#include "main/app/modals.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

/* System information screen */

static const util::Hash _SYSTEM_INFO_IDE_HEADERS[]{
	"SystemInfoScreen.ide.header.primary"_h,
	"SystemInfoScreen.ide.header.secondary"_h
};

static const char *const _FILE_SYSTEM_TYPES[]{
	nullptr,
	"FAT12",
	"FAT16",
	"FAT32",
	"exFAT"
};

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr, __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void SystemInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("SystemInfoScreen.title");
	_body   = _bodyText;
	_prompt = STR("SystemInfoScreen.prompt");

	char id1[32], id2[32];
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	// Digital I/O board
	auto &dump = APP->_dump;

	_PRINT(STR("SystemInfoScreen.digitalIO.header"));

	if (dump.flags & cart::DUMP_SYSTEM_ID_OK) {
		dump.systemID.toString(id1);
		dump.systemID.toSerialNumber(id2);

		_PRINT(STR("SystemInfoScreen.digitalIO.info"), id1, id2);
	} else if (dump.flags & cart::DUMP_HAS_SYSTEM_ID) {
		_PRINT(STR("SystemInfoScreen.digitalIO.error"));
	} else {
		_PRINT(STR("SystemInfoScreen.digitalIO.noBoard"));
	}

	_PRINTLN();

	// IDE drives
	for (int i = 0; i < 2; i++) {
		auto &dev = ide::devices[i];

		_PRINT(STRH(_SYSTEM_INFO_IDE_HEADERS[i]));

		if (dev.flags & ide::DEVICE_READY) {
			_PRINT(
				STR("SystemInfoScreen.ide.commonInfo"), dev.model, dev.revision,
				dev.serialNumber
			);

			if (dev.flags & ide::DEVICE_ATAPI) {
				_PRINT(
					STR("SystemInfoScreen.ide.atapiInfo"),
					(dev.flags & ide::DEVICE_HAS_PACKET16) ? 16 : 12
				);
			} else {
				_PRINT(
					STR("SystemInfoScreen.ide.ataInfo"),
					uint64_t(dev.capacity / (0x100000 / ide::ATA_SECTOR_SIZE)),
					(dev.flags & ide::DEVICE_HAS_LBA48) ? 48 : 28
				);

				if (dev.flags & ide::DEVICE_HAS_TRIM)
					_PRINT(STR("SystemInfoScreen.ide.hasTrim"));
				if (dev.flags & ide::DEVICE_HAS_FLUSH)
					_PRINT(STR("SystemInfoScreen.ide.hasFlush"));
			}
		} else {
			_PRINT(STR("SystemInfoScreen.ide.error"));
		}

		_PRINTLN();
	}

	// FAT file system
	auto &fs    = APP->_fileProvider;
	auto fsType = fs.getFileSystemType();

	_PRINT(STR("SystemInfoScreen.fat.header"));

	if (fsType) {
		char label[32];

		fs.getVolumeLabel(label, sizeof(label));
		_PRINT(
			STR("SystemInfoScreen.fat.info"), _FILE_SYSTEM_TYPES[fsType], label,
			fs.getSerialNumber(), fs.getCapacity() / 0x100000,
			fs.getFreeSpace() / 0x100000
		);
	} else {
		_PRINT(STR("SystemInfoScreen.fat.error"));
	}

	_PRINTLN();

	// BIOS ROM
	auto &info = APP->_systemInfo;

	_PRINT(STR("SystemInfoScreen.bios.header"));
	_PRINT(STR("SystemInfoScreen.bios.commonInfo"), info.biosCRC);

	if (rom::sonyKernelHeader.validateMagic()) {
		_PRINT(
			STR("SystemInfoScreen.bios.kernelInfo.sony"),
			rom::sonyKernelHeader.version, rom::sonyKernelHeader.year,
			rom::sonyKernelHeader.month, rom::sonyKernelHeader.day
		);
	} else if (rom::openBIOSHeader.validateMagic()) {
		char buildID[64];

		rom::openBIOSHeader.getBuildID(buildID);
		_PRINT(STR("SystemInfoScreen.bios.kernelInfo.openbios"), buildID);
	} else {
		_PRINT(STR("SystemInfoScreen.bios.kernelInfo.unknown"));
	}

	if (info.shell)
		_PRINT(
			STR("SystemInfoScreen.bios.shellInfo.konami"), info.shell->name,
			info.shell->bootFileName
		);
	else
		_PRINT(STR("SystemInfoScreen.bios.shellInfo.unknown"));

	_PRINTLN();

	// RTC RAM
	_PRINT(STR("SystemInfoScreen.rtc.header"));
	_PRINT(STR("SystemInfoScreen.rtc.info"), info.rtcCRC);

	if (info.flags & SYSTEM_INFO_RTC_BATTERY_LOW)
		_PRINT(STR("SystemInfoScreen.rtc.batteryLow"));

	_PRINTLN();

	// Flash
	_PRINT(STR("SystemInfoScreen.flash.header"));
	_PRINT(
		STR("SystemInfoScreen.flash.info"), info.flash.jedecID & 0xff,
		info.flash.jedecID >> 8, info.flash.crc[0]
	);

	if (info.flash.flags & FLASH_REGION_INFO_BOOTABLE)
		_PRINT(STR("SystemInfoScreen.flash.bootable"));

	_PRINTLN();

	// PCMCIA cards
	for (int i = 0; i < 2; i++) {
		auto card = info.pcmcia[i];

		_PRINT(STR("SystemInfoScreen.pcmcia.header"), i + 1);

		if (card.flags & FLASH_REGION_INFO_PRESENT) {
			_PRINT(
				STR("SystemInfoScreen.pcmcia.info"), card.jedecID & 0xff,
				card.jedecID >> 8, card.crc[0], card.crc[1], card.crc[3]
			);

			if (card.flags & FLASH_REGION_INFO_BOOTABLE)
				_PRINT(STR("SystemInfoScreen.pcmcia.bootable"));
		} else {
			_PRINT(STR("SystemInfoScreen.pcmcia.noCard"));
		}

		_PRINTLN();
	}

	*(--ptr) = 0;
	LOG("remaining=%d", end - ptr);

	TextScreen::show(ctx, goBack);
}

void SystemInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}

/* Executable picker screen */

void ExecPickerScreen::_setPathToParent(void) {
	auto ptr = __builtin_strrchr(_currentPath, '/');

	if (ptr) {
		size_t length = ptr - _currentPath;

		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length] = 0;
	} else {
		selectedPath[0] = 0;
	}
}

void ExecPickerScreen::_setPathToChild(const char *entry) {
	size_t length = __builtin_strlen(_currentPath);

	if (length) {
		__builtin_memcpy(selectedPath, _currentPath, length);
		selectedPath[length++] = '/';
	}

	__builtin_strncpy(
		&selectedPath[length], entry, sizeof(selectedPath) - length
	);
}

void ExecPickerScreen::_unloadDirectory(void) {
	_numFiles       = 0;
	_numDirectories = 0;
	_files.destroy();
	_directories.destroy();
}

const char *ExecPickerScreen::_getItemName(ui::Context &ctx, int index) const {
	static char name[file::MAX_NAME_LENGTH]; // TODO: get rid of this ugly crap

	if (_currentPath[0])
		index--;

	const char *path;
	char       icon;

	if (index < 0) {
		path = STR("ExecPickerScreen.parentDir");
		icon = CH_PARENT_DIR_ICON;
	} else if (index < _numDirectories) {
		auto entries = _directories.as<file::FileInfo>();

		path = entries[index].name;
		icon = CH_DIR_ICON;
	} else {
		auto entries = _files.as<file::FileInfo>();

		path = entries[index - _numDirectories].name;
		icon = CH_FILE_ICON;
	}

	name[0] = icon;
	name[1] = ' ';
	__builtin_strncpy(&name[2], path, sizeof(name) - 2);

	return name;
}

int ExecPickerScreen::loadDirectory(ui::Context &ctx, const char *path) {
	_unloadDirectory();

	// Count the number of files and subfolders in the current directory, so
	// that we can allocate enough space for them.
	auto directory = APP->_fileProvider.openDirectory(path);
	if (!directory)
		return -1;

	file::FileInfo info;

	while (directory->getEntry(info)) {
		if (info.attributes & file::DIRECTORY)
			_numDirectories++;
		else
			_numFiles++;
	}

	delete directory;

	_activeItem = 0;
	_listLength = _numFiles + _numDirectories;
	if (path[0])
		_listLength++;

	LOG("path: %s", path);
	LOG("found %d files, %d folders", _numFiles, _numDirectories);

	if (_numFiles)
		_files.allocate(sizeof(file::FileInfo) * _numFiles);
	if (_numDirectories)
		_directories.allocate(sizeof(file::FileInfo) * _numDirectories);

	auto files       = _files.as<file::FileInfo>();
	auto directories = _directories.as<file::FileInfo>();

	// Iterate over all entries again to populate the newly allocated arrays.
	directory = APP->_fileProvider.openDirectory(path);
	if (!directory)
		return -1;

	while (directory->getEntry(info)) {
		auto ptr = (info.attributes & file::DIRECTORY)
			? (directories++) : (files++);

		__builtin_memcpy(ptr, &info, sizeof(file::FileInfo));
	}

	delete directory;

	__builtin_strncpy(_currentPath, path, sizeof(_currentPath));
	return _numFiles + _numDirectories;
}

void ExecPickerScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ExecPickerScreen.title");
	_prompt     = STR("ExecPickerScreen.prompt");
	_itemPrompt = STR("ExecPickerScreen.itemPrompt");

	//loadDirectory(ctx, "");

	ListScreen::show(ctx, goBack);
}

void ExecPickerScreen::hide(ui::Context &ctx, bool goBack) {
	//_unloadDirectory();

	ListScreen::hide(ctx, goBack);
}

void ExecPickerScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			_unloadDirectory();
			ctx.show(APP->_mainMenuScreen, true, true);
		} else {
			int index = _activeItem;

			if (_currentPath[0])
				index--;

			if (index < _numDirectories) {
				auto entries = _directories.as<file::FileInfo>();

				if (index < 0)
					_setPathToParent();
				else
					_setPathToChild(entries[index].name);

				if (loadDirectory(ctx, selectedPath) < 0) {
					APP->_messageScreen.setMessage(
						MESSAGE_ERROR, *this,
						STR("ExecPickerScreen.subdirError"), selectedPath
					);

					ctx.show(APP->_messageScreen, false, true);
				}
			} else {
				auto entries = _files.as<file::FileInfo>();

				_setPathToChild(entries[index - _numDirectories].name);

				APP->_setupWorker(&App::_executableWorker);
				ctx.show(APP->_workerStatusScreen, false, true);
			}
		}
	}
}

/* Misc. screens */

struct Resolution {
public:
	util::Hash name;
	int        width, height;
	bool       forceInterlace;
};

static const Resolution _RESOLUTIONS[]{
	{
		.name           = "ResolutionScreen.320x240p"_h,
		.width          = 320,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.320x240i"_h,
		.width          = 320,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.368x240p"_h,
		.width          = 368,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.368x240i"_h,
		.width          = 368,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.512x240p"_h,
		.width          = 512,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.512x240i"_h,
		.width          = 512,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.640x240p"_h,
		.width          = 640,
		.height         = 240,
		.forceInterlace = false
	}, {
		.name           = "ResolutionScreen.640x240i"_h,
		.width          = 640,
		.height         = 240,
		.forceInterlace = true
	}, {
		.name           = "ResolutionScreen.640x480i"_h,
		.width          = 640,
		.height         = 480,
		.forceInterlace = true
	}
};

const char *ResolutionScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_RESOLUTIONS[index].name);
}

void ResolutionScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ResolutionScreen.title");
	_prompt     = STR("ResolutionScreen.prompt");
	_itemPrompt = STR("ResolutionScreen.itemPrompt");

	_listLength = util::countOf(_RESOLUTIONS);

	ListScreen::show(ctx, goBack);
}

void ResolutionScreen::update(ui::Context &ctx) {
	auto &res = _RESOLUTIONS[_activeItem];

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (!ctx.buttons.held(ui::BTN_LEFT) && !ctx.buttons.held(ui::BTN_RIGHT))			
			ctx.gpuCtx.setResolution(
				GP1_MODE_NTSC, res.width, res.height, res.forceInterlace
			);

		ctx.show(APP->_mainMenuScreen, true, true);
	}
}

void AboutScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("AboutScreen.title");
	_prompt = STR("AboutScreen.prompt");

	APP->_resourceProvider.loadData(_text, "assets/about.txt");

	auto ptr = reinterpret_cast<char *>(_text.ptr);
	_body    = ptr;

	// Replace single newlines with spaces to reflow the text, unless the line
	// preceding the newline ends with a space. The last character is also cut
	// off and replaced with a null terminator.
	for (size_t i = _text.length - 1; i; i--, ptr++) {
		if (*ptr != '\n')
			continue;
		if (__builtin_isspace(ptr[-1]))
			continue;

		if (ptr[1] == '\n')
			i--, ptr++;
		else
			*ptr = ' ';
	}

	*ptr = 0;

	TextScreen::show(ctx, goBack);
}

void AboutScreen::hide(ui::Context &ctx, bool goBack) {
	_body = nullptr;
	_text.destroy();

	TextScreen::hide(ctx, goBack);
}

void AboutScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_mainMenuScreen, true, true);
}
