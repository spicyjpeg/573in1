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

#include <stddef.h>
#include "common/fs/file.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/spu.hpp"
#include "ps1/system.h"
#include "main/app/threads.hpp"

/* Audio stream thread */

static constexpr size_t _STREAM_THREAD_STACK_SIZE = 0x2000;

static constexpr size_t _STREAM_BUFFERED_CHUNKS = 8;
static constexpr size_t _STREAM_MIN_FEED_CHUNKS = 4;

void _streamMain(void *arg0, void *arg1) {
	auto obj = reinterpret_cast<AudioStreamManager *>(arg0);

	if (obj->_status != AUDIO_STREAM_IDLE) {
		auto &stream = obj->_stream;
		auto &buffer = obj->_buffer;

		auto chunkLength = stream.getChunkLength();

		for (;;) {
			__atomic_signal_fence(__ATOMIC_ACQUIRE);

			if (obj->_status == AUDIO_STREAM_STOP) {
				stream.stop();
				break;
			}

			// Keep yielding to the worker thread until the stream's FIFO has
			// enough space for the new chunks.
			auto numChunks = stream.getFreeChunkCount();

			if (numChunks < _STREAM_MIN_FEED_CHUNKS) {
				switchThreadImmediate(obj->_yieldTo);
				continue;
			}

			auto length = obj->_file->read(buffer.ptr, chunkLength * numChunks);

			if (length >= chunkLength) {
				stream.feed(buffer.ptr, length);

				if (!stream.isPlaying())
					stream.start();
			} else if (obj->_status == AUDIO_STREAM_LOOP) {
				obj->_file->seek(spu::INTERLEAVED_VAG_BODY_OFFSET);
			} else {
				// Wait for any leftover data in the FIFO to finish playing,
				// then stop playback.
				while (!stream.isUnderrun())
					switchThreadImmediate(obj->_yieldTo);

				break;
			}
		}
	}

	// Do nothing and yield until the stream is restarted.
	obj->_status = AUDIO_STREAM_IDLE;
	flushWriteQueue();

	for (;;)
		switchThreadImmediate(obj->_yieldTo);
}

void AudioStreamManager::_closeFile(void) {
	if (!_file)
		return;

	_file->close();
	delete _file;

	_buffer.destroy();
	_file = nullptr;
}

void AudioStreamManager::_startThread(
	fs::File *file, AudioStreamStatus status
) {
	util::CriticalSection sec;

	_file   = file;
	_status = status;

	_stack.allocate(_STREAM_THREAD_STACK_SIZE);
	assert(_stack.ptr);

	auto stackBottom = _stack.as<uint8_t>();

	initThread(
		&_thread, &_streamMain, this, nullptr,
		&stackBottom[(_STREAM_THREAD_STACK_SIZE - 1) & ~7]
	);
}

bool AudioStreamManager::play(fs::File *file, bool loop) {
	if (isActive())
		stop();

	spu::VAGHeader header;

	if (file->read(&header, sizeof(header)) < sizeof(header))
		return false;
	if (
		file->seek(spu::INTERLEAVED_VAG_BODY_OFFSET)
		!= spu::INTERLEAVED_VAG_BODY_OFFSET
	)
		return false;

	auto bufferLength = _stream.getChunkLength() * _STREAM_MIN_FEED_CHUNKS;

	if (!_stream.initFromVAGHeader(header, 0x60000, _STREAM_BUFFERED_CHUNKS))
		return false;
	if (!_buffer.allocate(bufferLength))
		return false;

	_startThread(file, loop ? AUDIO_STREAM_LOOP : AUDIO_STREAM_PLAY_ONCE);
	return true;
}

void AudioStreamManager::stop(void) {
	if (!isActive())
		return;

	_status = AUDIO_STREAM_STOP;
	flushWriteQueue();

	while (isActive())
		yield();

	_closeFile();
}
