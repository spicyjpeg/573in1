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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/file/file.hpp"
#include "common/gpu.hpp"
#include "common/util.hpp"

namespace file {

/* File fragment table */

uint64_t FileFragment::getLBA(uint64_t sector, size_t tableLength) const {
	auto fragment = this;

	for (; tableLength; tableLength--, fragment++) {
		if (sector < fragment->length)
			return fragment->lba + sector;

		sector -= fragment->length;
	}

	return 0;
}

/* Base file and directory classes */

File::~File(void) {
	close();
}

Directory::~Directory(void) {
	close();
}

/* Base file and asset provider classes */

uint32_t currentSPUOffset = spu::DUMMY_BLOCK_END;

Provider::~Provider(void) {
	close();
}

size_t Provider::loadData(util::Data &output, const char *path) {
	auto _file = openFile(path, READ);

	if (!_file)
		return 0;

	//assert(_file->size <= SIZE_MAX);
	if (!output.allocate(size_t(_file->size))) {
		_file->close();
		delete _file;
		return 0;
	}

	size_t actualLength = _file->read(output.ptr, output.length);

	_file->close();
	delete _file;
	return actualLength;
}

size_t Provider::loadData(void *output, size_t length, const char *path) {
	auto _file = openFile(path, READ);

	if (!_file)
		return 0;

	//assert(file->size >= length);
	size_t actualLength = _file->read(output, length);

	_file->close();
	delete _file;
	return actualLength;
}

size_t Provider::saveData(const void *input, size_t length, const char *path) {
	auto _file = openFile(path, WRITE | ALLOW_CREATE);

	if (!_file)
		return 0;

	size_t actualLength = _file->write(input, length);

	_file->close();
	delete _file;
	return actualLength;
}

size_t Provider::loadTIM(gpu::Image &output, const char *path) {
	util::Data data;

	if (!loadData(data, path))
		return 0;

	auto header  = data.as<const gpu::TIMHeader>();
	auto section = reinterpret_cast<const uint8_t *>(&header[1]);

	if (!output.initFromTIMHeader(header)) {
		data.destroy();
		return 0;
	}
	if (header->flags & (1 << 3)) {
		auto clut = reinterpret_cast<const gpu::TIMSectionHeader *>(section);

		gpu::upload(clut->vram, &clut[1], true);
		section += clut->length;
	}

	auto image = reinterpret_cast<const gpu::TIMSectionHeader *>(section);

	gpu::upload(image->vram, &image[1], true);

	data.destroy();
	return data.length;
}

size_t Provider::loadVAG(spu::Sound &output, const char *path) {
	// Sounds should be decompressed and uploaded to the SPU one chunk at a
	// time, but whatever.
	util::Data data;

	if (!loadData(data, path))
		return 0;

	auto header = data.as<const spu::VAGHeader>();
	auto body   = reinterpret_cast<const uint32_t *>(&header[1]);

	if (!output.initFromVAGHeader(header, currentSPUOffset)) {
		data.destroy();
		return 0;
	}

	currentSPUOffset += spu::upload(
		currentSPUOffset, body, data.length - sizeof(spu::VAGHeader), true
	);

	data.destroy();
	return data.length;
}

struct [[gnu::packed]] BMPHeader {
public:
	uint16_t magic;
	uint32_t fileLength;
	uint8_t  _reserved[4];
	uint32_t dataOffset;

	uint32_t headerLength, width, height;
	uint16_t numPlanes, bpp;
	uint32_t compType, dataLength, ppmX, ppmY, numColors, numColors2;

	inline void init(int _width, int _height, int _bpp) {
		util::clear(*this);

		size_t length = _width * _height * _bpp / 8;

		magic        = util::concatenate('B', 'M');
		fileLength   = sizeof(BMPHeader) + length;
		dataOffset   = sizeof(BMPHeader);
		headerLength = sizeof(BMPHeader) - offsetof(BMPHeader, headerLength);
		width        = _width;
		height       = _height;
		numPlanes    = 1;
		bpp          = _bpp;
		dataLength   = length;
	}
};

size_t Provider::saveVRAMBMP(gpu::RectWH &rect, const char *path) {
	auto _file = openFile(path, WRITE | ALLOW_CREATE);

	if (!_file)
		return 0;

	BMPHeader header;

	header.init(rect.w, rect.h, 16);

	size_t     length = _file->write(&header, sizeof(header));
	util::Data buffer;

	if (buffer.allocate<uint16_t>(rect.w + 32)) {
		// Read the image from VRAM one line at a time from the bottom up, as
		// the BMP format stores lines in reversed order.
		gpu::RectWH slice;

		slice.x = rect.x;
		slice.w = rect.w;
		slice.h = 1;

		for (int y = rect.y + rect.h - 1; y >= rect.y; y--) {
			slice.y         = y;
			auto lineLength = gpu::download(slice, buffer.ptr, true);

			// BMP stores channels in BGR order as opposed to RGB, so the red
			// and blue channels must be swapped.
			auto ptr = buffer.as<uint16_t>();

			for (int i = lineLength; i > 0; i -= 2) {
				uint16_t value = *ptr, newValue;

				newValue  = (value & (31 <<  5));
				newValue |= (value & (31 << 10)) >> 10;
				newValue |= (value & (31 <<  0)) << 10;
				*(ptr++)  = newValue;
			}

			length += _file->write(buffer.ptr, lineLength);
		}

		buffer.destroy();
	}

	_file->close();
	delete _file;

	return length;
}

/* String table parser */

static const char _ERROR_STRING[]{ "missingno" };

const char *StringTable::get(util::Hash id) const {
	if (!ptr)
		return _ERROR_STRING;

	auto blob  = reinterpret_cast<const char *>(ptr);
	auto table = reinterpret_cast<const StringTableEntry *>(ptr);

	auto entry = &table[id % TABLE_BUCKET_COUNT];

	if (entry->hash == id)
		return &blob[entry->offset];

	while (entry->chained) {
		entry = &table[entry->chained];

		if (entry->hash == id)
			return &blob[entry->offset];
	}

	return _ERROR_STRING;
}

size_t StringTable::format(
	char *buffer, size_t length, util::Hash id, ...
) const {
	va_list ap;

	va_start(ap, id);
	size_t outLength = vsnprintf(buffer, length, get(id), ap);
	va_end(ap);

	return outLength;
}

}
