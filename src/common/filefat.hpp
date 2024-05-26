
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/file.hpp"
#include "vendor/ff.h"

namespace file {

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
	char  _drive[4];

	bool _selectDrive(void);

public:
	inline FATProvider(void) {
		_fs.fs_type = 0;
		_drive[0]   = '#';
		_drive[1]   = ':';
		_drive[2]   = 0;
	}

	bool init(int drive);
	void close(void);
	uint64_t getFreeSpace(void);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

}
