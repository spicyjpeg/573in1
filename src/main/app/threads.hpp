/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

#include <stdint.h>
#include "common/fs/file.hpp"
#include "common/util/templates.hpp"
#include "common/spu.hpp"
#include "ps1/system.h"

/* Audio stream thread */

enum AudioStreamRequest {
	AUDIO_STREAM_STOP         = 0,
	AUDIO_STREAM_PLAY_ONCE    = 1,
	AUDIO_STREAM_PLAY_LOOPING = 2
};

class AudioStreamManager {
	friend void _streamMain(void *arg0, void *arg1);

private:
	AudioStreamRequest _request;

	fs::File *_file;
	Thread   *_yieldTo;

	Thread      _thread;
	util::Data  _stack;
	spu::Stream _stream;
	util::Data  _buffer;

	void _closeFile(void);
	void _startThread(AudioStreamRequest request, fs::File *file);

public:
	inline AudioStreamManager(void)
	:
		_request(AUDIO_STREAM_STOP),
		_file(nullptr),
		_yieldTo(nullptr) {}

	inline void init(Thread *yieldTo) {
		_yieldTo = yieldTo;
		_startThread(AUDIO_STREAM_STOP, nullptr);
	}
	inline spu::ChannelMask getChannelMask(void) const {
		return _stream.getChannelMask();
	}

	inline void yield(void) {
		switchThread(&_thread);
		forceThreadSwitch();
	}
	inline void handleInterrupt(void) {
		_stream.handleInterrupt();
	}

	bool play(fs::File *file, bool loop = false);
	void stop(void);
};
