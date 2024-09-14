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

#include <assert.h>
#include <stdint.h>
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "common/spu.hpp"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace spu {

/* Basic API */

static constexpr int _DMA_CHUNK_SIZE = 4;
static constexpr int _DMA_TIMEOUT    = 100000;
static constexpr int _STATUS_TIMEOUT = 10000;

static bool _waitForStatus(uint16_t mask, uint16_t value) {
	for (int timeout = _STATUS_TIMEOUT; timeout > 0; timeout -= 10) {
		if ((SPU_STAT & mask) == value)
			return true;

		delayMicroseconds(10);
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

	SPU_FLAG_FM1     = 0;
	SPU_FLAG_FM2     = 0;
	SPU_FLAG_NOISE1  = 0;
	SPU_FLAG_NOISE2  = 0;
	SPU_FLAG_REVERB1 = 0;
	SPU_FLAG_REVERB2 = 0;

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
	stopChannels(ALL_CHANNELS);
}

Channel getFreeChannel(void) {
#if 0
	// The status flag gets set when a channel stops or loops for the first
	// time rather than when it actually goes silent (so it will be set early
	// for e.g. short looping samples with a long release envelope, or samples
	// looping indefinitely).
	ChannelMask mask =
		util::concat4(SPU_FLAG_STATUS1, SPU_FLAG_STATUS2) & ALL_CHANNELS;

	for (Channel ch = 0; mask; ch++, mask >>= 1) {
		if (mask & 1)
			return ch;
	}
#else
	for (Channel ch = 0; ch < NUM_CHANNELS; ch++) {
		if (!SPU_CH_ADSR_VOL(ch))
			return ch;
	}
#endif

	return -1;
}

ChannelMask getFreeChannels(int count) {
	ChannelMask mask = 0;

	for (Channel ch = 0; ch < NUM_CHANNELS; ch++) {
		if (SPU_CH_ADSR_VOL(ch))
			continue;

		mask |= 1 << ch;
		count--;

		if (!count)
			return mask;
	}

	return 0;
}

void stopChannels(ChannelMask mask) {
	mask &= ALL_CHANNELS;

	for (Channel ch = 0; mask; ch++, mask >>= 1) {
		if (!(mask & 1))
			continue;

		SPU_CH_VOL_L(ch) = 0;
		SPU_CH_VOL_R(ch) = 0;
		SPU_CH_FREQ(ch)  = 1 << 12;
		SPU_CH_ADDR(ch)  = DUMMY_BLOCK_OFFSET / 8;
	}

	SPU_FLAG_OFF1 = mask & 0xffff;
	SPU_FLAG_OFF2 = mask >> 16;
	SPU_FLAG_ON1  = mask & 0xffff;
	SPU_FLAG_ON2  = mask >> 16;
}

size_t upload(uint32_t offset, const void *data, size_t length, bool wait) {
	length /= 4;

	util::assertAligned<uint32_t>(data);
#if 0
	assert(!(length % _DMA_CHUNK_SIZE));
#else
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;
#endif

	if (!waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT))
		return 0;

	uint16_t ctrlReg = SPU_CTRL & ~SPU_CTRL_XFER_BITMASK;

	SPU_CTRL = ctrlReg;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, 0);

	SPU_DMA_CTRL = 4;
	SPU_ADDR     = offset / 8;
	SPU_CTRL     = ctrlReg | SPU_CTRL_XFER_DMA_WRITE;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, SPU_CTRL_XFER_DMA_WRITE);

	DMA_MADR(DMA_SPU) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_SPU) = util::concat4(_DMA_CHUNK_SIZE, length);
	DMA_CHCR(DMA_SPU) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_SLICE
		| DMA_CHCR_ENABLE;

	if (wait)
		waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

