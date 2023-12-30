
#include <stdint.h>
#include "common/ide.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/main.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

/* Main menu screens */

static constexpr int WARNING_COOLDOWN = 10;

void WarningScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("WarningScreen.title");
	_body       = STR("WarningScreen.body");
	_buttons[0] = _buttonText;

	_locked     = true;
	_numButtons = 1;

	_cooldownTimer = ctx.time + ctx.gpuCtx.refreshRate * WARNING_COOLDOWN;

	MessageBoxScreen::show(ctx, goBack);

	ctx.buttons.buttonMap = ui::MAP_SINGLE_BUTTON;
	ctx.buttons.reset();
}

void WarningScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	int time = _cooldownTimer - ctx.time;
	_locked  = (time > 0);

	if (_locked) {
		time = (time / ctx.gpuCtx.refreshRate) + 1;

		sprintf(_buttonText, STR("WarningScreen.cooldown"), time);
		return;
	}

	_buttons[0] = STR("WarningScreen.ok");

	if (ctx.buttons.pressed(ui::BTN_RIGHT) || ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_buttonMappingScreen, false, true);
}

static const util::Hash _MAPPING_NAMES[]{
	"ButtonMappingScreen.joystick"_h,
	"ButtonMappingScreen.ddrCab"_h,
	"ButtonMappingScreen.ddrSoloCab"_h,
	"ButtonMappingScreen.dmCab"_h,
	"ButtonMappingScreen.dmxCab"_h
};

const char *ButtonMappingScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_MAPPING_NAMES[index]);
}

void ButtonMappingScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ButtonMappingScreen.title");
	_prompt     = STR("ButtonMappingScreen.prompt");
	_itemPrompt = STR("ButtonMappingScreen.itemPrompt");

	_listLength = ui::NUM_BUTTON_MAPS - 1;

	ListScreen::show(ctx, goBack);

	ctx.buttons.buttonMap = ui::MAP_SINGLE_BUTTON;
	ctx.buttons.reset();
}

void ButtonMappingScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		ctx.buttons.buttonMap = ui::ButtonMap(_activeItem);
		ctx.buttons.reset();

		ctx.show(APP->_mainMenuScreen, false, true);
	}
}

struct MenuEntry {
public:
	util::Hash name, prompt;
	void       (MainMenuScreen::*target)(ui::Context &ctx);
};

static const MenuEntry _MENU_ENTRIES[]{
	{
#ifdef ENABLE_CART_MENU
		.name   = "MainMenuScreen.cartInfo.name"_h,
		.prompt = "MainMenuScreen.cartInfo.prompt"_h,
		.target = &MainMenuScreen::cartInfo
	}, {
#endif
		.name   = "MainMenuScreen.dump.name"_h,
		.prompt = "MainMenuScreen.dump.prompt"_h,
		.target = &MainMenuScreen::dump
	}, {
#if 0
		.name   = "MainMenuScreen.restore.name"_h,
		.prompt = "MainMenuScreen.restore.prompt"_h,
		.target = &MainMenuScreen::restore
	}, {
#endif
		.name   = "MainMenuScreen.systemInfo.name"_h,
		.prompt = "MainMenuScreen.systemInfo.prompt"_h,
		.target = &MainMenuScreen::systemInfo
	}, {
		.name   = "MainMenuScreen.setResolution.name"_h,
		.prompt = "MainMenuScreen.setResolution.prompt"_h,
		.target = &MainMenuScreen::setResolution
	}, {
		.name   = "MainMenuScreen.about.name"_h,
		.prompt = "MainMenuScreen.about.prompt"_h,
		.target = &MainMenuScreen::about
	}, {
#if 0
		.name   = "MainMenuScreen.launchEXE.name"_h,
		.prompt = "MainMenuScreen.launchEXE.prompt"_h,
		.target = &MainMenuScreen::launchEXE
	}, {
#endif
		.name   = "MainMenuScreen.ejectCD.name"_h,
		.prompt = "MainMenuScreen.ejectCD.prompt"_h,
		.target = &MainMenuScreen::ejectCD
	}, {
		.name   = "MainMenuScreen.reboot.name"_h,
		.prompt = "MainMenuScreen.reboot.prompt"_h,
		.target = &MainMenuScreen::reboot
	}
};

const char *MainMenuScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_MENU_ENTRIES[index].name);
}

void MainMenuScreen::cartInfo(ui::Context &ctx) {
	if (APP->_driver) {
		ctx.show(APP->_cartInfoScreen, false, true);
	} else {
		APP->_setupWorker(&App::_cartDetectWorker);
		ctx.show(APP->_workerStatusScreen, false, true);
	}
}

void MainMenuScreen::dump(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_romDumpWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("MainMenuScreen.dump.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void MainMenuScreen::restore(ui::Context &ctx) {
	//ctx.show(APP->_restoreMenuScreen, false, true);
}

void MainMenuScreen::systemInfo(ui::Context &ctx) {
	if (APP->_systemInfo.flags & SYSTEM_INFO_VALID) {
		ctx.show(APP->_systemInfoScreen, false, true);
	} else {
		APP->_setupWorker(&App::_systemInfoWorker);
		ctx.show(APP->_workerStatusScreen, false, true);
	}
}

void MainMenuScreen::setResolution(ui::Context &ctx) {
	ctx.show(APP->_resolutionScreen, false, true);
}

void MainMenuScreen::about(ui::Context &ctx) {
	ctx.show(APP->_aboutScreen, false, true);
}

void MainMenuScreen::launchEXE(ui::Context &ctx) {
	//ctx.show(APP->_aboutScreen, false, true);
}

void MainMenuScreen::ejectCD(ui::Context &ctx) {
	APP->_setupWorker(&App::_atapiEjectWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void MainMenuScreen::reboot(ui::Context &ctx) {
	APP->_setupWorker(&App::_rebootWorker);
	ctx.show(APP->_workerStatusScreen, false, true);
}

void MainMenuScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("MainMenuScreen.title");
	_prompt     = STRH(_MENU_ENTRIES[0].prompt);
	_itemPrompt = STR("MainMenuScreen.itemPrompt");

	_listLength = util::countOf(_MENU_ENTRIES);

	ListScreen::show(ctx, goBack);
}

void MainMenuScreen::update(ui::Context &ctx) {
	auto &action = _MENU_ENTRIES[_activeItem];
	_prompt      = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		(this->*action.target)(ctx);
}

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
