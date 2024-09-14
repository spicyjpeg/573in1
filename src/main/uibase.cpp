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

#include <stdint.h>
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/util/tween.hpp"
#include "common/gpu.hpp"
#include "common/gpufont.hpp"
#include "common/io.hpp"
#include "common/pad.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

namespace ui {

/* Button state manager */

static const uint32_t _BUTTON_MAPPINGS[NUM_BUTTON_MAPS][NUM_BUTTONS]{
	{
		// MAP_JOYSTICK
		0
			| io::JAMMA_P1_LEFT
			| io::JAMMA_P2_LEFT
			| io::JAMMA_P1_UP
			| io::JAMMA_P2_UP,
		0
			| io::JAMMA_P1_RIGHT
			| io::JAMMA_P2_RIGHT
			| io::JAMMA_P1_DOWN
			| io::JAMMA_P2_DOWN,
		0
			| io::JAMMA_P1_START
			| io::JAMMA_P2_START
			| io::JAMMA_P1_BUTTON1
			| io::JAMMA_P2_BUTTON1,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	}, {
		// MAP_DDR_CAB
		io::JAMMA_P1_BUTTON2 | io::JAMMA_P2_BUTTON2,
		io::JAMMA_P1_BUTTON3 | io::JAMMA_P2_BUTTON3,
		io::JAMMA_P1_START   | io::JAMMA_P2_START,
		io::JAMMA_TEST       | io::JAMMA_SERVICE
	}, {
		// MAP_DDR_SOLO_CAB
		io::JAMMA_P1_BUTTON5,
		io::JAMMA_P2_BUTTON5,
		io::JAMMA_P1_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	}, {
		// MAP_DM_CAB
		io::JAMMA_P2_LEFT,
		io::JAMMA_P2_RIGHT,
		io::JAMMA_P1_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	}, {
		// MAP_DMX_CAB (more or less redundant with MAP_JOYSTICK)
		io::JAMMA_P1_UP    | io::JAMMA_P2_UP,
		io::JAMMA_P1_DOWN  | io::JAMMA_P2_DOWN,
		io::JAMMA_P1_START | io::JAMMA_P2_START,
		io::JAMMA_TEST     | io::JAMMA_SERVICE
	}, {
		// MAP_SINGLE_BUTTON
		0,
		0,
		0
			| io::JAMMA_P1_START
			| io::JAMMA_P2_START
			| io::JAMMA_TEST
			| io::JAMMA_SERVICE,
		0
	}
};

ButtonState::ButtonState(void)
: _buttonMap(MAP_JOYSTICK), _held(0), _prevHeld(0), _longHeld(0),
_prevLongHeld(0), _pressed(0), _released(0), _longPressed(0), _longReleased(0),
_repeatTimer(0) {}

uint8_t ButtonState::_getHeld(void) const {
	auto    inputs = io::getJAMMAInputs();
	auto    map    = _BUTTON_MAPPINGS[_buttonMap];
	uint8_t held   = 0;

#ifdef ENABLE_PS1_CONTROLLER
	if (pad::ports[0].pollPad() || pad::ports[1].pollPad()) {
		for (int i = 1; i >= 0; i--) {
			auto &port = pad::ports[i];

			if (
				(port.padType != pad::PAD_DIGITAL) &&
				(port.padType != pad::PAD_ANALOG) &&
				(port.padType != pad::PAD_ANALOG_STICK)
			)
				continue;

			if (port.buttons & (pad::BTN_LEFT | pad::BTN_UP))
				held |= 1 << BTN_LEFT;
			if (port.buttons & (pad::BTN_RIGHT | pad::BTN_DOWN))
				held |= 1 << BTN_RIGHT;
			if (port.buttons & (pad::BTN_CIRCLE | pad::BTN_CROSS))
				held |= 1 << BTN_START;
			if (port.buttons & pad::BTN_SELECT)
				held |= 1 << BTN_DEBUG;
		}

		return held; // Ignore JAMMA inputs
	}
#endif

	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (inputs & map[i])
			held |= 1 << i;
	}

	return held;
}

