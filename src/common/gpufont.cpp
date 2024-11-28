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
#include "common/util/hash.hpp"
#include "common/util/string.hpp"
#include "common/gpu.hpp"
#include "common/gpufont.hpp"
#include "ps1/gpucmd.h"

namespace gpu {

/* Font metrics class */

CharacterSize FontMetrics::get(util::UTF8CodePoint id) const {
	if (!ptr)
		return 0;

	auto header = as<FontMetricsHeader>();
	auto entry  = util::getHashTableEntry(
		reinterpret_cast<const FontMetricsEntry *>(header + 1),
		header->numBuckets,
		id
	);

	if (entry)
		return entry->size;
	else if (id != FONT_INVALID_CHAR)
		return get(FONT_INVALID_CHAR);
	else
		return 0;
}

/* Font class */

void Font::draw(
	Context    &ctx,
	const char *str,
	const Rect &rect,
	const Rect &clipRect,
	Color      color,
	bool       wordWrap
) const {
	if (!str || !metrics.ptr)
		return;

	ctx.setTexturePage(image.texpage);

	auto header = metrics.as<FontMetricsHeader>();

	int x      = rect.x1;
	int clipX1 = clipRect.x1;
	int clipX2 = clipRect.x2;

	int y      = rect.y1     + header->baselineOffset;
	int clipY1 = clipRect.y1 + header->baselineOffset;
	int clipY2 = clipRect.y2 + header->baselineOffset;
	int rectY2 = rect.y2     + header->baselineOffset - header->lineHeight;

	for (;;) {
		auto ch   = util::parseUTF8Character(str);
		bool wrap = wordWrap;
		str      += ch.length;

		switch (ch.codePoint) {
			case 0:
				return;

			case '\t':
				x += header->tabWidth;
				x -= x % header->tabWidth;
				break;

			case '\n':
				x  = rect.x1;
				y += header->lineHeight;
				break;

			case '\r':
				x = rect.x1;
				break;

			case ' ':
				x += header->spaceWidth;
				break;

			default:
				auto size = metrics.get(ch.codePoint);

				int u = size & 0xff; size >>= 8;
				int v = size & 0xff; size >>= 8;
				int w = size & 0x7f; size >>= 7;
				int h = size & 0x7f; size >>= 7;

				if (y > clipY2)
					return;
				if (
					(x >= (clipX1 - w)) && (x <= clipX2) && (y >= (clipY1 - h))
				) {
					auto cmd = ctx.newPacket(4);

					cmd[0] = color | gp0_rectangle(true, size & 1, true);
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
			boundaryX -= getStringWidth(str, true);

		if (x > boundaryX) {
			x  = rect.x1;
			y += header->lineHeight;
		}
		if (y > rectY2)
			return;
	}
}

void Font::draw(
	Context    &ctx,
	const char *str,
	const Rect &rect,
	Color      color,
	bool       wordWrap
) const {
	draw(ctx, str, rect, rect, color, wordWrap);
}

void Font::draw(
	Context      &ctx,
	const char   *str,
	const RectWH &rect,
	Color        color,
	bool         wordWrap
) const {
	Rect _rect{
		.x1 = rect.x,
		.y1 = rect.y,
		.x2 = int16_t(rect.x + rect.w),
		.y2 = int16_t(rect.y + rect.h)
	};

	draw(ctx, str, _rect, color, wordWrap);
}

int Font::getCharacterWidth(util::UTF8CodePoint ch) const {
	auto header = metrics.as<FontMetricsHeader>();

	switch (ch) {
		case 0:
		case '\n':
		case '\r':
			return 0;

		case '\t':
			return header->tabWidth;

		case ' ':
			return header->spaceWidth;

		default:
			auto size = metrics.get(ch);

			return (size >> 16) & 0x7f;
	}
}

void Font::getStringBounds(
	const char *str,
	Rect       &rect,
	bool       wordWrap,
	bool       breakOnSpace
) const {
	if (!str || !metrics.ptr)
		return;

	auto header = metrics.as<FontMetricsHeader>();

	int x = rect.x1, maxX = rect.x1, y = rect.y1;

	for (;;) {
		auto ch   = util::parseUTF8Character(str);
		bool wrap = wordWrap;
		str      += ch.length;

		switch (ch.codePoint) {
			case 0:
				goto _break;

			case '\t':
				if (breakOnSpace)
					goto _break;

				x += header->tabWidth;
				x -= x % header->tabWidth;
				break;

			case '\n':
				if (breakOnSpace)
					goto _break;
				if (x > maxX)
					maxX = x;

				x  = rect.x1;
				y += header->lineHeight;
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

				x += header->spaceWidth;
				break;

			default:
				auto size = metrics.get(ch.codePoint);

				x   += (size >> 16) & 0x7f;
				wrap = false;
		}

		int boundaryX = rect.x2;

		if (wrap)
			boundaryX -= getStringWidth(str, true);

		if (x > boundaryX) {
			if (x > maxX)
				maxX = x;

			x  = rect.x1;
			y += header->lineHeight;
		}
		if (y > (rect.y2 - header->lineHeight))
			goto _break;
	}

_break:
	rect.x2 = maxX;
	rect.y2 = y + header->lineHeight;
}

int Font::getStringWidth(const char *str, bool breakOnSpace) const {
	if (!str || !metrics.ptr)
		return 0;

	auto header = metrics.as<FontMetricsHeader>();

	int width = 0, maxWidth = 0;

	for (;;) {
		auto ch = util::parseUTF8Character(str);
		str    += ch.length;

		switch (ch.codePoint) {
			case 0:
				goto _break;

			case '\t':
				if (breakOnSpace)
					goto _break;

				width += header->tabWidth;
				width -= width % header->tabWidth;
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

				width += header->spaceWidth;
				break;

			default:
				width += (metrics.get(ch.codePoint) >> 16) & 0x7f;
		}
	}

_break:
	if (width > maxWidth)
		maxWidth = width;

	return maxWidth;
}

int Font::getStringHeight(
	const char *str,
	int        width,
	bool       wordWrap,
	bool       breakOnSpace
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
