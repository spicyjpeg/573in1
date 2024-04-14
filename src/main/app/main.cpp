
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/main.hpp"
#include "main/uibase.hpp"

/* Main menu screens */

static constexpr int _WARNING_COOLDOWN = 10;

void WarningScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("WarningScreen.title");
	_body       = STR("WarningScreen.body");
	_buttons[0] = _buttonText;

	_locked     = true;
	_numButtons = 1;

	_cooldownTimer = ctx.time + ctx.gpuCtx.refreshRate * _WARNING_COOLDOWN;

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
		.name   = "MainMenuScreen.cartInfo.name"_h,
		.prompt = "MainMenuScreen.cartInfo.prompt"_h,
		.target = &MainMenuScreen::cartInfo
	}, {
		.name   = "MainMenuScreen.storageMenu.name"_h,
		.prompt = "MainMenuScreen.storageMenu.prompt"_h,
		.target = &MainMenuScreen::storageMenu
	}, {
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
		.name   = "MainMenuScreen.runExecutable.name"_h,
		.prompt = "MainMenuScreen.runExecutable.prompt"_h,
		.target = &MainMenuScreen::runExecutable
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
	if (APP->_cartDriver) {
		ctx.show(APP->_cartInfoScreen, false, true);
	} else {
		APP->_setupWorker(&App::_cartDetectWorker);
		ctx.show(APP->_workerStatusScreen, false, true);
	}
}

void MainMenuScreen::storageMenu(ui::Context &ctx) {
	ctx.show(APP->_storageMenuScreen, false, true);
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

void MainMenuScreen::runExecutable(ui::Context &ctx) {
	APP->_filePickerScreen.setMessage(
		*this,
		[](ui::Context &ctx) {
			APP->_setupWorker(&App::_executableWorker);
			ctx.show(APP->_workerStatusScreen, false, true);
		},
		STR("MainMenuScreen.runExecutable.filePrompt")
	);

	APP->_filePickerScreen.loadRootAndShow(ctx);
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
