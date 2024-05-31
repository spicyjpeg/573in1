
#include "common/gpu.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/app/tests.hpp"
#include "main/uibase.hpp"
#include "main/uicommon.hpp"
#include "tests.hpp"

/* Top-level test menu */

struct TestMenuEntry {
public:
	util::Hash name, prompt;
	void       (TestMenuScreen::*target)(ui::Context &ctx);
};

static const TestMenuEntry _TEST_MENU_ENTRIES[]{
	{
		.name   = "TestMenuScreen.jammaTest.name"_h,
		.prompt = "TestMenuScreen.jammaTest.prompt"_h,
		.target = &TestMenuScreen::jammaTest
	}, {
		.name   = "TestMenuScreen.audioTest.name"_h,
		.prompt = "TestMenuScreen.audioTest.prompt"_h,
		.target = &TestMenuScreen::audioTest
	}, {
		.name   = "TestMenuScreen.colorIntensity.name"_h,
		.prompt = "TestMenuScreen.colorIntensity.prompt"_h,
		.target = &TestMenuScreen::colorIntensity
	}, {
		.name   = "TestMenuScreen.geometry.name"_h,
		.prompt = "TestMenuScreen.geometry.prompt"_h,
		.target = &TestMenuScreen::geometry
	}
};

const char *TestMenuScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_TEST_MENU_ENTRIES[index].name);
}

void TestMenuScreen::jammaTest(ui::Context &ctx) {
	ctx.show(APP->_jammaTestScreen, false, true);
}

void TestMenuScreen::audioTest(ui::Context &ctx) {
	ctx.show(APP->_audioTestScreen, false, true);
}

void TestMenuScreen::colorIntensity(ui::Context &ctx) {
	ctx.show(APP->_colorIntensityScreen, false, true);
}

void TestMenuScreen::geometry(ui::Context &ctx) {
	ctx.show(APP->_geometryScreen, false, true);
}

void TestMenuScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("TestMenuScreen.title");
	_prompt     = STRH(_TEST_MENU_ENTRIES[0].prompt);
	_itemPrompt = STR("TestMenuScreen.itemPrompt");

	_listLength = util::countOf(_TEST_MENU_ENTRIES);

	ListScreen::show(ctx, goBack);
}

void TestMenuScreen::update(ui::Context &ctx) {
	auto &entry = _TEST_MENU_ENTRIES[_activeItem];
	_prompt     = STRH(entry.prompt);

	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT))
			ctx.show(APP->_mainMenuScreen, true, true);
		else
			(this->*entry.target)(ctx);
	}
}

/* Test submenus */

static const util::Hash _JAMMA_INPUT_NAMES[]{
	"JAMMATestScreen.p2.left"_h,    // io::JAMMA_P2_LEFT
	"JAMMATestScreen.p2.right"_h,   // io::JAMMA_P2_RIGHT
	"JAMMATestScreen.p2.up"_h,      // io::JAMMA_P2_UP
	"JAMMATestScreen.p2.down"_h,    // io::JAMMA_P2_DOWN
	"JAMMATestScreen.p2.button1"_h, // io::JAMMA_P2_BUTTON1
	"JAMMATestScreen.p2.button2"_h, // io::JAMMA_P2_BUTTON2
	"JAMMATestScreen.p2.button3"_h, // io::JAMMA_P2_BUTTON3
	"JAMMATestScreen.p2.start"_h,   // io::JAMMA_P2_START
	"JAMMATestScreen.p1.left"_h,    // io::JAMMA_P1_LEFT
	"JAMMATestScreen.p1.right"_h,   // io::JAMMA_P1_RIGHT
	"JAMMATestScreen.p1.up"_h,      // io::JAMMA_P1_UP
	"JAMMATestScreen.p1.down"_h,    // io::JAMMA_P1_DOWN
	"JAMMATestScreen.p1.button1"_h, // io::JAMMA_P1_BUTTON1
	"JAMMATestScreen.p1.button2"_h, // io::JAMMA_P1_BUTTON2
	"JAMMATestScreen.p1.button3"_h, // io::JAMMA_P1_BUTTON3
	"JAMMATestScreen.p1.start"_h,   // io::JAMMA_P1_START
	"JAMMATestScreen.p1.button4"_h, // io::JAMMA_P1_BUTTON4
	"JAMMATestScreen.p1.button5"_h, // io::JAMMA_P1_BUTTON5
	"JAMMATestScreen.test"_h,       // io::JAMMA_TEST
	"JAMMATestScreen.p1.button6"_h, // io::JAMMA_P1_BUTTON6
	"JAMMATestScreen.p2.button4"_h, // io::JAMMA_P2_BUTTON4
	"JAMMATestScreen.p2.button5"_h, // io::JAMMA_P2_BUTTON5
	0,                              // io::JAMMA_RAM_LAYOUT
	"JAMMATestScreen.p2.button6"_h, // io::JAMMA_P2_BUTTON6
	"JAMMATestScreen.coin1"_h,      // io::JAMMA_COIN1
	"JAMMATestScreen.coin2"_h,      // io::JAMMA_COIN2
	0,                              // io::JAMMA_PCMCIA_CD1
	0,                              // io::JAMMA_PCMCIA_CD2
	"JAMMATestScreen.service"_h     // io::JAMMA_SERVICE
};

