
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/file.hpp"
#include "vendor/miniz.h"

namespace file {

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

	FileSystemType getFileSystemType(void);
	uint64_t getCapacity(void);

	bool getFileInfo(FileInfo &output, const char *path);

	size_t loadData(util::Data &output, const char *path);
	size_t loadData(void *output, size_t length, const char *path);
};

}
