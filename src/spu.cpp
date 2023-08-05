
#include <assert.h>
#include <stdint.h>
#include "ps1/registers.h"
#include "spu.hpp"
#include "util.hpp"

namespace spu {

/* Basic API */

static constexpr int _DMA_CHUNK_SIZE = 8;
static constexpr int _DMA_TIMEOUT    = 10000;
static constexpr int _STATUS_TIMEOUT = 1000;

static bool _waitForStatus(uint16_t mask, uint16_t value) {
	for (int timeout = _STATUS_TIMEOUT; timeout > 0; timeout--) {
		if ((SPU_STAT & mask) == value)
			return true;

		delayMicroseconds(1);
	}

	return false;
}

void init(void) {
	BIU_DEV4_CTRL = 0
		| ( 1 << 0) // Write delay
		| (14 << 4) // Read delay
		| BIU_CTRL_RECOVERY
		| BIU_CTRL_WIDTH_16
		| BIU_CTRL_AUTO_INCR
		| (9 << 16) // Number of address lines
		| (0 << 24) // DMA read/write delay
		| BIU_CTRL_DMA_DELAY;

	SPU_CTRL = 0;
	_waitForStatus(0x3f, 0);

	SPU_MASTER_VOL_L = 0;
	SPU_MASTER_VOL_R = 0;
	SPU_REVERB_VOL_L = 0;
	SPU_REVERB_VOL_R = 0;
	SPU_REVERB_ADDR  = 0xfffe;

	SPU_CTRL = SPU_CTRL_ENABLE;
	_waitForStatus(0x3f, 0);

	// Place a dummy (silent) looping block at the beginning of SPU RAM.
	SPU_DMA_CTRL = 4;
	SPU_ADDR     = DUMMY_BLOCK_OFFSET / 8;

	SPU_DATA = 0x0500;
	for (int i = 7; i > 0; i--)
		SPU_DATA = 0;

	SPU_CTRL = SPU_CTRL_XFER_WRITE | SPU_CTRL_ENABLE;
	_waitForStatus(SPU_CTRL_XFER_BITMASK | SPU_STAT_BUSY, SPU_CTRL_XFER_WRITE);
	delayMicroseconds(100);

	SPU_CTRL = SPU_CTRL_UNMUTE | SPU_CTRL_ENABLE;
	resetAllChannels();
}

int getFreeChannel(void) {
	uint32_t flags = SPU_FLAG_STATUS1 | (SPU_FLAG_STATUS2 << 16);

	for (int ch = 0; flags; ch++, flags >>= 1) {
		if (flags & 1)
			return ch;
	}

	return -1;
}

void stopChannel(int ch) {
	SPU_CH_VOL_L(ch) = 0;
	SPU_CH_VOL_R(ch) = 0;
	SPU_CH_FREQ(ch)  = 0;
	SPU_CH_ADDR(ch)  = 0;

	if (ch < 16) {
		SPU_FLAG_OFF1 = 1 << ch;
		SPU_FLAG_ON1  = 1 << ch;
	} else {
		SPU_FLAG_OFF2 = 1 << (ch - 16);
		SPU_FLAG_ON2  = 1 << (ch - 16);
	}
}

void resetAllChannels(void) {
	for (int ch = 23; ch >= 0; ch--) {
		SPU_CH_VOL_L(ch) = 0;
		SPU_CH_VOL_R(ch) = 0;
		SPU_CH_FREQ(ch)  = 0x1000;
		SPU_CH_ADDR(ch)  = DUMMY_BLOCK_OFFSET / 8;
	}

	SPU_FLAG_FM1     = 0;
	SPU_FLAG_FM2     = 0;
	SPU_FLAG_NOISE1  = 0;
	SPU_FLAG_NOISE2  = 0;
	SPU_FLAG_REVERB1 = 0;
	SPU_FLAG_REVERB2 = 0;
	SPU_FLAG_ON1     = 0xffff;
	SPU_FLAG_ON2     = 0x00ff;
}

size_t upload(uint32_t ramOffset, const void *data, size_t length, bool wait) {
	length /= 4;

	util::assertAligned<uint32_t>(data);
	//assert(!(length % _DMA_CHUNK_SIZE));
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

	if (!waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT))
		return 0;

	uint16_t ctrlReg = SPU_CTRL & ~SPU_CTRL_XFER_BITMASK;

	SPU_CTRL = ctrlReg;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, 0);

	SPU_DMA_CTRL = 4;
	SPU_ADDR     = ramOffset / 8;
	SPU_CTRL     = ctrlReg | SPU_CTRL_XFER_DMA_WRITE;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, SPU_CTRL_XFER_DMA_WRITE);

	DMA_MADR(DMA_SPU) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_SPU) = _DMA_CHUNK_SIZE | (length << 16);
	DMA_CHCR(DMA_SPU) = DMA_CHCR_WRITE | DMA_CHCR_MODE_SLICE | DMA_CHCR_ENABLE;

	if (wait)
		waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

/* Sound class */

bool Sound::initFromVAGHeader(const VAGHeader *header, uint32_t ramOffset) {
	if ((header->magic != 0x70474156) || header->interleave)
		return false;

	offset     = ramOffset / 8;
	sampleRate = (__builtin_bswap32(header->sampleRate) << 12) / 44100;
	length     = __builtin_bswap32(header->length);

	return true;
}

int Sound::play(int ch, int16_t volume) const {
	if ((ch < 0) || (ch > 23))
		return -1;
	if (!offset)
		return -1;

	SPU_CH_VOL_L(ch) = volume;
	SPU_CH_VOL_R(ch) = volume;
	SPU_CH_FREQ (ch) = sampleRate;
	SPU_CH_ADDR (ch) = offset;
	SPU_CH_ADSR1(ch) = 0x00ff;
	SPU_CH_ADSR2(ch) = 0x0000;

	if (ch < 16)
		SPU_FLAG_ON1 = 1 << ch;
	else
		SPU_FLAG_ON2 = 1 << (ch - 16);

	return ch;
}

}
