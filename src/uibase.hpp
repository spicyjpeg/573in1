
#pragma once

#include <stdint.h>
#include "gpu.hpp"
#include "spu.hpp"
#include "util.hpp"

namespace ui {

/* Public constants */

enum Color : gpu::Color {
	COLOR_DEFAULT    = 0x808080,
	COLOR_SHADOW     = 0x000000,
	COLOR_BACKDROP   = 0xb0b0b0,
	COLOR_ACCENT1    = 0x50d0f0,
	COLOR_ACCENT2    = 0x3088a0,
	COLOR_WINDOW1    = 0x505050,
	COLOR_WINDOW2    = 0x242424,
	COLOR_WINDOW3    = 0x080808,
	COLOR_HIGHLIGHT1 = 0x40b0c8,
	COLOR_HIGHLIGHT2 = 0x3088a0,
	COLOR_PROGRESS1  = 0x10c048,
	COLOR_PROGRESS2  = 0x007820,
	COLOR_BOX1       = 0x000000,
	COLOR_BOX2       = 0x282828,
	COLOR_TEXT1      = 0x707070,
	COLOR_TEXT2      = 0x383838,
	COLOR_TITLE      = 0x808080,
	COLOR_SUBTITLE   = 0x4078a0
};

static constexpr int NUM_UI_SOUNDS = 6;

enum Sound {
	SOUND_STARTUP = 0,
	SOUND_ERROR   = 1,
	SOUND_MOVE    = 2,
	SOUND_ENTER   = 3,
	SOUND_EXIT    = 4,
	SOUND_CLICK   = 5
};

enum AnimationSpeed {
	SPEED_FASTEST = 10,
	SPEED_FAST    = 15,
	SPEED_SLOW    = 20
};

static constexpr int SCREEN_MARGIN_X          = 16;
static constexpr int SCREEN_MARGIN_Y          = 20;
static constexpr int SCREEN_BLOCK_MARGIN      = 6;
static constexpr int SCREEN_PROMPT_HEIGHT     = 30;
static constexpr int SCREEN_PROMPT_HEIGHT_MIN = 10;

static constexpr int LIST_BOX_PADDING  = 4;
static constexpr int LIST_ITEM_PADDING = 2;

static constexpr int MODAL_WIDTH          = 256;
static constexpr int MODAL_HEIGHT_FULL    = 120;
static constexpr int MODAL_HEIGHT_REDUCED = 50;
static constexpr int MODAL_PADDING        = 5;

static constexpr int TITLE_BAR_HEIGHT  = 18;
static constexpr int TITLE_BAR_PADDING = 5;

static constexpr int BUTTON_HEIGHT  = 18;
static constexpr int BUTTON_SPACING = 3;
static constexpr int BUTTON_PADDING = 5;

static constexpr int PROGRESS_BAR_HEIGHT = 8;

static constexpr int SHADOW_OFFSET = 4;

/* Button state manager */

static constexpr int NUM_BUTTONS     = 4;
static constexpr int NUM_BUTTON_MAPS = 6;
static constexpr int REPEAT_DELAY    = 30;

enum Button {
	BTN_LEFT  = 0,
	BTN_RIGHT = 1,
	BTN_START = 2,
	BTN_DEBUG = 3
};

enum ButtonMap {
	MAP_JOYSTICK      = 0,
	MAP_DDR_CAB       = 1,
	MAP_DDR_SOLO_CAB  = 2,
	MAP_DM_CAB        = 3,
	MAP_DMX_CAB       = 4,
	MAP_SINGLE_BUTTON = 5  // Used when selecting button mapping
};

class ButtonState {
private:
	uint32_t _mappings[NUM_BUTTONS];
	uint8_t  _held, _prevHeld;
	uint8_t  _pressed, _released, _repeating;

	int _repeatTimer;

public:
	ButtonMap buttonMap;

	inline bool pressed(Button button) const {
		return _pressed & (1 << button);
	}
	inline bool pressedRepeating(Button button) const {
		return (_pressed | _repeating) & (1 << button);
	}
	inline bool released(Button button) const {
		return _released & (1 << button);
	}
	inline bool repeating(Button button) const {
		return _repeating & (1 << button);
	}
	inline bool held(Button button) const {
		return _held & (1 << button);
	}

	ButtonState(void);
	void update(void);
};

/* UI context */

class Layer;
class Screen;

class Context {
private:
	Screen *_screens[2];
	Layer  *_background, *_overlay;
	int    _currentScreen;

public:
	gpu::Context &gpuCtx;

	gpu::Font  font;
	spu::Sound sounds[NUM_UI_SOUNDS];

	ButtonState buttons;

	int  time;
	void *screenData; // Opaque, can be accessed by screens

	inline void tick(void) {
		time++;
	}
	inline void setBackgroundLayer(Layer &layer) {
		_background = &layer;
	}
	inline void setOverlayLayer(Layer &layer) {
		_overlay = &layer;
	}

	Context(gpu::Context &gpuCtx, void *screenData = nullptr);
	void show(Screen &screen, bool goBack = false, bool playSound = false);
	void draw(void);
	void update(void);
};

/* Layer classes */

class Layer {
protected:
	void _newLayer(Context &ctx, int x, int y, int width, int height) const;
	void _setTexturePage(Context &ctx, uint16_t texpage, bool dither = false) const;
	void _setBlendMode(Context &ctx, gpu::BlendMode blendMode, bool dither = false) const;

public:
	virtual void draw(Context &ctx) const {}
	virtual void update(Context &ctx) {}
};

class TiledBackground : public Layer {
public:
	gpu::Image tile;
	const char *text;

	void draw(Context &ctx) const;
};

class LogOverlay : public Layer {
private:
	util::Logger &_logger;
	util::Tween<int, util::QuadOutEasing> _slideAnim;

public:
	LogOverlay(util::Logger &logger);
	void draw(Context &ctx) const;
	void update(Context &ctx);
};

/* Base screen classes */

// This is probably the most stripped-down way to implement something that
// vaguely resembles MVC. The class is the model, draw() is the view, update()
// is the controller.
class Screen : public Layer {
public:
	virtual void show(Context &ctx, bool goBack = false) {}
	virtual void hide(Context &ctx, bool goBack = false) {}
	virtual void draw(Context &ctx, bool active = true) const {}
	virtual void update(Context &ctx) {}
};

class AnimatedScreen : public Screen {
private:
	util::Tween<int, util::QuadOutEasing> _slideAnim;

protected:
	void _newLayer(Context &ctx, int x, int y, int width, int height) const;

public:
	virtual void show(Context &ctx, bool goBack = false);
	virtual void hide(Context &ctx, bool goBack = false);
};

class BackdropScreen : public Screen {
private:
	util::Tween<int, util::LinearEasing> _backdropAnim;

public:
	virtual void show(Context &ctx, bool goBack = false);
	virtual void hide(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
};

class ModalScreen : public BackdropScreen {
private:
	util::Tween<int, util::QuadOutEasing> _titleBarAnim;

protected:
	int _width, _height;

	const char *_title, *_body;

public:
	ModalScreen(int width, int height);
	virtual void show(Context &ctx, bool goBack = false);
	virtual void draw(Context &ctx, bool active = true) const;
};

}
