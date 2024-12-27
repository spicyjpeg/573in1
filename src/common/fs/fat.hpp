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
#include "common/fs/file.hpp"
#include "common/storage/device.hpp"
#include "vendor/ff.h"

namespace fs {

/* FAT file and directory classes */

class FATFile : public File {
	friend class FATProvider;

private:
	FIL _fd;

public:
	size_t read(void *output, size_t length);
	size_t write(const void *input, size_t length);
	uint64_t seek(uint64_t offset);
	uint64_t tell(void) const;
	void close(void);
};

class FATDirectory : public Directory {
	friend class FATProvider;

private:
	DIR _fd;

public:
	bool getEntry(FileInfo &output);
	void close(void);
};

/* FAT filesystem provider */

class FATProvider : public Provider {
private:
	FATFS _fs;

public:
	inline FATProvider(void) {
		_fs.fs_type = 0;
	}

	bool init(storage::Device &dev, int mutexID);
	void close(void);
	uint64_t getFreeSpace(void);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
	bool deleteFile(const char *path);
};

}
