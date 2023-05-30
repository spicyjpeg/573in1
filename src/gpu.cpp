
#include <assert.h>
#include <stdint.h>
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "ps1/system.h"
#include "gpu.hpp"

namespace gpu {

/* Basic API */

static constexpr int _DMA_CHUNK_SIZE = 8;
static constexpr int _DMA_TIMEOUT    = 10000;

size_t upload(const RectWH &rect, const void *data, bool wait) {
	size_t length = (rect.w * rect.h) / 2;

	assert(!(length % _DMA_CHUNK_SIZE));
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

	if (!waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT))
		return 0;

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_NONE);

	while (!(GPU_GP1 & GP1_STAT_CMD_READY))
		__asm__ volatile("");

	GPU_GP0 = gp0_flushCache();
	GPU_GP0 = gp0_vramWrite();
	GPU_GP0 = gp0_xy(rect.x, rect.y);
	GPU_GP0 = gp0_xy(rect.w, rect.h);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);

	while (!(GPU_GP1 & GP1_STAT_WRITE_READY))
		__asm__ volatile("");

	DMA_MADR(DMA_GPU) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_GPU) = _DMA_CHUNK_SIZE | (length << 16);
	DMA_CHCR(DMA_GPU) = DMA_CHCR_WRITE | DMA_CHCR_MODE_SLICE | DMA_CHCR_ENABLE;

	if (wait)
		waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

/* Rendering context */

void Context::_flushLayer(void) {
	if (_currentListPtr == _lastListPtr)
		return;

	auto layer = _drawBuffer().layers.pushItem();
	assert(layer);

	*(_currentListPtr++) = gp0_endTag(1);
	*(_currentListPtr++) = gp0_irq();

	*layer        = _lastListPtr;
	_lastListPtr  = _currentListPtr;
}

void Context::_applyResolution(VideoMode mode, int shiftX, int shiftY) const {
	GP1HorizontalRes hres;
	GP1VerticalRes   vres = (height > 256) ? GP1_VRES_512 : GP1_VRES_256;

	int span;

	if (width < 320) {
		hres = GP1_HRES_256;
		span = width * 10;
	} else if (width < 368) {
		hres = GP1_HRES_320;
		span = width * 8;
	} else if (width < 512) {
		hres = GP1_HRES_368;
		span = width * 7;
	} else if (width < 640) {
		hres = GP1_HRES_512;
		span = width * 5;
	} else {
		hres = GP1_HRES_640;
		span = width * 4;
	}

	int x = shiftX + 0x760,                offsetX = span   / 2;
	int y = shiftY + (mode ? 0xa3 : 0x88), offsetY = height / (vres ? 4 : 2);

	GPU_GP1 = gp1_fbMode(hres, vres, mode, height > 256, GP1_COLOR_16BPP);
	GPU_GP1 = gp1_fbRangeH(x - offsetX, x + offsetX);
	GPU_GP1 = gp1_fbRangeV(y - offsetY, y + offsetY);
}

void Context::flip(void) {
	auto &oldBuffer = _drawBuffer(), &newBuffer = _dispBuffer();

	// Ensure the GPU has finished drawing the previous frame.
	while (newBuffer.layers.length)
		__asm__ volatile("");

	auto mask = setInterruptMask(0);

	_flushLayer();
	_currentListPtr = newBuffer.displayList;
	_lastListPtr    = newBuffer.displayList;
	_currentBuffer ^= 1;

	GPU_GP1 = gp1_fbOffset(oldBuffer.clip.x1, oldBuffer.clip.y1);

	// Kick off drawing.
	drawNextLayer();
	if (mask)
		setInterruptMask(mask);
}

void Context::drawNextLayer(void) {
	//auto mask  = setInterruptMask(0);
	auto layer = _dispBuffer().layers.popItem();

	//if (mask)
		//setInterruptMask(mask);
	if (!layer)
		return;

	while (DMA_CHCR(DMA_GPU) & DMA_CHCR_ENABLE)
		__asm__ volatile("");
	while (!(GPU_GP1 & GP1_STAT_CMD_READY))
		__asm__ volatile("");

	GPU_GP1 = gp1_acknowledge();
	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);

	DMA_MADR(DMA_GPU) = reinterpret_cast<uint32_t>(*layer);
	DMA_CHCR(DMA_GPU) = DMA_CHCR_WRITE | DMA_CHCR_MODE_LIST | DMA_CHCR_ENABLE;
}

