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
#include "common/util/containers.hpp"
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "vendor/qrcodegen.h"

namespace gpu {

/* Types */

using Color      = uint32_t;
using BlendMode  = GP0BlendMode;
using ColorDepth = GP0ColorDepth;
using VideoMode  = GP1VideoMode;

struct Rect {
public:
	int16_t x1, y1, x2, y2;
};

struct RectWH {
public:
	int16_t x, y, w, h;
};

/* Basic API */

static inline bool isIdle(void) {
	return (
		!(DMA_CHCR(DMA_GPU) & DMA_CHCR_ENABLE) &&
		(GPU_GP1 & GP1_STAT_CMD_READY)
	);
}

static inline void enableDisplay(bool enable) {
	GPU_GP1 = gp1_dispBlank(!enable);
}

static inline VideoMode getVideoMode(void) {
	return VideoMode((GPU_GP1 / GP1_STAT_FB_MODE_BITMASK) & 1);
}

void init(void);
size_t upload(const RectWH &rect, const void *data, bool wait = false);
size_t download(const RectWH &rect, void *data, bool wait = false);
void sendLinkedList(const void *data, bool wait = false);

/* Rendering context */

struct Buffer {
public:
	Rect       clip;
	util::Data displayList;
};

class Context {
private:
	Buffer   _buffers[2];
	uint32_t *_currentListPtr;
	int      _currentBuffer;

	uint32_t _lastTexpage;

	void _applyResolution(
		VideoMode mode,
		bool      forceInterlace = false,
		int       shiftX = 0,
		int       shiftY = 0
	) const;

public:
	int width, height, refreshRate;

	inline void getVRAMClipRect(RectWH &output) const {
		auto &clip = _buffers[_currentBuffer ^ 1].clip;

		output.x = clip.x1;
		output.y = clip.y1;
		output.w = width;
		output.h = height;
	}

	inline void newLayer(int x, int y) {
		newLayer(x, y, width, height);
	}

	inline void drawRect(RectWH &rect, Color color, bool blend = false) {
		drawRect(rect.x, rect.y, rect.w, rect.h, color, blend);
	}
	inline void drawGradientRectH(
		RectWH &rect,
		Color  left,
		Color  right,
		bool   blend = false
	) {
		drawGradientRectH(rect.x, rect.y, rect.w, rect.h, left, right, blend);
	}
	inline void drawGradientRectV(
		RectWH &rect,
		Color  top,
		Color  bottom,
		bool   blend = false
	) {
		drawGradientRectV(rect.x, rect.y, rect.w, rect.h, top, bottom, blend);
	}
	inline void drawGradientRectD(
		RectWH &rect,
		Color  top,
		Color  middle,
		Color  bottom,
		bool   blend = false
	) {
		drawGradientRectD(
			rect.x, rect.y, rect.w, rect.h, top, middle, bottom, blend
		);
	}

	inline void drawBackdrop(Color color) {
		drawRect(0, 0, width, height, color);
	}
	inline void drawBackdrop(Color color, BlendMode blendMode) {
		setBlendMode(blendMode, true);
		drawRect(0, 0, width, height, color, true);
	}

	Context(
		VideoMode mode,
		int       width,
		int       height,
		bool      forceInterlace = false,
		bool      sideBySide     = false
	);
	void setResolution(
		VideoMode mode,
		int       width,
		int       height,
		bool      forceInterlace = false,
		bool      sideBySide     = false
	);
	void flip(void);

	uint32_t *newPacket(size_t length);
	void newLayer(int x, int y, int drawWidth, int drawHeight);
	void setTexturePage(uint16_t page, bool dither = false);
	void setBlendMode(BlendMode blendMode, bool dither = false);

	void drawRect(
		int   x,
		int   y,
		int   width,
		int   height,
		Color color,
		bool  blend = false
	);
	void drawGradientRectH(
		int   x,
		int   y,
		int   width,
		int   height,
		Color left,
		Color right,
		bool  blend = false
	);
	void drawGradientRectV(
		int   x,
		int   y,
		int   width,
		int   height,
		Color top,
		Color bottom,
		bool  blend = false
	);
	void drawGradientRectD(
		int   x,
		int   y,
		int   width,
		int   height,
		Color top,
		Color middle,
		Color bottom,
		bool  blend = false
	);
};

/* Image class */

class TIMSectionHeader {
public:
	uint32_t length;
	RectWH   vram;

	inline const void *getData(void) const {
		return this + 1;
	}
	inline const TIMSectionHeader *getNextSection(void) const {
		return reinterpret_cast<const TIMSectionHeader *>(
			uintptr_t(this) + length
		);
	}
};

class TIMHeader {
public:
	uint32_t magic, flags;

	inline bool validateMagic(void) const {
		return (magic == 0x10) && (getColorDepth() <= GP0_COLOR_16BPP);
	}
	inline ColorDepth getColorDepth(void) const {
		return ColorDepth(flags & 7);
	}

	inline const TIMSectionHeader *getImage(void) const {
		auto image = reinterpret_cast<const TIMSectionHeader *>(this + 1);

		if (flags & (1 << 3))
			image = image->getNextSection();

		return image;
	}
	inline const TIMSectionHeader *getCLUT(void) const {
		if (flags & (1 << 3))
			return reinterpret_cast<const TIMSectionHeader *>(this + 1);

		return nullptr;
	}
};

class Image {
public:
	uint16_t u, v, width, height;
	uint16_t texpage, palette;

	Image(void);
	void initFromVRAMRect(
		const RectWH &rect,
		ColorDepth   colorDepth,
		BlendMode    blendMode = GP0_BLEND_SEMITRANS
	);
	bool initFromTIMHeader(
		const TIMHeader &header,
		BlendMode       blendMode = GP0_BLEND_SEMITRANS
	);
	void drawScaled(
		Context &ctx,
		int     x,
		int     y,
		int     w,
		int     h,
		bool    blend = false
	) const;
	void draw(Context &ctx, int x, int y, bool blend = false) const;
};

/* QR code encoder */

bool generateQRCode(
	Image         &output,
	int           x,
	int           y,
	const char    *str,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);
bool generateQRCode(
	Image         &output,
	int           x,
	int           y,
	const uint8_t *data,
	size_t        length,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);

}
