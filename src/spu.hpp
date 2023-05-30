
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"

namespace spu {

/* Basic API */

static inline void setVolume(int16_t master, int16_t reverb = 0) {
	SPU_MASTER_VOL_L = master;
	SPU_MASTER_VOL_R = master;
	SPU_REVERB_VOL_L = reverb;
	SPU_REVERB_VOL_R = reverb;
}

void init(void);
int getFreeChannel(void);
void stopChannel(int ch);
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
	inline int play(int16_t volume = 0x3fff) const {
		return play(getFreeChannel(), volume);
	}

	bool initFromVAGHeader(const VAGHeader *header, uint32_t ramOffset);
	int play(int ch, int16_t volume = 0x3fff) const;
};

}