void ButtonState::reset(void) {
	_held         = _getHeld();
	_prevHeld     = _held;
	_longHeld     = 0;
	_prevLongHeld = 0;

	_pressed      = 0;
	_released     = 0;
	_longPressed  = 0;
	_longReleased = 0;
	_repeatTimer  = 0;
}

void ButtonState::update(void) {
	_prevHeld     = _held;
	_prevLongHeld = _longHeld;
	_held         = _getHeld();

	uint32_t changed = _prevHeld ^ _held;

	if (_buttonMap == MAP_SINGLE_BUTTON) {
		_pressed  = 0;
		_released = 0;
		_longHeld = 0;

		// In single-button mode, interpret a short button press as the right
		// button and a long press as start. Note that the repeat timer is not
		// started if single button mode is enabled while a button is held down.
		if (changed & _held) {
			_repeatTimer = 1;
		} else if (changed & _prevHeld) {
			if (_repeatTimer && (_repeatTimer < REPEAT_DELAY))
				_pressed  |= 1 << BTN_RIGHT;

			_repeatTimer = 0;
		} else if (_held && _repeatTimer) {
			if (_repeatTimer == REPEAT_DELAY)
				_pressed |= 1 << BTN_START;

			_repeatTimer++;
		}
	} else {
		if (changed & _held)
			_repeatTimer = 1;
		else if (changed & _prevHeld)
			_repeatTimer = 0;
		else if (_held && _repeatTimer)
			_repeatTimer++;

		_pressed  = (changed & _held)     & ~_pressed;
		_released = (changed & _prevHeld) & ~_released;
		_longHeld = (_repeatTimer >= REPEAT_DELAY) ? _held : 0;
	}

	changed = _prevLongHeld ^ _longHeld;

	_longPressed  = (changed & _longHeld)     & ~_longPressed;
	_longReleased = (changed & _prevLongHeld) & ~_longReleased;
}

/* UI context */

Context::Context(gpu::Context &gpuCtx, void *screenData)
: _currentScreen(0), gpuCtx(gpuCtx), time(0), screenData(screenData) {
	util::clear(_screens);
	util::clear(backgrounds);
	util::clear(overlays);
}

void Context::show(Screen &screen, bool goBack, bool playSound) {
	auto oldScreen = getCurrentScreen();

	if (oldScreen)
		oldScreen->hide(*this, goBack);

	_currentScreen ^= 1;
	_screens[_currentScreen] = &screen;

	if (playSound)
		sounds[goBack ? SOUND_EXIT : SOUND_ENTER].play();

	screen.show(*this, goBack);
}

void Context::draw(void) {
	auto oldScreen = getInactiveScreen();
	auto newScreen = getCurrentScreen();

	for (auto layer : backgrounds) {
		if (layer)
			layer->draw(*this);
	}

	if (oldScreen)
		oldScreen->draw(*this, false);
	if (newScreen)
		newScreen->draw(*this, true);

	for (auto layer : overlays) {
		if (layer)
			layer->draw(*this);
	}
}

void Context::update(void) {
	buttons.update();

	auto screen = getCurrentScreen();

	if (screen)
		screen->update(*this);
}

/* Layer classes */

void Layer::_newLayer(Context &ctx, int x, int y, int width, int height) const {
	ctx.gpuCtx.newLayer(x, y, width, height);
}

void Layer::_setTexturePage(Context &ctx, uint16_t texpage, bool dither) const {
	ctx.gpuCtx.setTexturePage(texpage, dither);
}

void Layer::_setBlendMode(
	Context &ctx, gpu::BlendMode blendMode, bool dither
) const {
	ctx.gpuCtx.setBlendMode(blendMode, dither);
}

void TiledBackground::draw(Context &ctx, bool active) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	_setTexturePage(ctx, tile.texpage);

	int offsetX = uint32_t(ctx.time / 2) % tile.width;
	int offsetY = uint32_t(ctx.time / 3) % tile.height;

	for (int x = -offsetX; x < ctx.gpuCtx.width; x += tile.width) {
		for (int y = -offsetY; y < ctx.gpuCtx.height; y += tile.height)
			tile.draw(ctx.gpuCtx, x, y);
	}
}

