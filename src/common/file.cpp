
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/file.hpp"
#include "common/gpu.hpp"
#include "common/util.hpp"
#include "ps1/pcdrv.h"
#include "vendor/ff.h"
#include "vendor/miniz.h"

namespace file {

static const char *const _FATFS_ERROR_NAMES[]{
	"OK",
	"DISK_ERR",
	"INT_ERR",
	"NOT_READY",
	"NO_FILE",
	"NO_PATH",
	"INVALID_NAME",
	"DENIED",
	"EXIST",
	"INVALID_OBJECT",
	"WRITE_PROTECTED",
	"INVALID_DRIVE",
	"NOT_ENABLED",
	"NO_FILESYSTEM",
	"MKFS_ABORTED",
	"TIMEOUT",
	"LOCKED",
	"NOT_ENOUGH_CORE",
	"TOO_MANY_OPEN_FILES",
	"INVALID_PARAMETER"
};

static const char *const _MINIZ_ZIP_ERROR_NAMES[]{
	"NO_ERROR",
	"UNDEFINED_ERROR",
	"TOO_MANY_FILES",
	"FILE_TOO_LARGE",
	"UNSUPPORTED_METHOD",
	"UNSUPPORTED_ENCRYPTION",
	"UNSUPPORTED_FEATURE",
	"FAILED_FINDING_CENTRAL_DIR",
	"NOT_AN_ARCHIVE",
	"INVALID_HEADER_OR_CORRUPTED",
	"UNSUPPORTED_MULTIDISK",
	"DECOMPRESSION_FAILED",
	"COMPRESSION_FAILED",
	"UNEXPECTED_DECOMPRESSED_SIZE",
	"CRC_CHECK_FAILED",
	"UNSUPPORTED_CDIR_SIZE",
	"ALLOC_FAILED",
	"FILE_OPEN_FAILED",
	"FILE_CREATE_FAILED",
	"FILE_WRITE_FAILED",
	"FILE_READ_FAILED",
	"FILE_CLOSE_FAILED",
	"FILE_SEEK_FAILED",
	"FILE_STAT_FAILED",
	"INVALID_PARAMETER",
	"INVALID_FILENAME",
	"BUF_TOO_SMALL",
	"INTERNAL_ERROR",
	"FILE_NOT_FOUND",
	"ARCHIVE_TOO_LARGE",
	"VALIDATION_FAILED",
	"WRITE_CALLBACK_FAILED"
};

/* File classes */

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

size_t FATFile::read(void *output, size_t length) {
	size_t actualLength;
	auto   error = f_read(&_fd, output, length, &actualLength);

	if (error) {
		LOG("%s, file=0x%08x", _FATFS_ERROR_NAMES[error], this);
		return 0;
	}

	return uint64_t(actualLength);
}

#ifdef ENABLE_FILE_WRITING
size_t FATFile::write(const void *input, size_t length) {
	size_t actualLength;
	auto   error = f_write(&_fd, input, length, &actualLength);

	if (error) {
		LOG("%s, file=0x%08x", _FATFS_ERROR_NAMES[error], this);
		return 0;
	}

	return uint64_t(actualLength);
}
#endif

uint64_t FATFile::seek(uint64_t offset) {
	auto error = f_lseek(&_fd, offset);

	if (error) {
		LOG("%s, file=0x%08x", _FATFS_ERROR_NAMES[error], this);
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

/* Directory classes */

Directory::~Directory(void) {
	close();
}

bool FATDirectory::getEntry(FileInfo &output) {
	FILINFO info;
	auto    error = f_readdir(&_fd, &info);

	if (error) {
		LOG("%s", _FATFS_ERROR_NAMES[error]);
		return false;
	}
	if (!info.fname[0])
		return false;

	__builtin_strncpy(output.name, info.fname, sizeof(output.name));
	output.length     = info.fsize;
	output.attributes = info.fattrib;

	return true;
}

void FATDirectory::close(void) {
	f_closedir(&_fd);
}

/* File and asset provider classes */

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

// TODO: these *really* belong somewhere else

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

bool FATProvider::init(const char *drive) {
	__builtin_strncpy(_drive, drive, sizeof(_drive));

	auto error = f_mount(&_fs, drive, 1);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], drive);
		return false;
	}

	LOG("FAT mount ok, drive=%s", drive);
	return true;
}

void FATProvider::close(void) {
	auto error = f_unmount(_drive);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return;
	}

	LOG("FAT unmount ok, drive=%s", _drive);
}

FileSystemType FATProvider::getFileSystemType(void) {
	return FileSystemType(_fs.fs_type);
}

uint64_t FATProvider::getCapacity(void) {
	if (!_fs.fs_type)
		return 0;

	size_t clusterSize = size_t(_fs.csize) * size_t(_fs.ssize);

	return uint64_t(_fs.n_fatent - 2) * uint64_t(clusterSize);
}