void Context::setResolution(
	VideoMode mode, int _width, int _height, bool sideBySide
) {
	auto mask = setInterruptMask(0);

	width       = _width;
	height      = _height;
	refreshRate = mode ? 50 : 60;

	for (int fb = 1; fb >= 0; fb--) {
		auto &clip = _buffers[fb].clip;

		clip.x1 = sideBySide ? (_width * fb) : 0;
		clip.y1 = sideBySide ? 0 : (_height * fb);
		clip.x2 = clip.x1 + _width  - 1;
		clip.y2 = clip.y1 + _height - 1;
	}

	_currentListPtr = _buffers[0].displayList;
	_lastListPtr    = _buffers[0].displayList;
	_currentBuffer  = 0;

	_applyResolution(mode);
	if (mask)
		setInterruptMask(mask);
}

uint32_t *Context::newPacket(size_t length) {
	auto ptr        = _currentListPtr;
	_currentListPtr = &ptr[length + 1];

	//assert(_currentListPtr <= &_drawBuffer().displayList[DISPLAY_LIST_SIZE]);

	*(ptr++) = gp0_tag(length, _currentListPtr);
	return ptr;
}

void Context::newLayer(int x, int y, int drawWidth, int drawHeight) {
	auto mask = setInterruptMask(0);

	_flushLayer();
	if (mask)
		setInterruptMask(mask);

	auto &clip = _dispBuffer().clip;

	x += clip.x1;
	y += clip.y1;

	auto cmd = newPacket(3);

	cmd[0] = gp0_fbOrigin(x, y);
	cmd[1] = gp0_fbOffset1(
		util::max(int(clip.x1), x),
		util::max(int(clip.y1), y)
	);
	cmd[2] = gp0_fbOffset2(
		util::min(int(clip.x2), x + drawWidth  - 1),
		util::min(int(clip.y2), y + drawHeight - 1)
	);
}

void Context::setTexturePage(uint16_t page, bool dither) {
	uint32_t cmd = gp0_texpage(page, dither, false);

	if (cmd != _lastTexpage) {
		*newPacket(1) = cmd;
		_lastTexpage  = cmd;
	}
}

void Context::setBlendMode(BlendMode blendMode, bool dither) {
	uint16_t page = _lastTexpage & ~gp0_texpage(
		gp0_page(0, 0, GP0_BLEND_BITMASK, GP0_COLOR_4BPP), true, true
	);
	page |= gp0_page(0, 0, blendMode, GP0_COLOR_4BPP);

	setTexturePage(page, dither);
}

void Context::drawRect(
	int x, int y, int width, int height, Color color, bool blend
) {
	auto cmd = newPacket(3);

	cmd[0] = color | gp0_rectangle(false, false, blend);
	cmd[1] = gp0_xy(x, y);
	cmd[2] = gp0_xy(width, height);
}

void Context::drawGradientRectH(
	int x, int y, int width, int height, Color left, Color right, bool blend
) {
	auto cmd = newPacket(8);

	cmd[0] = left | gp0_shadedQuad(true, false, blend);
	cmd[1] = gp0_xy(x, y);
	cmd[2] = right;
	cmd[3] = gp0_xy(x + width, y);
	cmd[4] = left;
	cmd[5] = gp0_xy(x, y + height);
	cmd[6] = right;
	cmd[7] = gp0_xy(x + width, y + height);
}

void Context::drawGradientRectV(
	int x, int y, int width, int height, Color top, Color bottom, bool blend
) {
	auto cmd = newPacket(8);

	cmd[0] = top | gp0_shadedQuad(true, false, blend);
	cmd[1] = gp0_xy(x, y);
	cmd[2] = top;
	cmd[3] = gp0_xy(x + width, y);
	cmd[4] = bottom;
	cmd[5] = gp0_xy(x, y + height);
	cmd[6] = bottom;
	cmd[7] = gp0_xy(x + width, y + height);
}

void Context::drawGradientRectD(
	int x, int y, int width, int height, Color top, Color middle, Color bottom,
	bool blend
) {
	auto cmd = newPacket(8);

	cmd[0] = top | gp0_shadedQuad(true, false, blend);
	cmd[1] = gp0_xy(x, y);
	cmd[2] = middle;
	cmd[3] = gp0_xy(x + width, y);
	cmd[4] = middle;
	cmd[5] = gp0_xy(x, y + height);
	cmd[6] = bottom;
	cmd[7] = gp0_xy(x + width, y + height);
}

void Context::drawBackdrop(Color color, BlendMode blendMode) {
	setBlendMode(blendMode, true);
	drawRect(0, 0, width, height, color, true);
}

/* Image and font classes */