void TextOverlay::draw(Context &ctx, bool active) const {
	int lineHeight = ctx.font.getLineHeight();

	gpu::RectWH rect;

	rect.y = ctx.gpuCtx.height - (8 + lineHeight);
	rect.h = lineHeight;

	if (leftText) {
		rect.x = 8;
		rect.w = ctx.gpuCtx.width - 16;
		ctx.font.draw(ctx.gpuCtx, leftText, rect, ctx.colors[COLOR_TEXT2]);
	}
	if (rightText) {
		int width = ctx.font.getStringWidth(rightText);

		rect.x = ctx.gpuCtx.width - (8 + width);
		rect.w = width;
		ctx.font.draw(ctx.gpuCtx, rightText, rect, ctx.colors[COLOR_TEXT2]);
	}
}

void SplashOverlay::draw(Context &ctx, bool active) const {
	int brightness = _fadeAnim.getValue(ctx.time);

	if (!brightness)
		return;

	// Backdrop
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	ctx.gpuCtx.drawBackdrop(
		gp0_rgb(brightness, brightness, brightness), GP0_BLEND_SUBTRACT
	);

	if (brightness < 0xff)
		return;

	// Image
	int x = (ctx.gpuCtx.width  - image.width)  / 2;
	int y = (ctx.gpuCtx.height - image.height) / 2;

	image.draw(ctx.gpuCtx, x, y);
}

void SplashOverlay::show(Context &ctx) {
	if (!_fadeAnim.getTargetValue())
#if 0
		_fadeAnim.setValue(ctx.time, 0, 0xff, SPEED_SLOWEST);
#else
		_fadeAnim.setValue(0xff);
#endif
}

void SplashOverlay::hide(Context &ctx) {
	if (_fadeAnim.getTargetValue())
		_fadeAnim.setValue(ctx.time, 0xff, 0, SPEED_SLOWEST);
}

void LogOverlay::draw(Context &ctx, bool active) const {
	int offset = _slideAnim.getValue(ctx.time);

	if (!offset)
		return;

	// Backdrop
	_newLayer(
		ctx, 0, offset - ctx.gpuCtx.height, ctx.gpuCtx.width,
		ctx.gpuCtx.height
	);
	ctx.gpuCtx.drawBackdrop(ctx.colors[COLOR_BACKDROP], GP0_BLEND_SUBTRACT);

	// Text
	int screenHeight = ctx.gpuCtx.height - SCREEN_MIN_MARGIN_Y * 2;
	int lineHeight   = ctx.font.getLineHeight();

	gpu::Rect rect;

	rect.x1 = SCREEN_MIN_MARGIN_X;
	rect.y1 = SCREEN_MIN_MARGIN_Y;
	rect.x2 = ctx.gpuCtx.width  - SCREEN_MIN_MARGIN_X;
	rect.y2 = SCREEN_MIN_MARGIN_Y + lineHeight;

	for (int i = (screenHeight / lineHeight) - 1; i >= 0; i--) {
		ctx.font.draw(
			ctx.gpuCtx, _buffer.getLine(i), rect, ctx.colors[COLOR_TEXT1]
		);

		rect.y1  = rect.y2;
		rect.y2 += lineHeight;
	}
}

void LogOverlay::toggle(Context &ctx) {
	bool shown = !_slideAnim.getTargetValue();

	_slideAnim.setValue(ctx.time, shown ? ctx.gpuCtx.height : 0, SPEED_SLOW);
	ctx.sounds[shown ? SOUND_ENTER : SOUND_EXIT].play();
}

void ScreenshotOverlay::draw(Context &ctx, bool active) const {
	int brightness = _flashAnim.getValue(ctx.time);

	if (!brightness)
		return;

	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	ctx.gpuCtx.drawBackdrop(
		gp0_rgb(brightness, brightness, brightness), GP0_BLEND_ADD
	);
}

void ScreenshotOverlay::animate(Context &ctx) {
	_flashAnim.setValue(ctx.time, 0xff, 0, SPEED_SLOW);
	ctx.sounds[ui::SOUND_SCREENSHOT].play();
}

