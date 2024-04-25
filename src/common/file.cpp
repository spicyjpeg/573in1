
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/file.hpp"
#include "common/gpu.hpp"
#include "common/util.hpp"
#include "ps1/pcdrv.h"

namespace file {

/* Base file and directory classes */

File::~File(void) {
	close();
}

size_t HostFile::read(void *output, size_t length) {
	int actualLength = pcdrvRead(_fd, output, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}

#ifdef ENABLE_FILE_WRITING
size_t HostFile::write(const void *input, size_t length) {
	int actualLength = pcdrvWrite(_fd, input, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}
#endif

uint64_t HostFile::seek(uint64_t offset) {
	int actualOffset = pcdrvSeek(_fd, int(offset), PCDRV_SEEK_SET);

	if (actualOffset < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualOffset, this);
		return 0;
	}

	return uint64_t(actualOffset);
}

uint64_t HostFile::tell(void) const {
	int actualOffset = pcdrvSeek(_fd, 0, PCDRV_SEEK_CUR);

	if (actualOffset < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualOffset, this);
		return 0;
	}

	return uint64_t(actualOffset);
}

void HostFile::close(void) {
	pcdrvClose(_fd);
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

	//assert(file->length <= SIZE_MAX);
	if (!output.allocate(size_t(_file->length))) {
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

	//assert(file->length >= length);
	size_t actualLength = _file->read(output, length);

	_file->close();
	delete _file;
	return actualLength;
}

size_t Provider::saveData(const void *input, size_t length, const char *path) {
#ifdef ENABLE_FILE_WRITING
	auto _file = openFile(path, WRITE | ALLOW_CREATE);

	if (!_file)
		return 0;

	size_t actualLength = _file->write(input, length);

	_file->close();
	delete _file;
	return actualLength;
#else
	return 0;
#endif
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

		magic        = 0x4d42;
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
#ifdef ENABLE_FILE_WRITING
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
#else
	return 0;
#endif
}

bool HostProvider::init(void) {
	int error = pcdrvInit();

	if (error < 0) {
		LOG("PCDRV error, code=%d", error);
		return false;
	}

	return true;
}

FileSystemType HostProvider::getFileSystemType(void) {
	return HOST;
}

#ifdef ENABLE_FILE_WRITING
bool HostProvider::createDirectory(const char *path) {
	int fd = pcdrvCreate(path, DIRECTORY);

	if (fd < 0) {
		LOG("PCDRV error, code=%d", fd);
		return false;
	}

	pcdrvClose(fd);
	return true;
}
#endif

File *HostProvider::openFile(const char *path, uint32_t flags) {
	PCDRVOpenMode mode = PCDRV_MODE_READ;

	if ((flags & (READ | WRITE)) == (READ | WRITE))
		mode = PCDRV_MODE_READ_WRITE;
	else if (flags & WRITE)
		mode = PCDRV_MODE_WRITE;

	int fd = pcdrvOpen(path, mode);

	if (fd < 0) {
		LOG("PCDRV error, code=%d", fd);
		return nullptr;
	}

	auto file = new HostFile();

	file->_fd    = fd;
	file->length = pcdrvSeek(fd, 0, PCDRV_SEEK_END);
	pcdrvSeek(fd, 0, PCDRV_SEEK_SET);

	return file;
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
