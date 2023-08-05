
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "ps1/pcdrv.h"
#include "vendor/ff.h"
#include "vendor/miniz.h"
#include "file.hpp"
#include "utilerror.hpp"

namespace file {

/* File classes */

size_t HostFile::read(void *output, size_t length) {
	int actualLength = pcdrvRead(_fd, output, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}

size_t HostFile::write(const void *input, size_t length) {
	int actualLength = pcdrvWrite(_fd, input, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}

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

size_t FATFile::read(void *output, size_t length) {
	size_t actualLength;
	auto   error = f_read(&_fd, output, length, &actualLength);

	if (error) {
		LOG("%s, file=0x%08x", util::getErrorString(error), this);
		return 0;
	}

	return uint64_t(actualLength);
}

size_t FATFile::write(const void *input, size_t length) {
	size_t actualLength;
	auto   error = f_write(&_fd, input, length, &actualLength);

	if (error) {
		LOG("%s, file=0x%08x", util::getErrorString(error), this);
		return 0;
	}

	return uint64_t(actualLength);
}

uint64_t FATFile::seek(uint64_t offset) {
	auto error = f_lseek(&_fd, offset);

	if (error) {
		LOG("%s, file=0x%08x", util::getErrorString(error), this);
		return 0;
	}

	return f_tell(&_fd);
}

uint64_t FATFile::tell(void) const {
	return f_tell(&_fd);
}

void FATFile::close(void) {
	f_close(&_fd);
}

/* File and asset provider classes */

uint32_t currentSPUOffset = spu::DUMMY_BLOCK_END;

bool Provider::fileExists(const char *path) {
	auto file = openFile(path, READ);

	if (!file)
		return false;

	file->close();
	return true;
}

size_t Provider::loadData(util::Data &output, const char *path) {
	auto file = openFile(path, READ);

	if (!file)
		return 0;

	//assert(file.length <= SIZE_MAX);
	if (!output.allocate(size_t(file->length)))
		return 0;

	size_t actualLength = file->read(output.ptr, output.length);
	file->close();

	return actualLength;
}

size_t Provider::loadData(void *output, size_t length, const char *path) {
	auto file = openFile(path, READ);

	if (!file)
		return 0;

	//assert(file.length >= length);
	size_t actualLength = file->read(output, length);
	file->close();

	return actualLength;
}

size_t Provider::saveData(const void *input, size_t length, const char *path) {
	auto file = openFile(path, WRITE | ALLOW_CREATE);

	if (!file)
		return 0;

	size_t actualLength = file->write(input, length);
	file->close();

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

bool HostProvider::init(void) {
	int error = pcdrvInit();

	if (error < 0) {
		LOG("PCDRV error, code=%d", error);
		return false;
	}

	return true;
}

bool HostProvider::createDirectory(const char *path) {
	int fd = pcdrvCreate(path, PCDRV_ATTR_DIRECTORY);

	if (fd < 0) {
		LOG("PCDRV error, code=%d", fd);
		return false;
	}

	pcdrvClose(fd);
	return true;
}

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

bool FATProvider::init(const char *drive) {
	auto error = f_mount(&_fs, drive, 1);

	if (error) {
		LOG("%s, drive=%s", util::getErrorString(error), drive);
		return false;
	}

	f_chdrive(drive);
	__builtin_strncpy(_drive, drive, sizeof(_drive));

	LOG("FAT mount ok, drive=%s", drive);
	return true;
}

void FATProvider::close(void) {
	auto error = f_unmount(_drive);

	if (error)
		LOG("%s, drive=%s", util::getErrorString(error), _drive);
	else
		LOG("FAT unmount ok, drive=%s", _drive);
}

bool FATProvider::fileExists(const char *path) {
	return !f_stat(path, nullptr);
}

bool FATProvider::createDirectory(const char *path) {
	auto error = f_mkdir(path);

	if (error) {
		LOG("%s, drive=%s", util::getErrorString(error), _drive);
		return false;
	}

	return true;
}

File *FATProvider::openFile(const char *path, uint32_t flags) {
	auto file  = new FATFile();
	auto error = f_open(&(file->_fd), path, uint8_t(flags));

	if (error) {
		LOG("%s, drive=%s", util::getErrorString(error), _drive);
		return nullptr;
	}

	file->length = f_size(&(file->_fd));
	return file;
}

static constexpr uint32_t _ZIP_FLAGS = 0
	| MZ_ZIP_FLAG_CASE_SENSITIVE
	| MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY;

bool ZIPProvider::init(File *file) {
	mz_zip_zero_struct(&_zip);
	_file = file;

	_zip.m_pIO_opaque       = reinterpret_cast<void *>(file);
	_zip.m_pNeeds_keepalive = nullptr;
	_zip.m_pRead            = [](
		void *opaque, uint64_t offset, void *output, size_t length
	) -> size_t {
		auto _file = reinterpret_cast<File *>(opaque);

		if (_file->seek(offset) != offset)
			return 0;

		return _file->read(output, length);
	};

	if (!mz_zip_reader_init(&_zip, file->length, _ZIP_FLAGS)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, file=0x%08x", util::getErrorString(error), file);
		return false;
	}

	LOG("ZIP init ok, file=0x%08x", file);
	return true;
}

bool ZIPProvider::init(const void *zipData, size_t length) {
	mz_zip_zero_struct(&_zip);
	_file = nullptr;

	if (!mz_zip_reader_init_mem(&_zip, zipData, length, _ZIP_FLAGS)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, ptr=0x%08x", util::getErrorString(error), zipData);
		return false;
	}

	LOG("ZIP init ok, ptr=0x%08x", zipData);
	return true;
}

void ZIPProvider::close(void) {
	mz_zip_reader_end(&_zip);

	if (_file)
		_file->close();
}

bool ZIPProvider::fileExists(const char *path) {
	return (mz_zip_reader_locate_file(&_zip, path, nullptr, 0) >= 0);
}

size_t ZIPProvider::loadData(util::Data &output, const char *path) {
	output.destroy();
	output.ptr = mz_zip_reader_extract_file_to_heap(
		&_zip, path, &(output.length), 0
	);

	if (!output.ptr) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, zip=0x%08x", util::getErrorString(error), this);
		return 0;
	}

	return output.length;
}

size_t ZIPProvider::loadData(void *output, size_t length, const char *path) {
	if (!mz_zip_reader_extract_file_to_mem(&_zip, path, output, length, 0)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, zip=0x%08x", util::getErrorString(error), this);
		return 0;
	}

	// FIXME: this may not reflect the file's actual length
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