void Image::initFromVRAMRect(
	const RectWH &rect, ColorDepth colorDepth, BlendMode blendMode
) {
	int shift = 2 - int(colorDepth);

	u       = (rect.x & 0x3f) << shift;
	v       = rect.y & 0xff;
	width   = rect.w << shift;
	height  = rect.h;
	texpage = gp0_page(rect.x / 64, rect.y / 256, blendMode, colorDepth);
}

bool Image::initFromTIMHeader(const TIMHeader *header, BlendMode blendMode) {
	if (header->magic != 0x10)
		return false;

	auto ptr = reinterpret_cast<const uint8_t *>(&header[1]);

	if (header->flags & (1 << 3)) {
		auto clut = reinterpret_cast<const TIMSectionHeader *>(ptr);

		palette = gp0_clut(clut->vram.x / 16, clut->vram.y);
		ptr    += clut->length;
	}

	auto image = reinterpret_cast<const TIMSectionHeader *>(ptr);

	initFromVRAMRect(image->vram, ColorDepth(header->flags & 3), blendMode);
	return true;
}

void Image::drawScaled(
	Context &ctx, int x, int y, int w, int h, bool blend
) const {
	int x2 = x + w, u2 = u + width;
	int y2 = y + h, v2 = v + height;

	auto cmd = ctx.newPacket(9);

	cmd[0] = gp0_quad(true, blend);
	cmd[1] = gp0_xy(x,  y);
	cmd[2] = gp0_uv(u,  v, palette);
	cmd[3] = gp0_xy(x2, y);
	cmd[4] = gp0_uv(u2, v, texpage);
	cmd[5] = gp0_xy(x,  y2);
	cmd[6] = gp0_uv(u,  v2, 0);
	cmd[7] = gp0_xy(x2, y2);
	cmd[8] = gp0_uv(u2, v2, 0);
}

void Image::draw(Context &ctx, int x, int y, bool blend) const {
	ctx.setTexturePage(texpage);
	auto cmd = ctx.newPacket(4);

	cmd[0] = gp0_rectangle(true, true, blend);
	cmd[1] = gp0_xy(x, y);
	cmd[2] = gp0_uv(u, v, palette);
	cmd[3] = gp0_xy(width, height);
}

void Font::draw(
	Context &ctx, const char *str, const Rect &rect, Color color, bool wordWrap
) const {
	// This is required for non-ASCII characters to work properly.
	auto _str = reinterpret_cast<const uint8_t *>(str);

	if (!str)
		return;

	ctx.setTexturePage(image.texpage);

	int x = rect.x1;
	int y = rect.y1;

	for (uint8_t ch = *_str; ch; ch = *(++_str)) {
		bool wrap = wordWrap;

		assert(ch < (FONT_CHAR_OFFSET + FONT_CHAR_COUNT));
		switch (ch) {
			case '\t':
				x += FONT_TAB_WIDTH - 1;
				x -= x % FONT_TAB_WIDTH;
				break;

			case '\n':
				x  = rect.x1;
				y += FONT_LINE_HEIGHT;
				break;

			case '\r':
				x = rect.x1;
				break;

			case ' ':
				x += FONT_SPACE_WIDTH;
				break;

			default:
				uint32_t size = metrics[ch - FONT_CHAR_OFFSET];

				int u = size & 0xff; size >>= 8;
				int v = size & 0xff; size >>= 8;
				int w = size & 0x7f; size >>= 7;
				int h = size & 0x7f; size >>= 7;

				auto cmd = ctx.newPacket(4);

				cmd[0] = color | gp0_rectangle(true, size, true);
				cmd[1] = gp0_xy(x, y);
				cmd[2] = gp0_uv(u + image.u, v + image.v, image.palette);
				cmd[3] = gp0_xy(w, h);

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
			y += FONT_LINE_HEIGHT;
		}
		if (y > rect.y2)
			break;
	}
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

int Font::getStringWidth(const char *str, bool breakOnSpace) const {
	auto _str = reinterpret_cast<const uint8_t *>(str);
	if (!str)
		return 0;

	int width = 0, maxWidth = 0;

	for (uint8_t ch = *_str; ch; ch = *(++_str)) {
		assert(ch < (FONT_CHAR_OFFSET + FONT_CHAR_COUNT));
		switch (ch) {
			case '\t':
				if (breakOnSpace)
					goto _break;

				width += FONT_TAB_WIDTH - 1;
				width -= width % FONT_TAB_WIDTH;
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

				width += FONT_SPACE_WIDTH;
				break;

			default:
				width += (metrics[ch - FONT_CHAR_OFFSET] >> 16) & 0x7f;
		}
	}

_break:
	return (width > maxWidth) ? width : maxWidth;
}

}
