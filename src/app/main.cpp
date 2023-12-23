
#include "app/app.hpp"
#include "app/main.hpp"
#include "ps1/gpucmd.h"
#include "uibase.hpp"
#include "util.hpp"

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
}

void ButtonMappingScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		ctx.buttons.buttonMap = ui::ButtonMap(_activeItem);
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
		.name   = "MainMenuScreen.systemInfo.name"_h,
		.prompt = "MainMenuScreen.systemInfo.prompt"_h,
		.target = &MainMenuScreen::systemInfo
	}, {
#endif
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
	//ctx.show(APP->systemInfoScreen, false, true);
}

void MainMenuScreen::setResolution(ui::Context &ctx) {
	ctx.show(APP->_resolutionScreen, false, true);
}

void MainMenuScreen::about(ui::Context &ctx) {
	ctx.show(APP->_aboutScreen, false, true);
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