#define _PRINT(...) (ptr += snprintf(ptr, end - ptr __VA_OPT__(,) __VA_ARGS__))
#define _PRINTLN()  (*(ptr++) = '\n')

void JAMMATestScreen::show(ui::Context &ctx, bool goBack) {
	_title  = STR("JAMMATestScreen.title");
	_body   = _bodyText;
	_prompt = STR("JAMMATestScreen.prompt");

	_bodyText[0] = 0;

	TextScreen::show(ctx, goBack);
}

void JAMMATestScreen::update(ui::Context &ctx) {
	char *ptr = _bodyText, *end = &_bodyText[sizeof(_bodyText)];

	auto inputs = io::getJAMMAInputs();

	if (inputs) {
		_PRINT(STR("JAMMATestScreen.inputs"));

		for (auto name : _JAMMA_INPUT_NAMES) {
			if ((inputs & 1) && name)
				_PRINT(STRH(name));

			inputs >>= 1;
		}

		_PRINT(STR("JAMMATestScreen.inputsNote"));
	} else {
		_PRINT(STR("JAMMATestScreen.noInputs"));
	}

	*(--ptr) = 0;
	//LOG("remaining=%d", end - ptr);

	//TextScreen::update(ctx);

	if (ctx.buttons.longPressed(ui::BTN_START))
		ctx.show(APP->_testMenuScreen, true, true);
}

struct AudioTestEntry {
public:
	util::Hash name;
	void       (AudioTestScreen::*target)(ui::Context &ctx);
};

static const AudioTestEntry _AUDIO_TEST_ENTRIES[]{
	{
		.name   = "AudioTestScreen.playLeft"_h,
		.target = &AudioTestScreen::playLeft
	}, {
		.name   = "AudioTestScreen.playRight"_h,
		.target = &AudioTestScreen::playRight
	}, {
		.name   = "AudioTestScreen.playBoth"_h,
		.target = &AudioTestScreen::playBoth
	}, {
		.name   = "AudioTestScreen.enableAmp"_h,
		.target = &AudioTestScreen::enableAmp
	}, {
		.name   = "AudioTestScreen.disableAmp"_h,
		.target = &AudioTestScreen::disableAmp
	}, {
		.name   = "AudioTestScreen.enableCDDA"_h,
		.target = &AudioTestScreen::enableCDDA
	}, {
		.name   = "AudioTestScreen.disableCDDA"_h,
		.target = &AudioTestScreen::disableCDDA
	}
};

const char *AudioTestScreen::_getItemName(ui::Context &ctx, int index) const {
	return STRH(_AUDIO_TEST_ENTRIES[index].name);
}

void AudioTestScreen::playLeft(ui::Context &ctx) {
	ctx.sounds[ui::SOUND_STARTUP].play(spu::MAX_VOLUME, 0);
}

void AudioTestScreen::playRight(ui::Context &ctx) {
	ctx.sounds[ui::SOUND_STARTUP].play(0, spu::MAX_VOLUME);
}

void AudioTestScreen::playBoth(ui::Context &ctx) {
	ctx.sounds[ui::SOUND_STARTUP].play();
}

void AudioTestScreen::enableAmp(ui::Context &ctx) {
	io::setMiscOutput(io::MISC_AMP_ENABLE, true);
}

void AudioTestScreen::disableAmp(ui::Context &ctx) {
	io::setMiscOutput(io::MISC_AMP_ENABLE, false);
}

