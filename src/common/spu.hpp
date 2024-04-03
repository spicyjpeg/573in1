
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

static inline void setChannelVolume(Channel ch, uint16_t volume) {
	SPU_CH_VOL_L(ch) = volume;
	SPU_CH_VOL_R(ch) = volume;
}

void init(void);
Channel getFreeChannel(void);
void stopChannel(Channel ch);
void resetAllChannels(void);
size_t upload(uint32_t ramOffset, const void *data, size_t length, bool wait);

/* Sound class */

struct [[gnu::packed]] VAGHeader {
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
	inline Channel play(uint16_t volume = MAX_VOLUME) const {
		return play(volume, getFreeChannel());
	}

	bool initFromVAGHeader(const VAGHeader *header, uint32_t ramOffset);
	Channel play(uint16_t volume, Channel ch) const;
};

}
