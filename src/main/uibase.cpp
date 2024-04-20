
#include <stdint.h>
#include "common/gpu.hpp"
#include "common/gpufont.hpp"
#include "common/io.hpp"
#include "common/pad.hpp"
#include "common/util.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"

namespace ui {

/* Button state manager */

static const uint32_t _BUTTON_MAPPINGS[NUM_BUTTON_MAPS][NUM_BUTTONS]{
	{ // MAP_JOYSTICK
		io::JAMMA_P1_LEFT  | io::JAMMA_P1_UP | io::JAMMA_P2_LEFT  | io::JAMMA_P2_UP,
		io::JAMMA_P1_RIGHT | io::JAMMA_P1_DOWN | io::JAMMA_P2_RIGHT | io::JAMMA_P2_DOWN,
		io::JAMMA_P1_START | io::JAMMA_P1_BUTTON1 | io::JAMMA_P2_START | io::JAMMA_P2_BUTTON1,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	},
	{ // MAP_DDR_CAB
		io::JAMMA_P1_BUTTON2 | io::JAMMA_P2_BUTTON2,
		io::JAMMA_P1_BUTTON3 | io::JAMMA_P2_BUTTON3,
		io::JAMMA_P1_START | io::JAMMA_P2_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	},
	{ // MAP_DDR_SOLO_CAB
		io::JAMMA_P1_BUTTON5,
		io::JAMMA_P2_BUTTON5,
		io::JAMMA_P1_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	},
	{ // MAP_DM_CAB
		io::JAMMA_P2_LEFT,
		io::JAMMA_P2_RIGHT,
		io::JAMMA_P1_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	},
	{ // MAP_DMX_CAB (more or less redundant with MAP_JOYSTICK)
		io::JAMMA_P1_UP | io::JAMMA_P2_UP,
		io::JAMMA_P1_DOWN | io::JAMMA_P2_DOWN,
		io::JAMMA_P1_START | io::JAMMA_P2_START,
		io::JAMMA_TEST | io::JAMMA_SERVICE
	},
	{ // MAP_SINGLE_BUTTON
		0,
		0,
		io::JAMMA_P1_START | io::JAMMA_P2_START | io::JAMMA_TEST | io::JAMMA_SERVICE,
		0
	}
};

ButtonState::ButtonState(void)
: _held(0), _prevHeld(0), _longHeld(0), _prevLongHeld(0), _pressed(0),
_released(0), _longPressed(0), _longReleased(0), _repeatTimer(0),
buttonMap(MAP_JOYSTICK) {}

uint8_t ButtonState::_getHeld(void) const {
	uint32_t inputs = io::getJAMMAInputs();
	uint8_t  held   = 0;
	auto     map    = _BUTTON_MAPPINGS[buttonMap];

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

	if (buttonMap == MAP_SINGLE_BUTTON) {
		_pressed  = 0;
		_released = 0;
		_longHeld = 0;

		// In single-button mode, interpret a short button press as the right
		// button and a long press as start.
		if (_held) {
			if (_repeatTimer == REPEAT_DELAY)
				_pressed |= 1 << BTN_START;

			_repeatTimer++;
		} else if (_prevHeld) {
			if (_repeatTimer >= REPEAT_DELAY)
				_released |= 1 << BTN_START;
			else
				_pressed |= 1 << BTN_RIGHT;

			_repeatTimer = 0;
		}
	} else {
		if (changed)
			_repeatTimer = 0;
		else if (_held)
			_repeatTimer++;

		_pressed  = (changed & _held) & ~_pressed;
		_released = (changed & _prevHeld) & ~_released;
		_longHeld = (_repeatTimer >= REPEAT_DELAY) ? _held : 0;
	}

	changed = _prevLongHeld ^ _longHeld;

	_longPressed  = (changed & _longHeld) & ~_longPressed;
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
	auto oldScreen = _screens[_currentScreen];

	if (oldScreen)
		oldScreen->hide(*this, goBack);

	_currentScreen ^= 1;
	_screens[_currentScreen] = &screen;

	if (playSound)
		sounds[goBack ? SOUND_EXIT : SOUND_ENTER].play();

	screen.show(*this, goBack);
}

void Context::draw(void) {
	auto oldScreen = _screens[_currentScreen ^ 1];
	auto newScreen = _screens[_currentScreen];

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

	for (auto layer : overlays) {
		if (layer)
			layer->update(*this);
	}

	if (_screens[_currentScreen])
		_screens[_currentScreen]->update(*this);
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
	gpu::RectWH rect;

	rect.y = ctx.gpuCtx.height - (8 + ctx.font.metrics.lineHeight);
	rect.h = ctx.font.metrics.lineHeight;

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

LogOverlay::LogOverlay(util::LogBuffer &buffer)
: _buffer(buffer) {
	_slideAnim.setValue(0);
}

void LogOverlay::draw(Context &ctx, bool active) const {
	int offset = _slideAnim.getValue(ctx.time);
	if (!offset)
		return;

	_newLayer(
		ctx, 0, offset - ctx.gpuCtx.height, ctx.gpuCtx.width,
		ctx.gpuCtx.height
	);
	ctx.gpuCtx.drawBackdrop(ctx.colors[COLOR_BACKDROP], GP0_BLEND_SUBTRACT);

	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
	int linesShown   = screenHeight / ctx.font.metrics.lineHeight;

	gpu::Rect rect;

	rect.x1 = SCREEN_MARGIN_X;
	rect.y1 = SCREEN_MARGIN_Y;
	rect.x2 = ctx.gpuCtx.width  - SCREEN_MARGIN_X;
	rect.y2 = SCREEN_MARGIN_Y + ctx.font.metrics.lineHeight;

	for (int i = linesShown - 1; i >= 0; i--) {
		ctx.font.draw(
			ctx.gpuCtx, _buffer.getLine(i), rect, ctx.colors[COLOR_TEXT1]
		);

		rect.y1  = rect.y2;
		rect.y2 += ctx.font.metrics.lineHeight;
	}
}

void LogOverlay::update(Context &ctx) {
	if (
		ctx.buttons.released(BTN_DEBUG) && !ctx.buttons.longReleased(BTN_DEBUG)
	) {
		bool shown = !_slideAnim.getTargetValue();

		_slideAnim.setValue(ctx.time, shown ? ctx.gpuCtx.height : 0, SPEED_SLOW);
		ctx.sounds[shown ? SOUND_ENTER : SOUND_EXIT].play();
	}
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

void ScreenshotOverlay::update(Context &ctx) {
	if (ctx.buttons.longPressed(BTN_DEBUG)) {
		if (callback(ctx)) {
			_flashAnim.setValue(ctx.time, 0xff, 0, SPEED_SLOW);
			ctx.sounds[ui::SOUND_SCREENSHOT].play();
		}
	}
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
	_backdropAnim.setValue(ctx.time, 0, 0x50, SPEED_FAST);
}

void BackdropScreen::hide(Context &ctx, bool goBack) {
	_backdropAnim.setValue(ctx.time, 0x50, 0, SPEED_FAST);
}

void BackdropScreen::draw(Context &ctx, bool active) const {
	int brightness = _backdropAnim.getValue(ctx.time);

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
		rect.y2 = TITLE_BAR_PADDING + ctx.font.metrics.lineHeight;
		//rect.y2 = TITLE_BAR_HEIGHT - TITLE_BAR_PADDING;
		ctx.font.draw(ctx.gpuCtx, _title, rect, ctx.colors[COLOR_TITLE]);

		rect.y1 = TITLE_BAR_HEIGHT + MODAL_PADDING;
		rect.y2 = _height - MODAL_PADDING;
		ctx.font.draw(ctx.gpuCtx, _body, rect, ctx.colors[COLOR_TEXT1], true);
	}
}

}
