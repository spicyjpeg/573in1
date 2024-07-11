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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/file/file.hpp"
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
