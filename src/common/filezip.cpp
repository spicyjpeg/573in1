
#include <stddef.h>
#include <stdint.h>
#include "common/file.hpp"
#include "common/filezip.hpp"
#include "common/util.hpp"
#include "vendor/miniz.h"

namespace file {

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

/* ZIP filesystem provider */

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

}
