
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/file.hpp"
#include "vendor/miniz.h"

namespace file {

/* ZIP directory class */

class ZIPDirectory : public Directory {
private:
	mz_zip_archive *_zip;
	size_t         _index;

public:
	inline ZIPDirectory(mz_zip_archive &zip)
	: _zip(&zip), _index(0) {}

	bool getEntry(FileInfo &output);
};

/* ZIP filesystem provider */

// This implementation only supports loading an entire file at once.
class ZIPProvider : public Provider {
private:
	mz_zip_archive _zip;
	File           *_file;

public:
	bool init(File *file);
	bool init(const void *zipData, size_t length);
	void close(void);

	bool getFileInfo(FileInfo &output, const char *path);
	Directory *openDirectory(const char *path);

	size_t loadData(util::Data &output, const char *path);
	size_t loadData(void *output, size_t length, const char *path);
};

}
