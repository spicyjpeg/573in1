
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "common/gpu.hpp"
#include "common/util.hpp"
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "ps1/system.h"
#include "vendor/qrcodegen.h"

namespace gpu {

/* Basic API */

static constexpr int _DMA_CHUNK_SIZE = 1;
static constexpr int _DMA_TIMEOUT    = 10000;

size_t upload(const RectWH &rect, const void *data, bool wait) {
	size_t length = (rect.w * rect.h) / 2;

	util::assertAligned<uint32_t>(data);
	//assert(!(length % _DMA_CHUNK_SIZE));
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

	if (!waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT))
		return 0;

	auto enable = disableInterrupts();
	GPU_GP1     = gp1_dmaRequestMode(GP1_DREQ_NONE);

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

	if (enable)
		enableInterrupts();
	if (wait)
		waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

size_t download(const RectWH &rect, void *data, bool wait) {
	size_t length = (rect.w * rect.h) / 2;

	util::assertAligned<uint32_t>(data);
	//assert(!(length % _DMA_CHUNK_SIZE));
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

	if (!waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT))
		return 0;

	auto enable = disableInterrupts();
	GPU_GP1     = gp1_dmaRequestMode(GP1_DREQ_NONE);

	while (!(GPU_GP1 & GP1_STAT_CMD_READY))
		__asm__ volatile("");

	GPU_GP0 = gp0_flushCache();
	GPU_GP0 = gp0_vramRead();
	GPU_GP0 = gp0_xy(rect.x, rect.y);
	GPU_GP0 = gp0_xy(rect.w, rect.h);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_READ);

	while (!(GPU_GP1 & GP1_STAT_READ_READY))
		__asm__ volatile("");

	DMA_MADR(DMA_GPU) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_GPU) = _DMA_CHUNK_SIZE | (length << 16);
	DMA_CHCR(DMA_GPU) = DMA_CHCR_READ | DMA_CHCR_MODE_SLICE | DMA_CHCR_ENABLE;

	if (enable)
		enableInterrupts();
	if (wait)
		waitForDMATransfer(DMA_GPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

/* Rendering context */

void Context::_applyResolution(
	VideoMode mode, bool forceInterlace, int shiftX, int shiftY
) const {
	GP1HorizontalRes hres;
	GP1VerticalRes   vres;

	int span, vdiv;

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

	if (height <= 256) {
		vres = GP1_VRES_256;
		vdiv = 1;
	} else {
		vres = GP1_VRES_512;
		vdiv = 2;

		forceInterlace = true;
	}

	int x = shiftX + 0x760,                offsetX = span   >> 1;
	int y = shiftY + (mode ? 0xa3 : 0x88), offsetY = height >> vdiv;

	GPU_GP1 = gp1_fbMode(hres, vres, mode, forceInterlace, GP1_COLOR_16BPP);
	GPU_GP1 = gp1_fbRangeH(x - offsetX, x + offsetX);
	GPU_GP1 = gp1_fbRangeV(y - offsetY, y + offsetY);
}

void Context::flip(void) {
	auto &oldBuffer = _drawBuffer(), &newBuffer = _dispBuffer();

	// Ensure the GPU has finished drawing the previous frame.
	while (!isIdle())
		__asm__ volatile("");

	// The GPU will take some additional time to toggle between odd and even
	// fields in interlaced mode.
	if (GPU_GP1 & GP1_STAT_FB_INTERLACE) {
		uint32_t drawField, dispField;

		do {
			auto status = GPU_GP1;
			drawField   = (status / GP1_STAT_DRAW_FIELD_ODD) & 1;
			dispField   = (status / GP1_STAT_DISP_FIELD_ODD) & 1;
		} while (!(drawField ^ dispField));
	}

	*_currentListPtr = gp0_endTag(0);
	_currentListPtr  = newBuffer.displayList;
	_currentBuffer  ^= 1;

	GPU_GP1 = gp1_fbOffset(newBuffer.clip.x1, newBuffer.clip.y1);
	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);

	DMA_MADR(DMA_GPU) = reinterpret_cast<uint32_t>(oldBuffer.displayList);
	DMA_CHCR(DMA_GPU) = DMA_CHCR_WRITE | DMA_CHCR_MODE_LIST | DMA_CHCR_ENABLE;
}

