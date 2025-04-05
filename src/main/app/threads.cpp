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

#include <stddef.h>
#include <stdint.h>
#include "common/fs/file.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/spu.hpp"
#include "main/app/threads.hpp"
#include "ps1/system.h"

/* Audio stream thread */

static constexpr size_t _STREAM_THREAD_STACK_SIZE = 0x2000;

static constexpr size_t _STREAM_BUFFERED_CHUNKS = 16;
static constexpr size_t _STREAM_MIN_FEED_CHUNKS = 8;

void _streamMain(void *arg0, void *arg1) {
	auto obj     = reinterpret_cast<AudioStreamManager *>(arg0);
	auto &stream = obj->_stream;
	auto &buffer = obj->_buffer;

	auto chunkLength = stream.getChunkLength();

	for (;;) {
		__atomic_signal_fence(__ATOMIC_ACQUIRE);

		if (obj->_request == AUDIO_STREAM_STOP)
			break;

		// Keep yielding to the worker thread until the stream's FIFO has
		// enough space for the new chunks.
		auto numChunks = stream.getFreeChunkCount();

		if (numChunks <= _STREAM_MIN_FEED_CHUNKS) {
			switchThread(obj->_yieldTo);
			forceThreadSwitch();
			continue;
		}

		auto length = obj->_file->read(buffer.ptr, chunkLength * numChunks);

		if (length >= chunkLength) {
			stream.feed(buffer.ptr, length);

			if (!stream.getChannelMask())
				stream.start();
		} else if (obj->_request == AUDIO_STREAM_PLAY_LOOPING) {
			obj->_file->seek(spu::INTERLEAVED_VAG_BODY_OFFSET);
		} else if (obj->_request == AUDIO_STREAM_PLAY_ONCE) {
			// Wait for any leftover data in the FIFO to finish playing, then
			// stop playback.
			while (!stream.isUnderrun()) {
				switchThread(obj->_yieldTo);
				forceThreadSwitch();
			}

			break;
		}
	}

	// Do nothing and yield until the stream is restarted.
	stream.stop();

	for (;;) {
		switchThread(obj->_yieldTo);
		forceThreadSwitch();
	}
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
	AudioStreamRequest request,
	fs::File           *file
) {
	util::CriticalSection sec;

	_request = request;
	_file    = file;

	if (!_stack.ptr) {
		_stack.allocate(_STREAM_THREAD_STACK_SIZE);
		assert(_stack.ptr);
	}

	auto stackPtr = _stack.as<uint8_t>();
	auto stackTop = &stackPtr[(_STREAM_THREAD_STACK_SIZE - 1) & ~7];

	initThread(&_thread, &_streamMain, this, nullptr, stackTop);
	LOG_APP("stack: 0x%08x-0x%08x", stackPtr, stackTop);
}

bool AudioStreamManager::play(fs::File *file, bool loop) {
	if (_stream.getChannelMask())
		stop();

	spu::VAGHeader header;

	if (file->read(&header, sizeof(header)) < sizeof(header))
		return false;
	if (
		file->seek(spu::INTERLEAVED_VAG_BODY_OFFSET)
		!= spu::INTERLEAVED_VAG_BODY_OFFSET
	)
		return false;

	size_t chunkLength     = header.interleave * header.getNumChannels();
	size_t spuBufferLength = chunkLength * _STREAM_BUFFERED_CHUNKS;
	size_t ramBufferLength = chunkLength * _STREAM_MIN_FEED_CHUNKS;

	if (!_stream.initFromVAGHeader(
		header,
		spu::SPU_RAM_END - spuBufferLength,
		_STREAM_BUFFERED_CHUNKS
	))
		return false;
	if (!_buffer.allocate(ramBufferLength))
		return false;

	_startThread(
		loop ? AUDIO_STREAM_PLAY_LOOPING : AUDIO_STREAM_PLAY_ONCE,
		file
	);

	while (!_stream.getChannelMask())
		yield();

	return true;
}

void AudioStreamManager::stop(void) {
	if (!_stream.getChannelMask())
		return;

	_request = AUDIO_STREAM_STOP;
	flushWriteQueue();

	while (_stream.getChannelMask())
		yield();

	_closeFile();
}
