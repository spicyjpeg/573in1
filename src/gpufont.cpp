
#include <stdint.h>
#include "ps1/gpucmd.h"
#include "gpu.hpp"
#include "gpufont.hpp"
#include "util.hpp"

namespace gpu {

/* Font class */

void Font::draw(
	Context &ctx, const char *str, const Rect &rect, const Rect &clipRect,
	Color color, bool wordWrap
) const {
	// This is required for non-ASCII characters to work properly.
	auto _str = reinterpret_cast<const uint8_t *>(str);

	if (!str)
		return;

	ctx.setTexturePage(image.texpage);

	int x = rect.x1, y = rect.y1;

	for (uint8_t ch = *_str; ch; ch = *(++_str)) {
		bool wrap = wordWrap;

		switch (ch) {
			case '\t':
				x += metrics.tabWidth - 1;
				x -= x % metrics.tabWidth;
				break;

			case '\n':
				x  = rect.x1;
				y += metrics.lineHeight;
				break;

			case '\r':
				x = rect.x1;
				break;

			case ' ':
				x += metrics.spaceWidth;
				break;

			default:
				uint32_t size = metrics.getCharacterSize(ch);

				int u = size & 0xff; size >>= 8;
				int v = size & 0xff; size >>= 8;
				int w = size & 0x7f; size >>= 7;
				int h = size & 0x7f; size >>= 7;

				if (y > clipRect.y2)
					return;
				if (
					(x >= (clipRect.x1 - w)) && (x <= clipRect.x2) &&
					(y >= (clipRect.y1 - h))
				) {
					auto cmd = ctx.newPacket(4);

					cmd[0] = color | gp0_rectangle(true, size, true);
					cmd[1] = gp0_xy(x, y);
					cmd[2] = gp0_uv(u + image.u, v + image.v, image.palette);
					cmd[3] = gp0_xy(w, h);
				}

				x   += w;
				wrap = false;
		}

		// Handle word wrapping by calculating the length of the next word and
		// checking if it can still fit in the current line.
		int boundaryX = rect.x2;
		if (wrap)
			boundaryX -= getStringWidth(
				reinterpret_cast<const char *>(&_str[1]), true
			);

		if (x > boundaryX) {
			x  = rect.x1;
			y += metrics.lineHeight;
		}
		if (y > (rect.y2 - metrics.lineHeight))
			return;
	}
}

void Font::draw(
	Context &ctx, const char *str, const Rect &rect, Color color, bool wordWrap
) const {
	draw(ctx, str, rect, rect, color, wordWrap);
}

void Font::draw(
	Context &ctx, const char *str, const RectWH &rect, Color color,
	bool wordWrap
) const {
	Rect _rect{
		.x1 = rect.x,
		.y1 = rect.y,
		.x2 = int16_t(rect.x + rect.w),
		.y2 = int16_t(rect.y + rect.h)
	};

	draw(ctx, str, _rect, color, wordWrap);
}

int Font::getCharacterWidth(char ch) const {
	switch (ch) {
		case 0:
		case '\n':
		case '\r':
			return 0;

		case '\t':
			return metrics.tabWidth;

		case ' ':
			return metrics.spaceWidth;

		default:
			return (metrics.getCharacterSize(ch) >> 16) & 0x7f;
	}
}

void Font::getStringBounds(
	const char *str, Rect &rect, bool wordWrap, bool breakOnSpace
) const {
	auto _str = reinterpret_cast<const uint8_t *>(str);

	if (!str)
		return;

	int x = rect.x1, maxX = rect.x1, y = rect.y1;

	for (uint8_t ch = *_str; ch; ch = *(++_str)) {
		bool wrap = wordWrap;

		switch (ch) {
			case '\t':
				if (breakOnSpace)
					goto _break;

				x += metrics.tabWidth - 1;
				x -= x % metrics.tabWidth;
				break;

			case '\n':
				if (breakOnSpace)
					goto _break;
				if (x > maxX)
					maxX = x;

				x  = rect.x1;
				y += metrics.lineHeight;
				break;

			case '\r':
				if (breakOnSpace)
					goto _break;
				if (x > maxX)
					maxX = x;

				x = rect.x1;
				break;

			case ' ':
				if (breakOnSpace)
					goto _break;

				x += metrics.spaceWidth;
				break;

			default:
				x   += (metrics.getCharacterSize(ch) >> 16) & 0x7f;
				wrap = false;
		}

		int boundaryX = rect.x2;
		if (wrap)
			boundaryX -= getStringWidth(
				reinterpret_cast<const char *>(&_str[1]), true
			);

		if (x > boundaryX) {
			if (x > maxX)
				maxX = x;

			x  = rect.x1;
			y += metrics.lineHeight;
		}
		if (y > (rect.y2 - metrics.lineHeight))
			goto _break;
	}

_break:
	rect.x2 = maxX;
	rect.y2 = y + metrics.lineHeight;
}

int Font::getStringWidth(const char *str, bool breakOnSpace) const {
	auto _str = reinterpret_cast<const uint8_t *>(str);
	if (!str)
		return 0;

	int width = 0, maxWidth = 0;

	for (uint8_t ch = *_str; ch; ch = *(++_str)) {
		switch (ch) {
			case '\t':
				if (breakOnSpace)
					goto _break;

				width += metrics.tabWidth - 1;
				width -= width % metrics.tabWidth;
				break;

			case '\n':
			case '\r':
				if (breakOnSpace)
					goto _break;
				if (width > maxWidth)
					maxWidth = width;

				width = 0;
				break;

			case ' ':
				if (breakOnSpace)
					goto _break;

				width += metrics.spaceWidth;
				break;

			default:
				width += (metrics.getCharacterSize(ch) >> 16) & 0x7f;
		}
	}

_break:
	return (width > maxWidth) ? width : maxWidth;
}

int Font::getStringHeight(
	const char *str, int width, bool wordWrap, bool breakOnSpace
) const {
	Rect _rect{
		.x1 = 0,
		.y1 = 0,
		.x2 = int16_t(width),
		.y2 = 0x7fff
	};

	getStringBounds(str, _rect, wordWrap, breakOnSpace);
	return _rect.y2;
}

}