/* Base screen classes */

void AnimatedScreen::_newLayer(
	Context &ctx, int x, int y, int width, int height
) const {
	Screen::_newLayer(ctx, x + _slideAnim.getValue(ctx.time), y, width, height);
}

void AnimatedScreen::show(Context &ctx, bool goBack) {
	int width = ctx.gpuCtx.width;
	_slideAnim.setValue(ctx.time, goBack ? (-width) : width, 0, SPEED_SLOW);
}

void AnimatedScreen::hide(Context &ctx, bool goBack) {
	int width = ctx.gpuCtx.width;
	_slideAnim.setValue(ctx.time, 0, goBack ? width : (-width), SPEED_SLOW);
}

void BackdropScreen::show(Context &ctx, bool goBack) {
	if (!_fadeAnim.getTargetValue())
		_fadeAnim.setValue(ctx.time, 0, 0x50, SPEED_FAST);
}

void BackdropScreen::hide(Context &ctx, bool goBack) {
	if (_fadeAnim.getTargetValue())
		_fadeAnim.setValue(ctx.time, 0x50, 0, SPEED_FAST);
}

void BackdropScreen::draw(Context &ctx, bool active) const {
	int brightness = _fadeAnim.getValue(ctx.time);

	if (!brightness)
		return;

	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	ctx.gpuCtx.drawBackdrop(
		gp0_rgb(brightness, brightness, brightness), GP0_BLEND_ADD
	);
}

ModalScreen::ModalScreen(int width, int height)
: _width(width), _height(height), _title(nullptr), _body(nullptr) {}

void ModalScreen::show(Context &ctx, bool goBack) {
	BackdropScreen::show(ctx, goBack);

	_titleBarAnim.setValue(ctx.time, 0, _width, SPEED_SLOW);
}

void ModalScreen::draw(Context &ctx, bool active) const {
	BackdropScreen::draw(ctx, active);

	if (active) {
		int windowHeight = TITLE_BAR_HEIGHT + _height;

		_newLayer(
			ctx, (ctx.gpuCtx.width  - _width) / 2,
			(ctx.gpuCtx.height - windowHeight) / 2, _width + SHADOW_OFFSET,
			windowHeight + SHADOW_OFFSET
		);
		_setBlendMode(ctx, GP0_BLEND_SEMITRANS, true);

		// Window
		ctx.gpuCtx.drawGradientRectD(
			0, 0, _width, windowHeight, ctx.colors[COLOR_WINDOW1],
			ctx.colors[COLOR_WINDOW2], ctx.colors[COLOR_WINDOW3]
		);
		ctx.gpuCtx.drawGradientRectH(
			0, 0, _titleBarAnim.getValue(ctx.time), TITLE_BAR_HEIGHT,
			ctx.colors[COLOR_ACCENT1], ctx.colors[COLOR_ACCENT2]
		);
		ctx.gpuCtx.drawRect(
			_width, SHADOW_OFFSET, SHADOW_OFFSET, windowHeight,
			ctx.colors[COLOR_SHADOW], true
		);
		ctx.gpuCtx.drawRect(
			SHADOW_OFFSET, windowHeight, _width - SHADOW_OFFSET, SHADOW_OFFSET,
			ctx.colors[COLOR_SHADOW], true
		);

		// Text
		gpu::Rect rect;

		rect.x1 = TITLE_BAR_PADDING;
		rect.y1 = TITLE_BAR_PADDING;
		rect.x2 = _width - TITLE_BAR_PADDING;
		rect.y2 = TITLE_BAR_PADDING + ctx.font.getLineHeight();
		ctx.font.draw(ctx.gpuCtx, _title, rect, ctx.colors[COLOR_TITLE]);

		rect.y1 = TITLE_BAR_HEIGHT + MODAL_PADDING;
		rect.y2 = _height - MODAL_PADDING;
		ctx.font.draw(ctx.gpuCtx, _body, rect, ctx.colors[COLOR_TEXT1], true);
	}
}

}
