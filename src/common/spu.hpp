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
#include "ps1/registers.h"

namespace spu {

using Channel     = int;
using ChannelMask = uint32_t;

enum LoopFlag : uint8_t {
	LOOP_END     = 1 << 0,
	LOOP_SUSTAIN = 1 << 1,
	LOOP_START   = 1 << 2
};

static constexpr uint32_t DUMMY_BLOCK_OFFSET = 0x1000;
static constexpr uint32_t DUMMY_BLOCK_END    = 0x1010;

static constexpr int      NUM_CHANNELS = 24;
static constexpr uint16_t MAX_VOLUME   = 0x3fff;

static constexpr ChannelMask ALL_CHANNELS = (1 << NUM_CHANNELS) - 1;

/* Basic API */

void init(void);
Channel getFreeChannel(void);
ChannelMask getFreeChannels(int count = NUM_CHANNELS);
void stopChannels(ChannelMask mask);

static inline void setMasterVolume(uint16_t master, uint16_t reverb = 0) {
	SPU_MASTER_VOL_L = master;
	SPU_MASTER_VOL_R = master;
	SPU_REVERB_VOL_L = reverb;
	SPU_REVERB_VOL_R = reverb;
}

static inline void setChannelVolume(Channel ch, uint16_t left, uint16_t right) {
	if ((ch < 0) || (ch >= NUM_CHANNELS))
		return;

	SPU_CH_VOL_L(ch) = left;
	SPU_CH_VOL_R(ch) = right;
}

static inline void stopChannel(Channel ch) {
	stopChannels(1 << ch);
}

size_t upload(uint32_t offset, const void *data, size_t length, bool wait);
size_t download(uint32_t offset, void *data, size_t length, bool wait);

/* Sound class */

static constexpr size_t INTERLEAVED_VAG_BODY_OFFSET = 2048;

struct VAGHeader {
public:
	uint32_t magic, version, interleave, length, sampleRate;
	uint16_t _reserved[5], channels;
	char     name[16];
};

class Sound {
public:
	uint32_t offset;
	uint16_t sampleRate, length;

	inline Channel play(
		uint16_t left = MAX_VOLUME, uint16_t right = MAX_VOLUME
	) const {
		return play(left, right, getFreeChannel());
	}

	Sound(void);
	bool initFromVAGHeader(const VAGHeader *header, uint32_t _offset);
	Channel play(uint16_t left, uint16_t right, Channel ch) const;
};

/* Stream class */

class Stream {
private:
	ChannelMask _channelMask;
	uint16_t    _head, _tail, _bufferedChunks;

	inline uint32_t _getChunkOffset(size_t chunk) const {
		return offset + getChunkLength() * chunk;
	}

	void _configureIRQ(void) const;

public:
	uint32_t offset;
	uint16_t interleave, numChunks, sampleRate, channels;

	inline ~Stream(void) {
		stop();
	}
	inline ChannelMask start(
		uint16_t left = MAX_VOLUME, uint16_t right = MAX_VOLUME
	) {
		return start(left, right, getFreeChannels(channels));
	}
	inline bool isPlaying(void) const {
		__atomic_signal_fence(__ATOMIC_ACQUIRE);

		return (_channelMask != 0);
	}
	inline size_t getChunkLength(void) const {
		return size_t(interleave) * size_t(channels);
	}
	inline size_t getFreeChunkCount(void) const {
		__atomic_signal_fence(__ATOMIC_ACQUIRE);

		// The currently playing chunk cannot be overwritten.
		size_t playingChunk = isPlaying() ? 1 : 0;
		return numChunks - (_bufferedChunks + playingChunk);
	}

	Stream(void);
	bool initFromVAGHeader(
		const VAGHeader *header, uint32_t _offset, size_t _numChunks
	);
	ChannelMask start(uint16_t left, uint16_t right, ChannelMask mask);
	void stop(void);
	void handleInterrupt(void);

	size_t feed(const void *data, size_t count);
	void resetBuffer(void);
};

}
