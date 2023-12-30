
#pragma once

#include <stdint.h>
#include "common/gpu.hpp"

namespace gpu {

/* Font class */

static constexpr char FONT_INVALID_CHAR = 0x7f;

class [[gnu::packed]] FontMetrics {
public:
	uint8_t  spaceWidth, tabWidth, lineHeight, _reserved;
	uint32_t characterSizes[256];

	inline uint32_t getCharacterSize(uint8_t ch) const {
		uint32_t sizes = characterSizes[ch];
		if (!sizes)
			return characterSizes[int(FONT_INVALID_CHAR)];

		return sizes;
	}
};

class Font {
public:
	Image       image;
	FontMetrics metrics;

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
