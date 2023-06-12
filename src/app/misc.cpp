
#include <stdio.h>
#include "app/app.hpp"
#include "app/misc.hpp"
#include "defs.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Initial setup screens */

void WorkerStatusScreen::show(ui::Context &ctx, bool goBack) {
	_title = STR("WorkerStatusScreen.title");

	ProgressScreen::show(ctx, goBack);
}

void WorkerStatusScreen::update(ui::Context &ctx) {
	auto &status    = APP->_workerStatus;
	auto nextScreen = status.nextScreen;

	if (nextScreen) {
		LOG("worker finished, next=0x%08x", nextScreen);

		ctx.show(*nextScreen, status.nextGoBack);
		return;
	}

	_setProgress(ctx, status.progress, status.progressTotal);
	_body = status.message;
}

static constexpr int WARNING_COOLDOWN = 15;

void WarningScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("WarningScreen.title");
	_body       = STR("WarningScreen.body");
	_buttons[0] = _buttonText;

	_locked     = true;
	_numButtons = 1;

#ifdef NDEBUG
	_cooldownTimer = ctx.time + ctx.gpuCtx.refreshRate * WARNING_COOLDOWN;
#else
	_cooldownTimer = 0;
#endif

	MessageScreen::show(ctx, goBack);

	ctx.buttons.buttonMap = ui::MAP_SINGLE_BUTTON;
}

void WarningScreen::update(ui::Context &ctx) {
	MessageScreen::update(ctx);

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

		APP->_setupWorker(&App::_cartDetectWorker);
		ctx.show(APP->_workerStatusScreen, false, true);
	}
}