void AudioTestScreen::enableCDDA(ui::Context &ctx) {
	io::setMiscOutput(io::MISC_CDDA_ENABLE, true);
}

void AudioTestScreen::disableCDDA(ui::Context &ctx) {
	io::setMiscOutput(io::MISC_CDDA_ENABLE, false);
}

void AudioTestScreen::show(ui::Context &ctx, bool goBack) {
	_title      = STR("AudioTestScreen.title");
	_prompt     = STR("AudioTestScreen.prompt");
	_itemPrompt = STR("AudioTestScreen.itemPrompt");

	_listLength = util::countOf(_AUDIO_TEST_ENTRIES);

	ListScreen::show(ctx, goBack);
}

void AudioTestScreen::update(ui::Context &ctx) {
	ListScreen::update(ctx);

	if (ctx.buttons.pressed(ui::BTN_START)) {
		if (ctx.buttons.held(ui::BTN_LEFT) || ctx.buttons.held(ui::BTN_RIGHT)) {
			io::setMiscOutput(io::MISC_AMP_ENABLE,  false);
			io::setMiscOutput(io::MISC_CDDA_ENABLE, false);

			ctx.show(APP->_testMenuScreen, true, true);
		} else {
			(this->*_AUDIO_TEST_ENTRIES[_activeItem].target)(ctx);
		}
	}
}

/* Base test pattern screen class */

static constexpr gpu::Color _BACKGROUND_COLOR = 0x000000;
static constexpr gpu::Color _FOREGROUND_COLOR = 0xffffff;

void TestPatternScreen::_drawTextOverlay(
	ui::Context &ctx, const char *title, const char *prompt
) const {
	int screenWidth  = ctx.gpuCtx.width  - ui::SCREEN_MARGIN_X * 2;
	int screenHeight = ctx.gpuCtx.height - ui::SCREEN_MARGIN_Y * 2;

	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);

	gpu::RectWH backdropRect;

	backdropRect.x = ui::SCREEN_MARGIN_X - ui::SHADOW_OFFSET;
	backdropRect.y = ui::SCREEN_MARGIN_Y - ui::SHADOW_OFFSET;
	backdropRect.w = ui::SHADOW_OFFSET * 2 + screenWidth;
	backdropRect.h = ui::SHADOW_OFFSET * 2 + ctx.font.metrics.lineHeight;
	ctx.gpuCtx.drawRect(backdropRect, ctx.colors[ui::COLOR_SHADOW], true);

	backdropRect.y += screenHeight - ui::SCREEN_PROMPT_HEIGHT_MIN;
	ctx.gpuCtx.drawRect(backdropRect, ctx.colors[ui::COLOR_SHADOW], true);

	gpu::Rect textRect;

	textRect.x1 = ui::SCREEN_MARGIN_X;
	textRect.y1 = ui::SCREEN_MARGIN_Y;
	textRect.x2 = textRect.x1 + screenWidth;
	textRect.y2 = textRect.y1 + ctx.font.metrics.lineHeight;
	ctx.font.draw(ctx.gpuCtx, title, textRect, ctx.colors[ui::COLOR_TITLE]);

	textRect.y1 += screenHeight - ui::SCREEN_PROMPT_HEIGHT_MIN;
	textRect.y2 += textRect.y1;
	ctx.font.draw(
		ctx.gpuCtx, prompt, textRect, ctx.colors[ui::COLOR_TEXT1], true
	);
}

void TestPatternScreen::draw(ui::Context &ctx, bool active) const {
	_newLayer(ctx, 0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height);
	ctx.gpuCtx.drawRect(
		0, 0, ctx.gpuCtx.width, ctx.gpuCtx.height, _BACKGROUND_COLOR
	);
}

void TestPatternScreen::update(ui::Context &ctx) {
	if (ctx.buttons.pressed(ui::BTN_START))
		ctx.show(APP->_testMenuScreen, true, true);
}

/* Color intensity test screen */

struct IntensityBar {
public:
	util::Hash name;
	gpu::Color color;
};

static constexpr int _INTENSITY_BAR_NAME_WIDTH = 32;
static constexpr int _INTENSITY_BAR_WIDTH      = 256;
static constexpr int _INTENSITY_BAR_HEIGHT     = 32;

static const IntensityBar _INTENSITY_BARS[]{
	{
		.name  = "ColorIntensityScreen.white"_h,
		.color = 0xffffff
	}, {
		.name  = "ColorIntensityScreen.red"_h,
		.color = 0x0000ff
	}, {
		.name  = "ColorIntensityScreen.green"_h,
		.color = 0x00ff00
	}, {
		.name  = "ColorIntensityScreen.blue"_h,
		.color = 0xff0000
	}
};

