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

static constexpr uint32_t DUMMY_BLOCK_OFFSET = 0x1000;
static constexpr uint32_t DUMMY_BLOCK_END    = 0x1010;

static constexpr int      NUM_CHANNELS = 24;
static constexpr uint16_t MAX_VOLUME   = 0x3fff;

using Channel = int;

/* Basic API */

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

void init(void);
Channel getFreeChannel(void);
void stopChannel(Channel ch);
void resetAllChannels(void);
size_t upload(uint32_t ramOffset, const void *data, size_t length, bool wait);

/* Sound class */

struct VAGHeader {
public:
	uint32_t magic, version, interleave, length, sampleRate;
	uint16_t _reserved[5], channels;
	char     name[16];
};

class Sound {
public:
	uint16_t offset, sampleRate;
	size_t   length;

	inline Sound(void)
	: offset(0), length(0) {}
	inline Channel play(
		uint16_t left = MAX_VOLUME, uint16_t right = MAX_VOLUME
	) const {
		return play(left, right, getFreeChannel());
	}

	bool initFromVAGHeader(const VAGHeader *header, uint32_t ramOffset);
	Channel play(uint16_t left, uint16_t right, Channel ch) const;
};

}
