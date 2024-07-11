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

struct RectRB {
public:
	int16_t x, y, r, b;
};

/* Basic API */

static inline void init(void) {
	GPU_GP1 = gp1_resetGPU();
	GPU_GP1 = gp1_resetFIFO();

	TIMER_CTRL(0) = TIMER_CTRL_EXT_CLOCK;
	TIMER_CTRL(1) = TIMER_CTRL_EXT_CLOCK;
}

static inline bool isIdle(void) {
	return (
		!(DMA_CHCR(DMA_GPU) & DMA_CHCR_ENABLE) && (GPU_GP1 & GP1_STAT_CMD_READY)
	);
}

static inline void enableDisplay(bool enable) {
	GPU_GP1 = gp1_dispBlank(!enable);
}

size_t upload(const RectWH &rect, const void *data, bool wait);
size_t download(const RectWH &rect, void *data, bool wait);

/* Rendering context */

static constexpr size_t DISPLAY_LIST_SIZE = 0x4000;
static constexpr size_t LAYER_STACK_SIZE  = 16;

struct Buffer {
public:
	Rect     clip;
	uint32_t displayList[DISPLAY_LIST_SIZE];
};

class Context {
private:
	Buffer   _buffers[2];
	uint32_t *_currentListPtr;
	int      _currentBuffer;

	uint32_t _lastTexpage;

	inline Buffer &_drawBuffer(void) {
		return _buffers[_currentBuffer];
	}
	inline Buffer &_dispBuffer(void) {
		return _buffers[_currentBuffer ^ 1];
	}

	void _applyResolution(
		VideoMode mode, bool forceInterlace = false, int shiftX = 0,
		int shiftY = 0
	) const;

public:
	int width, height, refreshRate;

	inline Context(
		VideoMode mode, int width, int height, bool forceInterlace = false,
		bool sideBySide = false
	) : _lastTexpage(0) {
		setResolution(mode, width, height, forceInterlace, sideBySide);
	}
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
		RectWH &rect, Color left, Color right, bool blend = false
	) {
		drawGradientRectH(rect.x, rect.y, rect.w, rect.h, left, right, blend);
	}
	inline void drawGradientRectV(
		RectWH &rect, Color top, Color bottom, bool blend = false
	) {
		drawGradientRectV(rect.x, rect.y, rect.w, rect.h, top, bottom, blend);
	}
	inline void drawGradientRectD(
		RectWH &rect, Color top, Color middle, Color bottom, bool blend = false
	) {
		drawGradientRectD(
			rect.x, rect.y, rect.w, rect.h, top, middle, bottom, blend
		);
	}

	void setResolution(
		VideoMode mode, int width, int height, bool forceInterlace = false,
		bool sideBySide = false
	);
	void flip(void);

	uint32_t *newPacket(size_t length);
	void newLayer(int x, int y, int drawWidth, int drawHeight);
	void setTexturePage(uint16_t page, bool dither = false);
	void setBlendMode(BlendMode blendMode, bool dither = false);

	void drawRect(
		int x, int y, int width, int height, Color color, bool blend = false
	);
	void drawGradientRectH(
		int x, int y, int width, int height, Color left, Color right,
		bool blend = false
	);
	void drawGradientRectV(
		int x, int y, int width, int height, Color top, Color bottom,
		bool blend = false
	);
	void drawGradientRectD(
		int x, int y, int width, int height, Color top, Color middle,
		Color bottom, bool blend = false
	);
	void drawBackdrop(Color color, BlendMode blendMode);
};

/* Image class */

struct TIMHeader {
public:
	uint32_t magic, flags;
};

struct TIMSectionHeader {
public:
	uint32_t length;
	RectWH   vram;
};

class Image {
public:
	uint16_t u, v, width, height;
	uint16_t texpage, palette;

	inline Image(void)
	: width(0), height(0) {}

	void initFromVRAMRect(
		const RectWH &rect, ColorDepth colorDepth,
		BlendMode blendMode = GP0_BLEND_SEMITRANS
	);
	bool initFromTIMHeader(
		const TIMHeader *header, BlendMode blendMode = GP0_BLEND_SEMITRANS
	);
	void drawScaled(
		Context &ctx, int x, int y, int w, int h, bool blend = false
	) const;
	void draw(Context &ctx, int x, int y, bool blend = false) const;
};

/* QR code encoder */

bool generateQRCode(
	Image &output, int x, int y, const char *str,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);
bool generateQRCode(
	Image &output, int x, int y, const uint8_t *data, size_t length,
	qrcodegen_Ecc ecc = qrcodegen_Ecc_MEDIUM
);

}
