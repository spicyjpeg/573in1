
#include <stdint.h>
#include "ps1/gpucmd.h"
#include "gpu.hpp"
#include "io.hpp"
#include "pad.hpp"
#include "uibase.hpp"

namespace ui {

/* Button state manager */

static const uint32_t _BUTTON_MAPPINGS[NUM_BUTTON_MAPS][NUM_BUTTONS]{
	{ // MAP_JOYSTICK
		io::JAMMA_P1_LEFT  | io::JAMMA_P1_UP | io::JAMMA_P2_LEFT  | io::JAMMA_P2_UP,
		io::JAMMA_P1_RIGHT | io::JAMMA_P1_DOWN | io::JAMMA_P2_RIGHT | io::JAMMA_P2_DOWN,
		io::JAMMA_P1_START | io::JAMMA_P2_START,
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
: _held(0), _repeatTimer(0) {}

void ButtonState::update(void) {
	_prevHeld = _held;
	_held     = 0;

	uint32_t inputs = io::getJAMMAInputs();
	auto     map    = _BUTTON_MAPPINGS[buttonMap];

	for (int i = 0; i < NUM_BUTTONS; i++) {
		if (inputs & map[i])
			_held |= 1 << i;
	}

#ifdef ENABLE_PS1_CONTROLLER
	if (pad::ports[0].pollPad() || pad::ports[1].pollPad()) {
		_held = 0; // Ignore JAMMA inputs

		for (int i = 1; i >= 0; i--) {
			auto &port = pad::ports[i];

			if (
				(port.padType != pad::PAD_DIGITAL) &&
				(port.padType != pad::PAD_ANALOG) &&
				(port.padType != pad::PAD_ANALOG_STICK)
			)
				continue;

			if (port.buttons & (pad::BTN_LEFT | pad::BTN_UP))
				_held |= 1 << BTN_LEFT;
			if (port.buttons & (pad::BTN_RIGHT | pad::BTN_DOWN))
				_held |= 1 << BTN_RIGHT;
			if (port.buttons & (pad::BTN_CIRCLE | pad::BTN_CROSS))
				_held |= 1 << BTN_START;
			if (port.buttons & pad::BTN_SELECT)
				_held |= 1 << BTN_DEBUG;
		}
	}
#endif

	uint32_t changed = _prevHeld ^ _held;

	if (buttonMap == MAP_SINGLE_BUTTON) {
		_pressed   = 0;
		_released  = 0;
		_repeating = 0;

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

		_pressed   = (changed & _held) & ~_pressed;
		_released  = (changed & _prevHeld) & ~_released;
		_repeating = (_repeatTimer >= REPEAT_DELAY) ? _held : 0;
	}
}

/* UI context */

Context::Context(gpu::Context &gpuCtx, void *screenData)
: _background(nullptr), _overlay(nullptr), _currentScreen(0), gpuCtx(gpuCtx),
time(0), screenData(screenData) {
	_screens[0] = nullptr;
	_screens[1] = nullptr;
}

void Context::show(Screen &screen, bool goBack, bool playSound) {
	auto oldScreen = _screens[_currentScreen];
	if (oldScreen)
		oldScreen->hide(*this, goBack);

	_currentScreen ^= 1;
	_screens[_currentScreen] = &screen;
	screen.show(*this, goBack);

	if (playSound)
		sounds[goBack ? SOUND_EXIT : SOUND_ENTER].play();
}

void Context::draw(void) {
	auto oldScreen = _screens[_currentScreen ^ 1];
	auto newScreen = _screens[_currentScreen];

	if (_background)
		_background->draw(*this);
	if (oldScreen)
		oldScreen->draw(*this, false);
	if (newScreen)
		newScreen->draw(*this, true);
	if (_overlay)
		_overlay->draw(*this);
}

void Context::update(void) {
	buttons.update();

	if (_overlay)
		_overlay->update(*this);
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

void TiledBackground::draw(Context &ctx) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	_setTexturePage(ctx, tile.texpage);

	int offsetX = uint32_t(ctx.time / 2) % tile.width;
	int offsetY = uint32_t(ctx.time / 3) % tile.height;

	for (int x = -offsetX; x < ctx.gpuCtx.width; x += tile.width) {
		for (int y = -offsetY; y < ctx.gpuCtx.height; y += tile.height)
			tile.draw(ctx.gpuCtx, x, y);
	}

	gpu::RectWH rect;

	int width = ctx.font.getStringWidth(text);
	rect.x    = ctx.gpuCtx.width  - (8 + width);
	rect.y    = ctx.gpuCtx.height - (8 + gpu::FONT_LINE_HEIGHT);
	rect.w    = width;
	rect.h    = gpu::FONT_LINE_HEIGHT;
	ctx.font.draw(ctx.gpuCtx, text, rect, COLOR_TEXT2);
}

LogOverlay::LogOverlay(util::Logger &logger)
: _logger(logger) {
	_slideAnim.setValue(0);
}

void LogOverlay::draw(Context &ctx) const {
	int offset = _slideAnim.getValue(ctx.time);
	if (!offset)
		return;

	_newLayer(
		ctx, 0, offset - ctx.gpuCtx.height, ctx.gpuCtx.width,
		ctx.gpuCtx.height
	);
	ctx.gpuCtx.drawBackdrop(COLOR_BACKDROP, GP0_BLEND_SUBTRACT);

	int screenHeight = ctx.gpuCtx.height - SCREEN_MARGIN_Y * 2;
	int linesShown   = screenHeight / gpu::FONT_LINE_HEIGHT;

	gpu::Rect rect;

	rect.x1 = SCREEN_MARGIN_X;
	rect.y1 = SCREEN_MARGIN_Y;
	rect.x2 = ctx.gpuCtx.width  - SCREEN_MARGIN_X;
	rect.y2 = SCREEN_MARGIN_Y + gpu::FONT_LINE_HEIGHT;

	for (int i = linesShown - 1; i >= 0; i--) {
		ctx.font.draw(ctx.gpuCtx, _logger.getLine(i), rect, COLOR_TEXT1);

		rect.y1  = rect.y2;
		rect.y2 += gpu::FONT_LINE_HEIGHT;
	}
}

void LogOverlay::update(Context &ctx) {
	if (ctx.buttons.pressed(BTN_DEBUG)) {
		bool shown = !_slideAnim.getTargetValue();

		_slideAnim.setValue(ctx.time, shown ? ctx.gpuCtx.height : 0, SPEED_SLOW);
		ctx.sounds[shown ? SOUND_ENTER : SOUND_EXIT].play();
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
			0, 0, _width, windowHeight, COLOR_WINDOW1, COLOR_WINDOW2,
			COLOR_WINDOW3
		);
		ctx.gpuCtx.drawGradientRectH(
			0, 0, _titleBarAnim.getValue(ctx.time), TITLE_BAR_HEIGHT,
			COLOR_ACCENT1, COLOR_ACCENT2
		);
		ctx.gpuCtx.drawRect(
			_width, SHADOW_OFFSET, SHADOW_OFFSET, windowHeight, COLOR_SHADOW,
			true
		);
		ctx.gpuCtx.drawRect(
			SHADOW_OFFSET, windowHeight, _width - SHADOW_OFFSET, SHADOW_OFFSET,
			COLOR_SHADOW, true
		);

		// Text
		gpu::Rect rect;

		rect.x1 = TITLE_BAR_PADDING;
		rect.y1 = TITLE_BAR_PADDING;
		rect.x2 = _width - TITLE_BAR_PADDING;
		rect.y2 = TITLE_BAR_HEIGHT - TITLE_BAR_PADDING;
		ctx.font.draw(ctx.gpuCtx, _title, rect, COLOR_TITLE);

		rect.y1 = TITLE_BAR_HEIGHT + MODAL_PADDING;
		rect.y2 = _height - MODAL_PADDING;
		ctx.font.draw(ctx.gpuCtx, _body, rect, COLOR_TEXT1, true);
	}
}

}
