
#include <stdarg.h>
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
	auto &worker    = APP->_workerStatus;
	auto nextScreen = worker.nextScreen;

	if (worker.status >= WORKER_NEXT) {
		worker.reset();
		ctx.show(*nextScreen, worker.status == WORKER_NEXT_BACK);

		LOG("worker finished, next=0x%08x", nextScreen);
		return;
	}

	_setProgress(ctx, worker.progress, worker.progressTotal);
	_body = worker.message;
}

static constexpr int WARNING_COOLDOWN = 15;

void WarningScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("WarningScreen.title");
	_body       = STR("WarningScreen.body");
	_buttons[0] = _buttonText;

	_locked     = true;
	_numButtons = 1;

	_cooldownTimer = ctx.time + ctx.gpuCtx.refreshRate * WARNING_COOLDOWN;

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

void ErrorScreen::setMessage(ui::Screen &prev, const char *format, ...) {
	_prevScreen = &prev;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_bodyText, sizeof(_bodyText), format, ap);
	va_end(ap);
}

void ErrorScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ErrorScreen.title");
	_body       = _bodyText;
	_buttons[0] = STR("ErrorScreen.ok");

	_numButtons = 1;
	_locked     = _prevScreen ? false : true;

	MessageScreen::show(ctx, goBack);
}

void ErrorScreen::update(ui::Context &ctx) {
	MessageScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(*_prevScreen, true, true);
}

void ConfirmScreen::setMessage(
	ui::Screen &prev, void (*callback)(ui::Context &ctx), const char *format,
	...
) {
	_prevScreen = &prev;
	_callback   = callback;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_bodyText, sizeof(_bodyText), format, ap);
	va_end(ap);
}

void ConfirmScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("ConfirmScreen.title");
	_body       = _bodyText;
	_buttons[0] = STR("ConfirmScreen.no");
	_buttons[1] = STR("ConfirmScreen.yes");

	_numButtons = 2;

	MessageScreen::show(ctx, goBack);
	ctx.sounds[ui::SOUND_ERROR].play();
}

void ConfirmScreen::update(ui::Context &ctx) {
	MessageScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton)
			_callback(ctx);
		else
			ctx.show(*_prevScreen, true, true);
	}
}
