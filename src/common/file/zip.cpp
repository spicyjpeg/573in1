
#include <stddef.h>
#include <stdint.h>
#include "common/file/file.hpp"
#include "common/file/zip.hpp"
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

/* Utilities */

static bool _zipStatToFileInfo(
	FileInfo &output, const mz_zip_archive_file_stat &stat
) {
	// Ignore all unsupported files.
	if (!stat.m_is_supported)
		return false;

#if 0
	auto ptr = __builtin_strrchr(stat.m_filename, '/');

	if (ptr)
		ptr++;
	else
		ptr = (char *) stat.m_filename;
#else
	auto ptr = stat.m_filename;
#endif

	__builtin_strncpy(output.name, ptr, sizeof(output.name));
	output.size       = stat.m_uncomp_size;
	output.attributes = READ_ONLY | ARCHIVE;

	if (stat.m_is_directory)
		output.attributes |= DIRECTORY;

	return true;
}

/* ZIP directory class */

bool ZIPDirectory::getEntry(FileInfo &output) {
	mz_zip_archive_file_stat stat;

	while (_index < _zip->m_total_files) {
		if (!mz_zip_reader_file_stat(_zip, _index++, &stat))
			continue;
		if (!_zipStatToFileInfo(output, stat))
			continue;

		return true;
	}

	return false;
}

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

	if (!mz_zip_reader_init(&_zip, file->size, _ZIP_FLAGS)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG_FS("%s", _MINIZ_ZIP_ERROR_NAMES[error]);
		return false;
	}

	type     = ZIP_FILE;
	capacity = _zip.m_archive_size;

	LOG_FS("mounted ZIP file");
	return true;
}

bool ZIPProvider::init(const void *zipData, size_t length) {
	mz_zip_zero_struct(&_zip);
	_file = nullptr;

	if (!mz_zip_reader_init_mem(&_zip, zipData, length, _ZIP_FLAGS)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG_FS("%s: 0x%08x", _MINIZ_ZIP_ERROR_NAMES[error], zipData);
		return false;
	}

	type     = ZIP_MEMORY;
	capacity = _zip.m_archive_size;

	LOG_FS("mounted ZIP: 0x%08x", zipData);
	return true;
}

void ZIPProvider::close(void) {
	mz_zip_reader_end(&_zip);

#if 0
	if (_file) {
		_file->close();
		delete _file;
	}
#endif

	type     = NONE;
	capacity = 0;
}

bool ZIPProvider::getFileInfo(FileInfo &output, const char *path) {
	// Any leading path separators must be stripped manually.
	while ((*path == '/') || (*path == '\\'))
		path++;

	mz_zip_archive_file_stat stat;

	int index = mz_zip_reader_locate_file(&_zip, path, nullptr, 0);

	if (index < 0)
		return false;
	if (!mz_zip_reader_file_stat(&_zip, index, &stat))
		return false;

	return _zipStatToFileInfo(output, stat);
}

Directory *ZIPProvider::openDirectory(const char *path) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	// ZIP subdirectories are not currently handled; all files are instead
	// returned as if they were part of the root directory.
	if (*path)
		return nullptr;

	return new ZIPDirectory(_zip);
}

size_t ZIPProvider::loadData(util::Data &output, const char *path) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	output.destroy();
	output.ptr = mz_zip_reader_extract_file_to_heap(
		&_zip, path, &(output.length), 0
	);

	if (!output.ptr) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG_FS("%s: %s", _MINIZ_ZIP_ERROR_NAMES[error], path);
		return 0;
	}

	return output.length;
}

size_t ZIPProvider::loadData(void *output, size_t length, const char *path) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	if (!mz_zip_reader_extract_file_to_mem(&_zip, path, output, length, 0)) {
		auto error = mz_zip_get_last_error(&_zip);

		LOG_FS("%s: %s", _MINIZ_ZIP_ERROR_NAMES[error], path);
		return 0;
	}

	// FIXME: this may not reflect the file's actual length
	return length;
}

}
