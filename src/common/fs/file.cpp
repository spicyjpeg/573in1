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
#include "common/fs/file.hpp"
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "common/gpu.hpp"

namespace fs {

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

Provider::~Provider(void) {
	close();
}

size_t Provider::loadData(util::Data &output, const char *path) {
	auto file = openFile(path, READ);

	if (!file)
		return 0;

	assert(file->size <= SIZE_MAX);
	if (!output.allocate(size_t(file->size))) {
		file->close();
		delete file;
		return 0;
	}

	size_t actualLength = file->read(output.ptr, output.length);

	file->close();
	delete file;
	return actualLength;
}

size_t Provider::loadData(void *output, size_t length, const char *path) {
	auto file = openFile(path, READ);

	if (!file)
		return 0;

	assert(file->size >= length);
	size_t actualLength = file->read(output, length);

	file->close();
	delete file;
	return actualLength;
}

size_t Provider::saveData(const void *input, size_t length, const char *path) {
	auto file = openFile(path, WRITE | ALLOW_CREATE);

	if (!file)
		return 0;

	size_t actualLength = file->write(input, length);

	file->close();
	delete file;
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

	size_t uploadLength = 0;

	if (header->flags & (1 << 3)) {
		auto clut     = reinterpret_cast<const gpu::TIMSectionHeader *>(section);
		uploadLength += gpu::upload(clut->vram, &clut[1], true);

		section += clut->length;
	}

	auto image    = reinterpret_cast<const gpu::TIMSectionHeader *>(section);
	uploadLength += gpu::upload(image->vram, &image[1], true);

	data.destroy();
	return uploadLength;
}

size_t Provider::loadVAG(
	spu::Sound &output, uint32_t offset, const char *path
) {
	// Sounds should be decompressed and uploaded to the SPU one chunk at a
	// time, but whatever.
	util::Data data;

	if (!loadData(data, path))
		return 0;

	auto header = data.as<const spu::VAGHeader>();
	auto body   = reinterpret_cast<const uint32_t *>(&header[1]);

	if (!output.initFromVAGHeader(header, offset)) {
		data.destroy();
		return 0;
	}

	auto uploadLength =
		spu::upload(offset, body, data.length - sizeof(spu::VAGHeader), true);

	data.destroy();
	return uploadLength;
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

		magic        = util::concat2('B', 'M');
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
	auto file = openFile(path, WRITE | ALLOW_CREATE);

	if (!file)
		return 0;

	BMPHeader header;

	header.init(rect.w, rect.h, 16);

	size_t     length = file->write(&header, sizeof(header));
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

			length += file->write(buffer.ptr, lineLength);
		}

		buffer.destroy();
	}

	file->close();
	delete file;

	return length;
}

/* String table parser */

static const char _ERROR_STRING[]{ "missingno" };

const char *StringTable::get(util::Hash id) const {
	if (!ptr)
		return _ERROR_STRING;

	auto blob  = as<const char>();
	auto table = as<const StringTableEntry>();
	auto index = id % STRING_TABLE_BUCKET_COUNT;

	do {
		auto entry = &table[index];
		index      = entry->chained;

		if (entry->hash == id)
			return &blob[entry->offset];
	} while (index);

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
