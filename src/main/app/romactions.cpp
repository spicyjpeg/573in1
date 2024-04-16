
#include <stdio.h>
#include "common/io.hpp"
#include "common/rom.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/romactions.hpp"
#include "main/cartdata.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* Storage device submenu */

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr __VA_OPT__(,) __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void StorageInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("StorageInfoScreen.title");
	_body   = _bodyText;
	_prompt = STR("StorageInfoScreen.prompt");

	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	// BIOS ROM
	_PRINT(STR("StorageInfoScreen.bios.header"));

	if (rom::sonyKernelHeader.validateMagic()) {
		_PRINT(
			STR("StorageInfoScreen.bios.kernelInfo.sony"),
			rom::sonyKernelHeader.version, rom::sonyKernelHeader.year,
			rom::sonyKernelHeader.month, rom::sonyKernelHeader.day
		);
	} else if (rom::openBIOSHeader.validateMagic()) {
		char buildID[64];

		rom::openBIOSHeader.getBuildID(buildID);
		_PRINT(STR("StorageInfoScreen.bios.kernelInfo.openbios"), buildID);
	} else {
		_PRINT(STR("StorageInfoScreen.bios.kernelInfo.unknown"));
	}

	auto shell = rom::getShellInfo();

	if (shell)
		_PRINT(
			STR("StorageInfoScreen.bios.shellInfo.konami"), shell->name,
			shell->bootFileName
		);
	else
		_PRINT(STR("StorageInfoScreen.bios.shellInfo.unknown"));

	_PRINTLN();

	// RTC RAM
	_PRINT(STR("StorageInfoScreen.rtc.header"));
	_PRINT(
		io::isRTCBatteryLow()
			? STR("StorageInfoScreen.rtc.batteryLow")
			: STR("StorageInfoScreen.rtc.batteryOK")
	);

	_PRINTLN();

	// Internal flash
	auto id = rom::flash.getJEDECID();

	_PRINT(STR("StorageInfoScreen.flash.header"));
	_PRINT(
		STR("StorageInfoScreen.flash.info"),
		(id >>  0) & 0xff,
		(id >>  8) & 0xff,
		(id >> 16) & 0xff,
		(id >> 24) & 0xff
	);

	if (rom::flash.hasBootExecutable())
		_PRINT(STR("StorageInfoScreen.flash.bootable"));

	// TODO: show information about currently installed game

	_PRINTLN();

	// PCMCIA cards
	for (int i = 0; i < 2; i++) {
		auto &card = rom::pcmcia[i];

		_PRINT(STR("StorageInfoScreen.pcmcia.header"), i + 1);

		if (card.isPresent()) {
			auto id = card.getJEDECID();

			_PRINT(
				STR("StorageInfoScreen.pcmcia.info"),
				(id >>  0) & 0xff,
				(id >>  8) & 0xff,
				(id >> 16) & 0xff,
				(id >> 24) & 0xff
			);

			if (card.hasBootExecutable())
				_PRINT(STR("StorageInfoScreen.pcmcia.bootable"));
		} else {
			_PRINT(STR("StorageInfoScreen.pcmcia.noCard"));
		}

		_PRINTLN();
	}

	*(--ptr) = 0;
	LOG("remaining=%d", end - ptr);

	TextScreen::show(ctx, goBack);
}

void StorageInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT))
			ctx.show(APP->_mainMenuScreen, true, true);
		else
			ctx.show(APP->_storageActionsScreen, false, true);
	}
}

struct Action {
public:
	util::Hash        name, prompt;
	const rom::Region &region;
	void              (StorageActionsScreen::*target)(ui::Context &ctx);
};

