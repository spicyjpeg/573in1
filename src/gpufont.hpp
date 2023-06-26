
#pragma once

#include <stdint.h>
#include "gpu.hpp"

namespace gpu {

/* Font class */

static constexpr int FONT_CHAR_OFFSET = ' ';
static constexpr int FONT_CHAR_COUNT  = 120;
static constexpr int FONT_SPACE_WIDTH = 4;
static constexpr int FONT_TAB_WIDTH   = 32;
static constexpr int FONT_LINE_HEIGHT = 10;

class Font {
public:
	Image    image;
	uint32_t metrics[FONT_CHAR_COUNT];

	void draw(
		Context &ctx, const char *str, const Rect &rect, const Rect &clipRect,
		Color color = 0x808080, bool wordWrap = false
	) const;
	void draw(
		Context &ctx, const char *str, const Rect &rect, Color color = 0x808080,
		bool wordWrap = false
	) const;
	void draw(
		Context &ctx, const char *str, const RectWH &rect,
		Color color = 0x808080, bool wordWrap = false
	) const;
	int getCharacterWidth(char ch) const;
	void getStringBounds(
		const char *str, Rect &rect, bool wordWrap = false,
		bool breakOnSpace = false
	) const;
	int getStringWidth(const char *str, bool breakOnSpace = false) const;
	int getStringHeight(
		const char *str, int width, bool wordWrap = false,
		bool breakOnSpace = false
	) const;
};

}