#ifdef ENABLE_FILE_WRITING
uint64_t FATProvider::getFreeSpace(void) {
	if (!_fs.fs_type)
		return 0;

	uint32_t count;
	FATFS    *dummy;
	auto     error = f_getfree(_drive, &count, &dummy);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return 0;
	}

	size_t clusterSize = size_t(_fs.csize) * size_t(_fs.ssize);

	return uint64_t(count) * uint64_t(clusterSize);
}
#endif

size_t FATProvider::getVolumeLabel(char *output, size_t length) {
	//assert(length >= 23);

	if (!_fs.fs_type)
		return 0;

	auto error = f_getlabel(_drive, output, nullptr);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return 0;
	}

	return __builtin_strlen(output);
}

uint32_t FATProvider::getSerialNumber(void) {
	if (!_fs.fs_type)
		return 0;

	uint32_t serial;
	auto     error = f_getlabel(_drive, nullptr, &serial);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return 0;
	}

	return serial;
}

bool FATProvider::_selectDrive(void) {
	if (!_fs.fs_type)
		return false;

	return !f_chdrive(_drive);
}

bool FATProvider::getFileInfo(FileInfo &output, const char *path) {
	if (!_selectDrive())
		return false;

	FILINFO info;
	auto    error = f_stat(path, &info);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return false;
	}

	__builtin_strncpy(output.name, info.fname, sizeof(output.name));
	output.length     = info.fsize;
	output.attributes = info.fattrib;

	return true;
}

Directory *FATProvider::openDirectory(const char *path) {
	if (!_selectDrive())
		return nullptr;

	auto dir   = new FATDirectory();
	auto error = f_opendir(&(dir->_fd), path);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		delete dir;
		return nullptr;
	}

	return dir;
}

#ifdef ENABLE_FILE_WRITING
bool FATProvider::createDirectory(const char *path) {
	if (!_selectDrive())
		return false;

	auto error = f_mkdir(path);

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		return false;
	}

	return true;
}
#endif

File *FATProvider::openFile(const char *path, uint32_t flags) {
	if (!_selectDrive())
		return nullptr;

	auto _file = new FATFile();
	auto error = f_open(&(_file->_fd), path, uint8_t(flags));

	if (error) {
		LOG("%s, drive=%s", _FATFS_ERROR_NAMES[error], _drive);
		delete _file;
		return nullptr;
	}

	_file->length = f_size(&(_file->_fd));
	return _file;
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

		LOG("%s, file=0x%08x", _MINIZ_ZIP_ERROR_NAMES[error], file);
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

		LOG("%s, ptr=0x%08x", _MINIZ_ZIP_ERROR_NAMES[error], zipData);
		return false;
	}

	LOG("ZIP init ok, ptr=0x%08x", zipData);
	return true;
}

void ZIPProvider::close(void) {
	mz_zip_reader_end(&_zip);

	if (_file) {
		_file->close();
		delete _file;
	}
}

FileSystemType ZIPProvider::getFileSystemType(void) {
	if (!_zip.m_zip_mode)
		return NONE;

	return _file ? ZIP_FILE : ZIP_MEMORY;
}

uint64_t ZIPProvider::getCapacity(void) {
	return _zip.m_archive_size;
}

bool ZIPProvider::getFileInfo(FileInfo &output, const char *path) {
	mz_zip_archive_file_stat info;

	int index = mz_zip_reader_locate_file(&_zip, path, nullptr, 0);

	if (index < 0)
		return false;
	if (!mz_zip_reader_file_stat(&_zip, index, &info))
		return false;
	if (!info.m_is_supported)
		return false;

	auto ptr = __builtin_strrchr(info.m_filename, '/');

	if (ptr)
		ptr++;
	else
		ptr = info.m_filename;

	__builtin_strncpy(output.name, ptr, sizeof(output.name));
	output.length     = info.m_uncomp_size;
	output.attributes = READ_ONLY | ARCHIVE;

	if (info.m_is_directory)
		output.attributes |= DIRECTORY;

	return true;
}

size_t ZIPProvider::loadData(util::Data &output, const char *path) {
	output.destroy();
	output.ptr = mz_zip_reader_extract_file_to_heap(
		&_zip, path, &(output.length), 0
	);

	if (!output.ptr) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, zip=0x%08x", _MINIZ_ZIP_ERROR_NAMES[error], this);
		return 0;
	}

	return output.length;
}

size_t ZIPProvider::loadData(void *output, size_t length, const char *path) {
	if (!mz_zip_reader_extract_file_to_mem(&_zip, path, output, length, 0)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG("%s, zip=0x%08x", _MINIZ_ZIP_ERROR_NAMES[error], this);
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
