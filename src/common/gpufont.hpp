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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/gpufont.hpp"
#include "common/util/string.hpp"
#include "common/util/templates.hpp"
#include "common/gpu.hpp"

namespace gpu {

/* Font metrics class */

static constexpr size_t METRICS_BUCKET_COUNT    = 256;
static constexpr size_t METRICS_CODE_POINT_BITS = 21;

static constexpr util::UTF8CodePoint FONT_INVALID_CHAR = 0xfffd;

using CharacterSize = uint32_t;

struct FontMetricsHeader {
public:
	uint8_t spaceWidth, tabWidth, lineHeight;
	int8_t  baselineOffset;
};

struct FontMetricsEntry {
public:
	uint32_t      codePoint;
	CharacterSize size;

	inline util::UTF8CodePoint getCodePoint(void) const {
		return codePoint & ((1 << METRICS_CODE_POINT_BITS) - 1);
	}
	inline uint32_t getChained(void) const {
		return codePoint >> METRICS_CODE_POINT_BITS;
	}
};

class FontMetrics : public util::Data {
public:
	inline const FontMetricsHeader *getHeader(void) const {
		return as<const FontMetricsHeader>();
	}
	inline CharacterSize operator[](util::UTF8CodePoint id) const {
		return get(id);
	}

	CharacterSize get(util::UTF8CodePoint id) const;
};

/* Font class */

class Font {
public:
	Image       image;
	FontMetrics metrics;

	inline int getSpaceWidth(void) const {
		if (!metrics.ptr)
			return 0;

		return metrics.getHeader()->spaceWidth;
	}
	inline int getLineHeight(void) const {
		if (!metrics.ptr)
			return 0;

		return metrics.getHeader()->lineHeight;
	}

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
	int getCharacterWidth(util::UTF8CodePoint ch) const;
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