size_t download(uint32_t offset, void *data, size_t length, bool wait) {
	length /= 4;

	util::assertAligned<uint32_t>(data);
#if 0
	assert(!(length % _DMA_CHUNK_SIZE));
#else
	length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;
#endif

	if (!waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT))
		return 0;

	uint16_t ctrlReg = SPU_CTRL & ~SPU_CTRL_XFER_BITMASK;

	SPU_CTRL = ctrlReg;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, 0);

	SPU_DMA_CTRL = 4;
	SPU_ADDR     = offset / 8;
	SPU_CTRL     = ctrlReg | SPU_CTRL_XFER_DMA_READ;
	_waitForStatus(SPU_CTRL_XFER_BITMASK, SPU_CTRL_XFER_DMA_READ);

	DMA_MADR(DMA_SPU) = reinterpret_cast<uint32_t>(data);
	DMA_BCR (DMA_SPU) = util::concat4(_DMA_CHUNK_SIZE, length);
	DMA_CHCR(DMA_SPU) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_SLICE
		| DMA_CHCR_ENABLE;

	if (wait)
		waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT);

	return length * _DMA_CHUNK_SIZE * 4;
}

/* Sound class */

Sound::Sound(void)
: offset(0), sampleRate(0), length(0) {}

bool Sound::initFromVAGHeader(const VAGHeader *header, uint32_t _offset) {
	if (header->magic != util::concat4('V', 'A', 'G', 'p'))
		return false;
	if (header->channels > 1)
		return false;

	offset     = _offset;
	sampleRate = (__builtin_bswap32(header->sampleRate) << 12) / 44100;
	length     = __builtin_bswap32(header->length) / 8;

	return true;
}

Channel Sound::play(uint16_t left, uint16_t right, Channel ch) const {
	if ((ch < 0) || (ch >= NUM_CHANNELS))
		return -1;
	if (!offset)
		return -1;

	SPU_CH_VOL_L(ch) = left;
	SPU_CH_VOL_R(ch) = right;
	SPU_CH_FREQ (ch) = sampleRate;
	SPU_CH_ADDR (ch) = offset / 8;
	SPU_CH_ADSR1(ch) = 0x00ff;
	SPU_CH_ADSR2(ch) = 0x0000;

	if (ch < 16)
		SPU_FLAG_ON1 = 1 << ch;
	else
		SPU_FLAG_ON2 = 1 << (ch - 16);

	return ch;
}

/* Stream class */

/*
 * The stream driver lays out a ring buffer of interleaved audio chunks in SPU
 * RAM as follows:
 *
 * +---------------------------------+---------------------------------+-----
 * |              Chunk              |              Chunk              |
 * | +------------+------------+     | +------------+------------+     |
 * | |  Ch0 data  |  Ch1 data  | ... | |  Ch0 data  |  Ch1 data  | ... | ...
 * | +------------+------------+     | +------------+------------+     |
 * +-^------------^------------------+-^------------^------------------+-----
 *   | Ch0 start  | Ch1 start          | Ch0 loop   | Ch1 loop
 *                                     | IRQ address
 *
 * The length of each chunk is given by the interleave size multiplied by the
 * channel count. Each data block must be terminated with the loop end and
 * sustain flags set in order to make the channels "jump" to the next chunk's
 * blocks.
 */

void Stream::_configureIRQ(void) const {
	uint16_t ctrlReg = SPU_CTRL;

	// Disable the IRQ if an underrun occurs.
	// TODO: handle this in a slightly better way
	if (!_bufferedChunks) {
		SPU_CTRL = ctrlReg & ~SPU_CTRL_IRQ_ENABLE;
		return;
	}

	// Exit if the IRQ has been set up before and not yet acknowledged by
	// handleInterrupt().
	if (ctrlReg & SPU_CTRL_IRQ_ENABLE)
		return;

	auto tempMask    = _channelMask;
	auto chunkOffset = _getChunkOffset(_head);

	SPU_IRQ_ADDR = chunkOffset / 8;
	SPU_CTRL     = ctrlReg | SPU_CTRL_IRQ_ENABLE;

	for (Channel ch = 0; tempMask; ch++, tempMask >>= 1) {
		if (!(tempMask & 1))
			continue;

		SPU_CH_LOOP_ADDR(ch) = chunkOffset / 8;
		chunkOffset         += interleave;
	}
}

