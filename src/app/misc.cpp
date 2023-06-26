
#include <stdarg.h>
#include <stdio.h>
#include "app/app.hpp"
#include "app/misc.hpp"
#include "uibase.hpp"
#include "util.hpp"

/* Common screens */

void WorkerStatusScreen::show(ui::Context &ctx, bool goBack) {
	_title = STR("WorkerStatusScreen.title");

	ProgressScreen::show(ctx, goBack);
}

void WorkerStatusScreen::update(ui::Context &ctx) {
	auto &worker    = APP->_workerStatus;
	auto nextScreen = worker.nextScreen;

	if ((worker.status == WORKER_NEXT) || (worker.status == WORKER_NEXT_BACK)) {
		worker.reset();
		ctx.show(*nextScreen, worker.status == WORKER_NEXT_BACK);

		LOG("worker finished, next=0x%08x", nextScreen);
		return;
	}

	_setProgress(ctx, worker.progress, worker.progressTotal);
	_body = worker.message;
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
