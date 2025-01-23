/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

#include <stdio.h>
#include "common/nvram/bios.hpp"
#include "common/sys573/base.hpp"
#include "common/util/log.hpp"
#include "common/rom.hpp"
#include "main/app/app.hpp"
#include "main/app/nvramactions.hpp"
#include "main/workers/miscworkers.hpp"
#include "main/workers/nvramworkers.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"

/* NVRAM device submenu */

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr __VA_OPT__(,) __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void NVRAMInfoScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("NVRAMInfoScreen.title");
	_body   = _bodyText;
	_prompt = STR("NVRAMInfoScreen.prompt");

	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	// BIOS ROM
	_PRINT(STR("NVRAMInfoScreen.bios.header"));

	if (nvram::sonyKernelHeader.validateMagic()) {
		_PRINT(
			STR("NVRAMInfoScreen.bios.kernelInfo.sony"),
			nvram::sonyKernelHeader.version,
			nvram::sonyKernelHeader.year,
			nvram::sonyKernelHeader.month,
			nvram::sonyKernelHeader.day
		);
	} else if (nvram::openBIOSHeader.validateMagic()) {
		char buildID[64];

		nvram::openBIOSHeader.getBuildID(buildID);
		_PRINT(STR("NVRAMInfoScreen.bios.kernelInfo.openbios"), buildID);
	} else {
		_PRINT(STR("NVRAMInfoScreen.bios.kernelInfo.unknown"));
	}

	nvram::ShellInfo shell;

	if (nvram::getShellInfo(shell)) {
		if (shell.bootFileName)
			_PRINT(
				STR("NVRAMInfoScreen.bios.shellInfo.konami"),
				shell.name,
				shell.bootFileName
			);
		else
			_PRINT(STR("NVRAMInfoScreen.bios.shellInfo.custom"), shell.name);
	} else {
		_PRINT(STR("NVRAMInfoScreen.bios.shellInfo.unknown"));
	}

	_PRINTLN();

	// RTC RAM
	_PRINT(STR("NVRAMInfoScreen.rtc.header"));
	_PRINT(STRH(
		sys573::isRTCBatteryLow()
			? "NVRAMInfoScreen.rtc.batteryLow"_h
			: "NVRAMInfoScreen.rtc.batteryOK"_h
	));

	_PRINTLN();

	// Internal flash
	auto id = rom::flash.getJEDECID();

	_PRINT(STR("NVRAMInfoScreen.flash.header"));
	_PRINT(
		STR("NVRAMInfoScreen.flash.info"),
		(id >>  0) & 0xff,
		(id >>  8) & 0xff,
		(id >> 16) & 0xff,
		(id >> 24) & 0xff
	);

	if (rom::flash.getBootExecutableHeader())
		_PRINT(STR("NVRAMInfoScreen.flash.bootable"));

	// TODO: show information about currently installed game

	_PRINTLN();

	// PCMCIA cards
	for (int i = 0; i < 2; i++) {
		auto &card = rom::pcmcia[i];

		_PRINT(STR("NVRAMInfoScreen.pcmcia.header"), i + 1);

		if (card.isPresent()) {
			auto id     = card.getJEDECID();
			auto length = card.getActualLength();

			_PRINT(
				STR("NVRAMInfoScreen.pcmcia.info"),
				(id >>  0) & 0xff,
				(id >>  8) & 0xff,
				(id >> 16) & 0xff,
				(id >> 24) & 0xff
			);

			if (length)
				_PRINT(
					STR("NVRAMInfoScreen.pcmcia.sizeInfo"), length / 0x100000
				);
			if (card.getBootExecutableHeader())
				_PRINT(STR("NVRAMInfoScreen.pcmcia.bootable"));
		} else {
			_PRINT(STR("NVRAMInfoScreen.pcmcia.noCard"));
		}

		_PRINTLN();
	}

	*(--ptr) = 0;
	LOG_APP("%d buffer bytes free", end - ptr);

	TextScreen::show(ctx, goBack);
}

void NVRAMInfoScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT))
			ctx.show(APP->_mainMenuScreen, true, true);
		else
			ctx.show(APP->_nvramActionsScreen, false, true);
	}
}

struct Action {
public:
	util::Hash        name, prompt;
	const rom::Region &region;
	void              (NVRAMActionsScreen::*target)(
		ui::Context &ctx, size_t length
	);
};

static const Action _ACTIONS[]{
	{
		.name   = "NVRAMActionsScreen.runExecutable.flash.name"_h,
		.prompt = "NVRAMActionsScreen.runExecutable.flash.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::runExecutable
	}, {
		.name   = "NVRAMActionsScreen.runExecutable.pcmcia1.name"_h,
		.prompt = "NVRAMActionsScreen.runExecutable.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &NVRAMActionsScreen::runExecutable
	}, {
		.name   = "NVRAMActionsScreen.runExecutable.pcmcia2.name"_h,
		.prompt = "NVRAMActionsScreen.runExecutable.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &NVRAMActionsScreen::runExecutable
	}, {
		.name   = "NVRAMActionsScreen.checksum.name"_h,
		.prompt = "NVRAMActionsScreen.checksum.prompt"_h,
		.region = rom::bios, // Dummy
		.target = &NVRAMActionsScreen::checksum
	}, {
		.name   = "NVRAMActionsScreen.dump.name"_h,
		.prompt = "NVRAMActionsScreen.dump.prompt"_h,
		.region = rom::bios, // Dummy
		.target = &NVRAMActionsScreen::dump
	}, {
		.name   = "NVRAMActionsScreen.restore.rtc.name"_h,
		.prompt = "NVRAMActionsScreen.restore.rtc.prompt"_h,
		.region = rom::rtc,
		.target = &NVRAMActionsScreen::restore
	}, {
		.name   = "NVRAMActionsScreen.restore.flash.name"_h,
		.prompt = "NVRAMActionsScreen.restore.flash.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::restore
	}, {
		.name   = "NVRAMActionsScreen.restore.pcmcia1.name"_h,
		.prompt = "NVRAMActionsScreen.restore.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &NVRAMActionsScreen::restore
	}, {
		.name   = "NVRAMActionsScreen.restore.pcmcia2.name"_h,
		.prompt = "NVRAMActionsScreen.restore.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &NVRAMActionsScreen::restore
	}, {
		.name   = "NVRAMActionsScreen.erase.rtc.name"_h,
		.prompt = "NVRAMActionsScreen.erase.rtc.prompt"_h,
		.region = rom::rtc,
		.target = &NVRAMActionsScreen::erase
	}, {
		.name   = "NVRAMActionsScreen.erase.flash.name"_h,
		.prompt = "NVRAMActionsScreen.erase.flash.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::erase
	}, {
		.name   = "NVRAMActionsScreen.erase.pcmcia1.name"_h,
		.prompt = "NVRAMActionsScreen.erase.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &NVRAMActionsScreen::erase
	}, {
		.name   = "NVRAMActionsScreen.erase.pcmcia2.name"_h,
		.prompt = "NVRAMActionsScreen.erase.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &NVRAMActionsScreen::erase
#if 0
	}, {
		.name   = "NVRAMActionsScreen.installExecutable.flash.name"_h,
		.prompt = "NVRAMActionsScreen.installExecutable.flash.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::installExecutable
	}, {
		.name   = "NVRAMActionsScreen.installExecutable.pcmcia1.name"_h,
		.prompt = "NVRAMActionsScreen.installExecutable.pcmcia1.prompt"_h,
		.region = rom::pcmcia[0],
		.target = &NVRAMActionsScreen::installExecutable
	}, {
		.name   = "NVRAMActionsScreen.installExecutable.pcmcia2.name"_h,
		.prompt = "NVRAMActionsScreen.installExecutable.pcmcia2.prompt"_h,
		.region = rom::pcmcia[1],
		.target = &NVRAMActionsScreen::installExecutable
#endif
	}, {
		.name   = "NVRAMActionsScreen.resetFlashHeader.name"_h,
		.prompt = "NVRAMActionsScreen.resetFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::resetFlashHeader
#if 0
	}, {
		.name   = "NVRAMActionsScreen.matchFlashHeader.name"_h,
		.prompt = "NVRAMActionsScreen.matchFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::matchFlashHeader
	}, {
		.name   = "NVRAMActionsScreen.editFlashHeader.name"_h,
		.prompt = "NVRAMActionsScreen.editFlashHeader.prompt"_h,
		.region = rom::flash,
		.target = &NVRAMActionsScreen::editFlashHeader
#endif
	}
};