Stream::Stream(void)
: _channelMask(0), offset(0), interleave(0), numChunks(0), sampleRate(0),
channels(0) {
	resetBuffer();
}

bool Stream::initFromVAGHeader(
	const VAGHeader *header, uint32_t _offset, size_t _numChunks
) {
	if (isPlaying())
		return false;
	if (header->magic != util::concat4('V', 'A', 'G', 'i'))
		return false;
	if (!header->interleave)
		return false;

	offset     = _offset;
	interleave = header->interleave;
	numChunks  = _numChunks;
	sampleRate = (__builtin_bswap32(header->sampleRate) << 12) / 44100;
	channels   = header->channels ? header->channels : 2;

	return true;
}

ChannelMask Stream::start(uint16_t left, uint16_t right, ChannelMask mask) {
	if (isPlaying() || !_bufferedChunks)
		return 0;

	mask &= ALL_CHANNELS;

	auto tempMask    = mask;
	auto chunkOffset = _getChunkOffset(_head);
	int  isRightCh   = 0;

	for (Channel ch = 0; tempMask; ch++, tempMask >>= 1) {
		if (!(tempMask & 1))
			continue;

		// Assume each pair of channels is a stereo pair. If the channel count
		// is odd, assume the last channel is mono.
		if (isRightCh) {
			SPU_CH_VOL_L(ch) = 0;
			SPU_CH_VOL_R(ch) = right;
		} else if (tempMask != 1) {
			SPU_CH_VOL_L(ch) = left;
			SPU_CH_VOL_R(ch) = 0;
		} else {
			SPU_CH_VOL_L(ch) = left;
			SPU_CH_VOL_R(ch) = right;
		}

		SPU_CH_FREQ(ch)  = sampleRate;
		SPU_CH_ADDR(ch)  = chunkOffset / 8;
		SPU_CH_ADSR1(ch) = 0x00ff;
		SPU_CH_ADSR2(ch) = 0x0000;

		chunkOffset += interleave;
		isRightCh   ^= 1;
	}

	_channelMask = mask;
	SPU_FLAG_ON1 = mask & 0xffff;
	SPU_FLAG_ON2 = mask >> 16;

	handleInterrupt();
	return mask;
}

void Stream::stop(void) {
	util::CriticalSection sec;

	if (isPlaying()) {
		SPU_CTRL    &= ~SPU_CTRL_IRQ_ENABLE;
		_channelMask = 0;

		stopChannels(_channelMask);
	}

	flushWriteQueue();
}

void Stream::handleInterrupt(void) {
	if (!isPlaying())
		return;

	// Disabling the IRQ is always required in order to acknowledge it.
	SPU_CTRL &= ~SPU_CTRL_IRQ_ENABLE;

	_head = (_head + 1) % numChunks;
	_bufferedChunks--;
	_configureIRQ();
}

size_t Stream::feed(const void *data, size_t count) {
	util::CriticalSection sec;

	auto ptr         = reinterpret_cast<uintptr_t>(data);
	auto chunkLength = getChunkLength();
	count            = util::min(count, getFreeChunkCount());

	for (auto i = count; i; i--) {
		upload(
			_getChunkOffset(_tail), reinterpret_cast<const void *>(ptr),
			chunkLength, true
		);

		_tail = (_tail + 1) % numChunks;
		ptr  += chunkLength;
	}

	_bufferedChunks += count;

	if (isPlaying())
		_configureIRQ();

	flushWriteQueue();
	return count;
}

void Stream::resetBuffer(void) {
	_head           = 0;
	_tail           = 0;
	_bufferedChunks = 0;
}

}