static const Action _ACTIONS[]{
	{
		.name   = "StorageActionsScreen.checksum.name"_h,
		.prompt = "StorageActionsScreen.checksum.prompt"_h,
		.region = rom::bios, // Dummy
		.target = &StorageActionsScreen::checksum
	}, {
		.name   = "StorageActionsScreen.dump.name"_h,
		.prompt = "StorageActionsScreen.dump.prompt"_h,
		.region = rom::bios, // Dummy
		.target = &StorageActionsScreen::dump
	}, {
		.name   = "StorageActionsScreen.restore.rtc.name"_h,
		.prompt = "StorageActionsScreen.restore.rtc.prompt"_h,
		.region = rom::rtc,
		.target = &StorageActionsScreen::restore
	}, {
		.name   = "StorageActionsScreen.restore.flash.name"_h,
		.prompt = "StorageActionsScreen.restore.flash.prompt"_h,
		.region = rom::flash,
		.target = &StorageActionsScreen::restore
	}, {
		.name   = "StorageActionsScreen.restore.pcmcia1.name"_h,
		.prompt = "StorageActionsScreen.restore.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &StorageActionsScreen::restore
	}, {
		.name   = "StorageActionsScreen.restore.pcmcia2.name"_h,
		.prompt = "StorageActionsScreen.restore.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &StorageActionsScreen::restore
	}, {
		.name   = "StorageActionsScreen.erase.rtc.name"_h,
		.prompt = "StorageActionsScreen.erase.rtc.prompt"_h,
		.region = rom::rtc,
		.target = &StorageActionsScreen::erase
	}, {
		.name   = "StorageActionsScreen.erase.flash.name"_h,
		.prompt = "StorageActionsScreen.erase.flash.prompt"_h,
		.region = rom::flash,
		.target = &StorageActionsScreen::erase
	}, {
		.name   = "StorageActionsScreen.erase.pcmcia1.name"_h,
		.prompt = "StorageActionsScreen.erase.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &StorageActionsScreen::erase
	}, {
		.name   = "StorageActionsScreen.erase.pcmcia2.name"_h,
		.prompt = "StorageActionsScreen.erase.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &StorageActionsScreen::erase
	}, {
		.name   = "StorageActionsScreen.resetFlashHeader.name"_h,
		.prompt = "StorageActionsScreen.resetFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &StorageActionsScreen::resetFlashHeader
	}, {
		.name   = "StorageActionsScreen.matchFlashHeader.name"_h,
		.prompt = "StorageActionsScreen.matchFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &StorageActionsScreen::matchFlashHeader
	}, {
		.name   = "StorageActionsScreen.editFlashHeader.name"_h,
		.prompt = "StorageActionsScreen.editFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &StorageActionsScreen::editFlashHeader
	}
};

const char *StorageActionsScreen::_getItemName(
	ui::Context &ctx, int index
) const {
	return STRH(_ACTIONS[index].name);
}

void StorageActionsScreen::checksum(ui::Context &ctx) {
	if (APP->_checksumScreen.valid) {
		ctx.show(APP->_checksumScreen, false, true);
	} else {
		APP->_setupWorker(&App::_romChecksumWorker);
		ctx.show(APP->_workerStatusScreen, false, true);
	}
}

void StorageActionsScreen::dump(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_romDumpWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("StorageActionsScreen.dump.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void StorageActionsScreen::restore(ui::Context &ctx) {
	APP->_filePickerScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			ctx.show(APP->_confirmScreen, false, true);
		},
		STR("StorageActionsScreen.restore.filePrompt")
	);
	APP->_confirmScreen.setMessage(
		APP->_filePickerScreen,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_romRestoreWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("StorageActionsScreen.restore.confirm")
	);

	APP->_filePickerScreen.loadRootAndShow(ctx);
}

void StorageActionsScreen::erase(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_romEraseWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("StorageActionsScreen.erase.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void StorageActionsScreen::resetFlashHeader(ui::Context &ctx) {
	APP->_confirmScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_flashHeaderEraseWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("StorageActionsScreen.resetFlashHeader.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void StorageActionsScreen::matchFlashHeader(ui::Context &ctx) {
	// TODO: implement
}

void StorageActionsScreen::editFlashHeader(ui::Context &ctx) {
	// TODO: implement
}

void StorageActionsScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("StorageActionsScreen.title");
	_prompt     = STRH(_ACTIONS[0].prompt);
	_itemPrompt = STR("StorageActionsScreen.itemPrompt");

	_listLength = util::countOf(_ACTIONS);

	ListScreen::show(ctx, goBack);
}

void StorageActionsScreen::update(ui::Context &ctx) {
	auto &action = _ACTIONS[_activeItem];
	_prompt      = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_mainMenuScreen, true, true);
		} else {
			if (action.region.isPresent()) {
				this->_selectedRegion = &(action.region);
				(this->*action.target)(ctx);
			} else {
				APP->_messageScreen.setMessage(
					MESSAGE_ERROR, *this, STR("StorageActionsScreen.cardError")
				);

				ctx.show(APP->_messageScreen, false, true);
			}
		}
	}
}

void ChecksumScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("ChecksumScreen.title");
	_body   = _bodyText;
	_prompt = STR("ChecksumScreen.prompt");

	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	_PRINT(STR("ChecksumScreen.bios"),  biosCRC);
	_PRINT(STR("ChecksumScreen.rtc"),   rtcCRC);
	_PRINT(STR("ChecksumScreen.flash"), flashCRC);

	_PRINTLN();

	for (int i = 0; i < 2; i++) {
		if (!rom::pcmcia[i].isPresent())
			continue;

		auto slot = i + 1;
		auto crc  = pcmciaCRC[i];

		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 16, crc[0]);
		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 32, crc[1]);
		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 64, crc[3]);

		_PRINTLN();
	}

	*(--ptr) = 0;
	LOG("remaining=%d", end - ptr);

	TextScreen::show(ctx, goBack);
}

void ChecksumScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_storageActionsScreen, true, true);
}