const char *NVRAMActionsScreen::_getItemName(
	ui::Context &ctx, int index
) const {
	return STRH(_ACTIONS[index].name);
}

void NVRAMActionsScreen::runExecutable(ui::Context &ctx, size_t length) {
	if (selectedRegion->getBootExecutableHeader()) {
		APP->_runWorker(&executableWorker, true);
	} else {
		APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
		APP->_messageScreen.setMessage(
			MESSAGE_ERROR, STR("NVRAMActionsScreen.runExecutable.error")
		);

		ctx.show(APP->_messageScreen, false, true);
	}
}

void NVRAMActionsScreen::checksum(ui::Context &ctx, size_t length) {
	if (APP->_checksumScreen.valid)
		ctx.show(APP->_checksumScreen, false, true);
	else
		APP->_runWorker(&nvramChecksumWorker, true);
}

void NVRAMActionsScreen::dump(ui::Context &ctx, size_t length) {
	APP->_confirmScreen.previousScreen = this;
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] =
				&(APP->_nvramInfoScreen);
			APP->_messageScreen.previousScreens[MESSAGE_ERROR]   =
				&(APP->_nvramActionsScreen);

			APP->_runWorker(&nvramDumpWorker, true);
		},
		STR("NVRAMActionsScreen.dump.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void NVRAMActionsScreen::restore(ui::Context &ctx, size_t length) {
	selectedLength = length;

	APP->_filePickerScreen.previousScreen = this;
	APP->_filePickerScreen.setMessage(
		[](ui::Context &ctx) {
			ctx.show(APP->_confirmScreen, false, true);
		},
		STR("NVRAMActionsScreen.restore.filePrompt")
	);

	APP->_confirmScreen.previousScreen = &(APP->_fileBrowserScreen);
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] =
				&(APP->_nvramInfoScreen);
			APP->_messageScreen.previousScreens[MESSAGE_ERROR]   =
				&(APP->_fileBrowserScreen);

			APP->_runWorker(&nvramRestoreWorker, true);
		},
		STR("NVRAMActionsScreen.restore.confirm")
	);

	APP->_filePickerScreen.reloadAndShow(ctx);
}

void NVRAMActionsScreen::erase(ui::Context &ctx, size_t length) {
	selectedLength = length;

	APP->_confirmScreen.previousScreen = this;
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] =
				&(APP->_nvramInfoScreen);
			APP->_messageScreen.previousScreens[MESSAGE_ERROR]   =
				&(APP->_nvramActionsScreen);

			APP->_runWorker(&nvramEraseWorker, true);
		},
		STR("NVRAMActionsScreen.erase.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void NVRAMActionsScreen::installExecutable(ui::Context &ctx, size_t length) {
	selectedLength = length;

	APP->_filePickerScreen.previousScreen = this;
	APP->_filePickerScreen.setMessage(
		[](ui::Context &ctx) {
			ctx.show(APP->_confirmScreen, false, true);
		},
		STR("NVRAMActionsScreen.installExecutable.filePrompt")
	);

	APP->_confirmScreen.previousScreen = &(APP->_fileBrowserScreen);
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] =
				&(APP->_nvramInfoScreen);
			APP->_messageScreen.previousScreens[MESSAGE_ERROR]   =
				&(APP->_fileBrowserScreen);

			APP->_runWorker(&flashExecutableWriteWorker, true);
		},
		STR("NVRAMActionsScreen.installExecutable.confirm")
	);

	APP->_filePickerScreen.reloadAndShow(ctx);
}

