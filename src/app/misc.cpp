
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

static const util::Hash _MESSAGE_TITLES[]{
	"MessageScreen.title.success"_h,
	"MessageScreen.title.error"_h
};

void MessageScreen::setMessage(
	MessageType type, ui::Screen &prev, const char *format, ...
) {
	_type       = type;
	_prevScreen = &prev;

	va_list ap;

	va_start(ap, format);
	vsnprintf(_bodyText, sizeof(_bodyText), format, ap);
	va_end(ap);
}

void MessageScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STRH(_MESSAGE_TITLES[_type]);
	_body       = _bodyText;
	_buttons[0] = STR("MessageScreen.ok");

	_numButtons = 1;
	_locked     = _prevScreen ? false : true;

	MessageBoxScreen::show(ctx, goBack);
	ctx.sounds[ui::SOUND_ALERT].play();
}

void MessageScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

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

	MessageBoxScreen::show(ctx, goBack);
	//ctx.sounds[ui::SOUND_ALERT].play();
}

void ConfirmScreen::update(ui::Context &ctx) {
	MessageBoxScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (_activeButton)
			_callback(ctx);
		else
			ctx.show(*_prevScreen, true, true);
	}
}
