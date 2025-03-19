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

#include <stdio.h>
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "main/app/app.hpp"
#include "main/app/mainmenu.hpp"
#include "main/workers/cartworkers.hpp"
#include "main/workers/miscworkers.hpp"
#include "main/uibase.hpp"

/* Main menu screens */

static constexpr int _WARNING_COOLDOWN = 10;
static constexpr int _AUTOBOOT_DELAY   = 5;

void WarningScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("WarningScreen.title");
	_body       = STR("WarningScreen.body");
	_buttons[0] = _buttonText;

	_locked     = true;
	_numButtons = 1;

#ifdef NDEBUG
	_timer         = ctx.time + ctx.gpuCtx.refreshRate * _WARNING_COOLDOWN;
#else
	_timer         = 0;
#endif
	_buttonText[0] = 0;

	MessageBoxScreen::show(ctx, goBack);
}

void WarningScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	int time = _timer - ctx.time;
	_locked  = (time > 0);

	if (_locked) {
		time = (time / ctx.gpuCtx.refreshRate) + 1;

		snprintf(
			_buttonText,
			sizeof(_buttonText),
			STR("WarningScreen.cooldown"),
			time
		);
		return;
	}

	_buttons[0] = STR("WarningScreen.ok");

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_buttonMappingScreen, false, true);
}

void AutobootScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("AutobootScreen.title");
	_body       = _bodyText;
	_buttons[0] = _buttonText;

	_numButtons = 1;

	_timer         = ctx.time + ctx.gpuCtx.refreshRate * _AUTOBOOT_DELAY;
	_buttonText[0] = 0;

	if (APP->_nvramActionsScreen.selectedRegion)
		snprintf(_bodyText, sizeof(_bodyText), STR("AutobootScreen.flash"));
	else
		snprintf(
			_bodyText,
			sizeof(_bodyText),
			STR("AutobootScreen.ide"),
			APP->_fileBrowserScreen.selectedPath
		);

	MessageBoxScreen::show(ctx, goBack);
}

void AutobootScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	int time = _timer - ctx.time;

	if (time < 0) {
		APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
			&(APP->_warningScreen);

		APP->_runWorker(&executableWorker);
		return;
	}

	time = (time / ctx.gpuCtx.refreshRate) + 1;

	snprintf(
		_buttonText,
		sizeof(_buttonText),
		STR("AutobootScreen.cancel"),
		time
	);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_warningScreen, false, true);
}

static const util::Hash _MAPPING_NAMES[]{
	"ButtonMappingScreen.joystick"_h,
	"ButtonMappingScreen.ddrCab"_h,
	"ButtonMappingScreen.ddrSoloCab"_h,
	"ButtonMappingScreen.dmCab"_h,
	"ButtonMappingScreen.dmxCab"_h
};

const char *ButtonMappingScreen::_getItemName(
	ui::Context &ctx, int index
) const {
	return STRH(_MAPPING_NAMES[index]);
}

void ButtonMappingScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ButtonMappingScreen.title");
	_prompt     = STR("ButtonMappingScreen.prompt");
	_itemPrompt = STR("ButtonMappingScreen.itemPrompt");

	_listLength = ui::NUM_BUTTON_MAPS - 1;

	ListScreen::show(ctx, goBack);
	ctx.buttons.setButtonMap(ui::MAP_SINGLE_BUTTON);
}

void ButtonMappingScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		ctx.buttons.setButtonMap(ui::ButtonMap(_activeItem));
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
		.name   = "MainMenuScreen.cartInfo.name"_h,
		.prompt = "MainMenuScreen.cartInfo.prompt"_h,
		.target = &MainMenuScreen::cartInfo
	}, {
		.name   = "MainMenuScreen.nvramInfo.name"_h,
		.prompt = "MainMenuScreen.nvramInfo.prompt"_h,
		.target = &MainMenuScreen::nvramInfo
	}, {
		.name   = "MainMenuScreen.ideInfo.name"_h,
		.prompt = "MainMenuScreen.ideInfo.prompt"_h,
		.target = &MainMenuScreen::ideInfo
	}, {
		.name   = "MainMenuScreen.runExecutable.name"_h,
		.prompt = "MainMenuScreen.runExecutable.prompt"_h,
		.target = &MainMenuScreen::runExecutable
	}, {
		.name   = "MainMenuScreen.setRTCTime.name"_h,
		.prompt = "MainMenuScreen.setRTCTime.prompt"_h,
		.target = &MainMenuScreen::setRTCTime
	}, {
		.name   = "MainMenuScreen.testMenu.name"_h,
		.prompt = "MainMenuScreen.testMenu.prompt"_h,
		.target = &MainMenuScreen::testMenu
#if 0
	}, {
		.name   = "MainMenuScreen.setLanguage.name"_h,
		.prompt = "MainMenuScreen.setLanguage.prompt"_h,
		.target = &MainMenuScreen::setLanguage
#endif
	}, {
		.name   = "MainMenuScreen.setResolution.name"_h,
		.prompt = "MainMenuScreen.setResolution.prompt"_h,
		.target = &MainMenuScreen::setResolution
	}, {
		.name   = "MainMenuScreen.about.name"_h,
		.prompt = "MainMenuScreen.about.prompt"_h,
		.target = &MainMenuScreen::about
	}, {
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
	APP->_runWorker(&cartDetectWorker, true);
}

void MainMenuScreen::nvramInfo(ui::Context &ctx) {
	ctx.show(APP->_nvramInfoScreen, false, true);
}

void MainMenuScreen::ideInfo(ui::Context &ctx) {
	ctx.show(APP->_ideInfoScreen, false, true);
}

void MainMenuScreen::runExecutable(ui::Context &ctx) {
	APP->_filePickerScreen.previousScreen = this;
	APP->_filePickerScreen.setMessage(
		[](ui::Context &ctx) {
			APP->_nvramActionsScreen.selectedRegion            = nullptr;
			APP->_messageScreen.previousScreens[MESSAGE_ERROR] =
				&(APP->_fileBrowserScreen);

			APP->_runWorker(&executableWorker, true);
		},
		STR("MainMenuScreen.runExecutable.filePrompt")
	);

	APP->_filePickerScreen.reloadAndShow(ctx);
}

void MainMenuScreen::setRTCTime(ui::Context &ctx) {
	ctx.show(APP->_rtcTimeScreen, false, true);
}

void MainMenuScreen::testMenu(ui::Context &ctx) {
	ctx.show(APP->_testMenuScreen, false, true);
}

void MainMenuScreen::setLanguage(ui::Context &ctx) {
	ctx.show(APP->_languageScreen, false, true);
}

void MainMenuScreen::setResolution(ui::Context &ctx) {
	ctx.show(APP->_resolutionScreen, false, true);
}

void MainMenuScreen::about(ui::Context &ctx) {
	ctx.show(APP->_aboutScreen, false, true);
}

void MainMenuScreen::ejectCD(ui::Context &ctx) {
	APP->_messageScreen.previousScreens[MESSAGE_SUCCESS] = this;
	APP->_messageScreen.previousScreens[MESSAGE_ERROR]   = this;

	APP->_runWorker(&atapiEjectWorker, true);
}

void MainMenuScreen::reboot(ui::Context &ctx) {
	APP->_runWorker(&rebootWorker, true);
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