void NVRAMActionsScreen::resetFlashHeader(ui::Context &ctx, size_t length) {
	APP->_confirmScreen.previousScreen = this;
	APP->_confirmScreen.setMessage(
		[](ui::Context &ctx) {
			util::clear(APP->_romHeaderDump.data);

			APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
				&(APP->_nvramActionsScreen);

			APP->_runWorker(&flashHeaderWriteWorker, true);
		},
		STR("NVRAMActionsScreen.resetFlashHeader.confirm")
	);

	ctx.show(APP->_confirmScreen, false, true);
}

void NVRAMActionsScreen::matchFlashHeader(ui::Context &ctx, size_t length) {
	// TODO: implement
}

void NVRAMActionsScreen::editFlashHeader(ui::Context &ctx, size_t length) {
	// TODO: implement
}

void NVRAMActionsScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("NVRAMActionsScreen.title");
	_prompt     = STRH(_ACTIONS[0].prompt);
	_itemPrompt = STR("NVRAMActionsScreen.itemPrompt");

	_listLength = util::countOf(_ACTIONS);

	ListScreen::show(ctx, goBack);
}

void NVRAMActionsScreen::update(ui::Context &ctx) {
	auto &action = _ACTIONS[_activeItem];
	_prompt      = STRH(action.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			ctx.show(APP->_nvramInfoScreen, true, true);
		} else {
			if (action.region.isPresent()) {
				auto length    = action.region.getActualLength();
				selectedRegion = &(action.region);

				if (length) {
					(this->*action.target)(ctx, length);
				} else {
					APP->_cardSizeScreen.callback = action.target;
					ctx.show(APP->_cardSizeScreen, false, true);
				}
			} else {
				APP->_messageScreen.previousScreens[MESSAGE_ERROR] = this;
				APP->_messageScreen.setMessage(
					MESSAGE_ERROR,
					STR("NVRAMActionsScreen.cardError")
				);

				ctx.show(APP->_messageScreen, false, true);
			}
		}
	}
}

void CardSizeScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("CardSizeScreen.title");
	_body       = STR("CardSizeScreen.body");
	_buttons[0] = STR("CardSizeScreen.cancel");
	_buttons[1] = STR("CardSizeScreen.8");
	_buttons[2] = STR("CardSizeScreen.16");
	_buttons[3] = STR("CardSizeScreen.32");
	_buttons[4] = STR("CardSizeScreen.64");

	_numButtons = 5;

	MessageBoxScreen::show(ctx, goBack);
}

void CardSizeScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton)
			(APP->_nvramActionsScreen.*callback)(
				ctx,
				0x400000 << _activeButton // 1 = 8 MB, 2 = 16 MB, ...
			);
		else
			ctx.show(APP->_nvramActionsScreen, true, true);
	}
}

void ChecksumScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("ChecksumScreen.title");
	_body   = _bodyText;
	_prompt = STR("ChecksumScreen.prompt");

	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	_PRINT(STR("ChecksumScreen.bios"),  values.bios);
	_PRINT(STR("ChecksumScreen.rtc"),   values.rtc);
	_PRINT(STR("ChecksumScreen.flash"), values.flash);

	_PRINTLN();

	for (int i = 0; i < 2; i++) {
		if (!rom::pcmcia[i].isPresent())
			continue;

		auto slot = i + 1;
		auto crc  = values.pcmcia[i];

		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 16, crc[0]);
		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 32, crc[1]);
		_PRINT(STR("ChecksumScreen.pcmcia"), slot, 64, crc[3]);

		_PRINTLN();
	}

	_PRINT(STR("ChecksumScreen.description"));

	LOG_APP("%d buffer bytes free", end - ptr);

	TextScreen::show(ctx, goBack);
}

void ChecksumScreen::update(ui::Context &ctx) {
	TextScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_nvramActionsScreen, true, true);
}