void ColorIntensityScreen::draw(ui::Context &ctx, bool active) const {
	TestPatternScreen::draw(ctx, active);

	int barWidth  = _INTENSITY_BAR_NAME_WIDTH + _INTENSITY_BAR_WIDTH;
	int barHeight = _INTENSITY_BAR_HEIGHT * util::countOf(_INTENSITY_BARS);
	int offsetX   = (ctx.gpuCtx.width  - barWidth)  / 2;
	int offsetY   = (ctx.gpuCtx.height - barHeight) / 2;

	gpu::RectWH textRect, barRect;

	textRect.x = offsetX;
	textRect.y =
		offsetY + (_INTENSITY_BAR_HEIGHT - ctx.font.metrics.lineHeight) / 2;
	textRect.w = _INTENSITY_BAR_NAME_WIDTH;
	textRect.h = ctx.font.metrics.lineHeight;

	barRect.x = offsetX + _INTENSITY_BAR_NAME_WIDTH;
	barRect.y = offsetY;
	barRect.w = _INTENSITY_BAR_WIDTH;
	barRect.h = _INTENSITY_BAR_HEIGHT / 2;

	for (auto &bar : _INTENSITY_BARS) {
		ctx.font.draw(
			ctx.gpuCtx, STRH(bar.name), textRect, ctx.colors[ui::COLOR_TEXT1]
		);
		textRect.y += _INTENSITY_BAR_HEIGHT;

		ctx.gpuCtx.setTexturePage(0, false);
		ctx.gpuCtx.drawGradientRectH(barRect, _BACKGROUND_COLOR, bar.color);
		barRect.y += _INTENSITY_BAR_HEIGHT / 2;

		ctx.gpuCtx.setTexturePage(0, true);
		ctx.gpuCtx.drawGradientRectH(barRect, _BACKGROUND_COLOR, bar.color);
		barRect.y += _INTENSITY_BAR_HEIGHT / 2;
	}

	char value[2]{ 0, 0 };

	textRect.x = barRect.x + 1;
	textRect.y = offsetY - ctx.font.metrics.lineHeight;
	textRect.w = _INTENSITY_BAR_WIDTH / 32;

	for (int i = 0; i < 32; i++, textRect.x += textRect.w) {
		value[0] = util::HEX_CHARSET[i & 15];
		ctx.font.draw(ctx.gpuCtx, value, textRect, ctx.colors[ui::COLOR_TEXT2]);
	}

	_drawTextOverlay(
		ctx, STR("ColorIntensityScreen.title"),
		STR("ColorIntensityScreen.prompt")
	);
}

/* Geometry test screen */

static constexpr int _GRID_CELL_SIZE = 16;

void GeometryScreen::draw(ui::Context &ctx, bool active) const {
	TestPatternScreen::draw(ctx, active);

	for (int x = -1; x < ctx.gpuCtx.width; x += _GRID_CELL_SIZE)
		ctx.gpuCtx.drawRect(
			x, 0, 2, ctx.gpuCtx.height, ctx.colors[ui::COLOR_TEXT1]
		);
	for (int y = -1; y < ctx.gpuCtx.height; y += _GRID_CELL_SIZE)
		ctx.gpuCtx.drawRect(
			0, y, ctx.gpuCtx.width, 2, ctx.colors[ui::COLOR_TEXT1]
		);

	int offset       = (_GRID_CELL_SIZE / 2) - 1;
	int rightOffset  = ctx.gpuCtx.width  - (offset + 2);
	int bottomOffset = ctx.gpuCtx.height - (offset + 2);

	for (int x = offset; x <= rightOffset; x += _GRID_CELL_SIZE) {
		for (int y = offset; y <= bottomOffset; y += _GRID_CELL_SIZE) {
			auto color = (
				(x == offset) || (y == offset) || (x == rightOffset) ||
				(y == bottomOffset)
			)
				? ctx.colors[ui::COLOR_ACCENT1]
				: _FOREGROUND_COLOR;

			ctx.gpuCtx.drawRect(x, y, 2, 2, color);
		}
	}

	_drawTextOverlay(
		ctx, STR("GeometryScreen.title"), STR("GeometryScreen.prompt")
	);
}
