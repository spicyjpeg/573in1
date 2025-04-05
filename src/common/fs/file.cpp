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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/fs/file.hpp"
#include "common/util/templates.hpp"
#include "common/gpu.hpp"
#include "common/mdec.hpp"
#include "common/spu.hpp"
#include "ps1/registers.h"

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

bool Provider::getNumberedPath(
	char       *output,
	size_t     length,
	const char *path,
	int        maxIndex
) {
	fs::FileInfo info;

	// Perform a binary search in order to quickly find the first unused path.
	int low  = 0;
	int high = maxIndex;

	while (low <= high) {
		int index = low + (high - low) / 2;

		snprintf(output, length, path, index);

		if (getFileInfo(info, output))
			low = index + 1;
		else
			high = index - 1;
	}

	if (low > maxIndex)
		return false;

	snprintf(output, length, path, low);
	return true;
}

size_t Provider::loadTIM(
	gpu::Image     &output,
	const char     *path,
	gpu::BlendMode blendMode
) {
	util::Data data;
	size_t     loadLength = 0;

	if (!loadData(data, path))
		return 0;

	auto header = data.as<gpu::TIMHeader>();

	if (output.initFromTIMHeader(*header, blendMode)) {
		auto image = header->getImage();
		auto clut  = header->getCLUT();

		if (clut)
			loadLength += gpu::upload(clut->vram, clut->getData(), true);

		loadLength += gpu::upload(image->vram, image->getData(), true);
	}

	return loadLength;
}

size_t Provider::loadBS(
	gpu::Image        &output,
	const gpu::RectWH &rect,
	const char        *path
) {
	util::Data data;
	size_t     loadLength = 0;

	if (!loadData(data, path))
		return 0;

	size_t bsLength = data.as<mdec::BSHeader>()->getUncompLength();

	mdec::BSDecompressor decompressor;
	util::Data           buffer;

	if (buffer.allocate(bsLength)) {
		auto   bsPtr       = buffer.as<uint32_t>();
		size_t sliceLength = (16 * rect.h) * 2;

		if (!decompressor.decompress(bsPtr, data.ptr, bsLength)) {
			// Reuse the file's buffer to store vertical slices received from
			// the MDEC as they are uploaded to VRAM.
			data.allocate(sliceLength);
			mdec::feedDecodedBS(bsPtr, MDEC_CMD_FORMAT_16BPP, false);

			gpu::RectWH slice;

			slice.x = rect.x;
			slice.y = rect.y;
			slice.w = 16;
			slice.h = rect.h;

			for (int i = rect.w; i > 0; i -= 16) {
				mdec::receive(data.ptr, sliceLength, true);

				loadLength += gpu::upload(slice, data.ptr, true);
				slice.x    += 16;
			}
		}
	}

	return loadLength;
}

size_t Provider::loadVAG(
	spu::Sound &output,
	uint32_t   offset,
	const char *path
) {
	// Sounds should be decompressed and uploaded to the SPU one chunk at a
	// time, but whatever.
	util::Data data;
	size_t     loadLength = 0;

	if (!loadData(data, path))
		return 0;

	auto header = data.as<spu::VAGHeader>();

	if (output.initFromVAGHeader(*header, offset))
		loadLength = spu::upload(
			offset, header->getData(), data.length - sizeof(spu::VAGHeader),
			true
		);

	return loadLength;
}

class [[gnu::packed]] BMPHeader {
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

		magic        = "BM"_c;
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

size_t Provider::saveVRAMBMP(const gpu::RectWH &rect, const char *path) {
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
		slice.y = rect.y + rect.h - 1;
		slice.w = rect.w;
		slice.h = 1;

		for (; slice.y >= rect.y; slice.y--) {
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
	}

	file->close();
	delete file;

	return length;
}

}