void Context::setResolution(
	VideoMode mode, int _width, int _height, bool forceInterlace,
	bool sideBySide
) {
	auto enable = disableInterrupts();

	width       = _width;
	height      = _height;
	refreshRate = mode ? 50 : 60;

	for (int fb = 1; fb >= 0; fb--) {
		auto &clip = _buffers[fb].clip;

		if (_height > 256) {
			clip.x1 = 0;
			clip.y1 = 0;
		} else if (sideBySide) {
			clip.x1 = fb ? _width : 0;
			clip.y1 = 0;
		} else {
			clip.x1 = 0;
			clip.y1 = fb ? _height : 0;
		}

		clip.x2 = clip.x1 + _width  - 1;
		clip.y2 = clip.y1 + _height - 1;
	}

	_currentListPtr = _buffers[0].displayList;
	_currentBuffer  = 0;

	flip();
	_applyResolution(mode, forceInterlace);
	if (enable)
		enableInterrupts();
}

uint32_t *Context::newPacket(size_t length) {
	auto ptr        = _currentListPtr;
	_currentListPtr = &ptr[length + 1];

	//assert(_currentListPtr <= &_drawBuffer().displayList[DISPLAY_LIST_SIZE]);

	*(ptr++) = gp0_tag(length, _currentListPtr);
	return ptr;
}

void Context::newLayer(int x, int y, int drawWidth, int drawHeight) {
	auto &clip = _drawBuffer().clip;

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

/* Image class */

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

	// Even though the packet has a texpage field, setTexturePage() is required
	// here in order to update _lastTexpage and ensure dithering is disabled.
	ctx.setTexturePage(texpage);
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

/* QR code encoder */

static void _loadQRCode(Image &output, int x, int y, const uint32_t *qrCode) {
	int    size = qrcodegen_getSize(qrCode) + 2;
	RectWH rect;

	// Generate a 16-color (only 2 colors used) palette and place it below the
	// QR code in VRAM.
	const uint32_t palette[8]{ 0x8000ffff };

	rect.x = x;
	rect.y = y + size;
	rect.w = 16;
	rect.h = 1;
	upload(rect, palette, true);

	rect.y = y;
	rect.w = qrcodegen_getStride(qrCode) * 2;
	rect.h = size;
	upload(rect, &qrCode[1], true);

	output.u       = (x & 0x3f) * 4;
	output.v       = y & 0xff;
	output.width   = size - 1;
	output.height  = size - 1;
	output.texpage =
		gp0_page(x / 64, y / 256, GP0_BLEND_SEMITRANS, GP0_COLOR_4BPP);
	output.palette = gp0_clut(x / 16, y + size);

	LOG("loaded at (%d,%d), size=%d", x, y, size);
}

bool generateQRCode(
	Image &output, int x, int y, const char *str, qrcodegen_Ecc ecc
) {
	uint32_t qrCode[qrcodegen_BUFFER_LEN_MAX];
	uint32_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

	auto segment = qrcodegen_makeAlphanumeric(
		str, reinterpret_cast<uint8_t *>(tempBuffer)
	);
	if (!qrcodegen_encodeSegments(&segment, 1, ecc, tempBuffer, qrCode)) {
		LOG("QR encoding failed");
		return false;
	}

	_loadQRCode(output, x, y, qrCode);
	return true;
}

bool generateQRCode(
	Image &output, int x, int y, const uint8_t *data, size_t length,
	qrcodegen_Ecc ecc
) {
	uint32_t qrCode[qrcodegen_BUFFER_LEN_MAX];
	uint32_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];

	auto segment = qrcodegen_makeBytes(
		data, length, reinterpret_cast<uint8_t *>(tempBuffer)
	);
	if (!qrcodegen_encodeSegments(&segment, 1, ecc, tempBuffer, qrCode)) {
		LOG("QR encoding failed");
		return false;
	}

	_loadQRCode(output, x, y, qrCode);
	return true;
}

}
