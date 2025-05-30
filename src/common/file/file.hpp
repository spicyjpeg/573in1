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
#include "common/gpu.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"

namespace file {

/* Common structures */

static constexpr size_t MAX_NAME_LENGTH = 64;
static constexpr size_t MAX_PATH_LENGTH = 256;

// The first 4 of these map to the FS_* enum used by FatFs.
enum FileSystemType {
	NONE       = 0,
	FAT12      = 1,
	FAT16      = 2,
	FAT32      = 3,
	EXFAT      = 4,
	ISO9660    = 5,
	ZIP_MEMORY = 6,
	ZIP_FILE   = 7,
	HOST       = 8,
	VFS        = 9
};

// These are functionally equivalent to the FA_* flags used by FatFs.
enum FileModeFlag {
	READ         = 1 << 0,
	WRITE        = 1 << 1,
	FORCE_CREATE = 1 << 3, // Create file if missing, truncate if it exists
	ALLOW_CREATE = 1 << 4  // Create file if missing
};

// These are equivalent to the standard MS-DOS file attributes (as well as PCDRV
// attributes and the AM_* flags used by FatFs).
enum FileAttributeFlag {
	READ_ONLY = 1 << 0,
	HIDDEN    = 1 << 1,
	SYSTEM    = 1 << 2,
	DIRECTORY = 1 << 4,
	ARCHIVE   = 1 << 5
};

struct FileInfo {
public:
	char     name[MAX_NAME_LENGTH];
	uint64_t size;
	uint32_t attributes;
};

/* File fragment table */

class FileFragment {
public:
	uint64_t lba, length;

	uint64_t getLBA(uint64_t sector, size_t tableLength) const;
};

class FileFragmentTable : public util::Data {
public:
	inline size_t getNumFragments(void) const {
		return length / sizeof(FileFragment);
	}
	inline uint64_t getLBA(uint64_t sector) const {
		return as<FileFragment>()->getLBA(sector, getNumFragments());
	}
};

/* Base file and directory classes */

class File {
public:
	uint64_t size;

	virtual ~File(void);

	virtual size_t read(void *output, size_t length) { return 0; }
	virtual size_t write(const void *input, size_t length) { return 0; }
	virtual uint64_t seek(uint64_t offset) { return 0; }
	virtual uint64_t tell(void) const { return 0; }
	virtual void close(void) {}
};

class Directory {
public:
	virtual ~Directory(void);

	virtual bool getEntry(FileInfo &output) { return false; }
	virtual void close(void) {}
};

/* Base file and asset provider classes */

extern uint32_t currentSPUOffset;

class Provider {
public:
	FileSystemType type;
	uint32_t       serialNumber;
	uint64_t       capacity;
	char           volumeLabel[MAX_NAME_LENGTH];

	inline Provider(void)
	: type(NONE), serialNumber(0), capacity(0) {
		volumeLabel[0] = 0;
	}
	template<class T> inline size_t loadStruct(T &output, const char *path) {
		return loadData(&output, sizeof(output), path);
	}
	template<class T> inline size_t saveStruct(const T &input, const char *path) {
		return saveData(&input, sizeof(input), path);
	}

	virtual ~Provider(void);

	virtual void close(void) {}
	virtual uint64_t getFreeSpace(void) { return 0; }

	virtual bool getFileInfo(FileInfo &output, const char *path) {
		return false;
	}
	virtual bool getFileFragments(FileFragmentTable &output, const char *path) {
		return false;
	}
	virtual Directory *openDirectory(const char *path) { return nullptr; }
	virtual bool createDirectory(const char *path) { return false; }

	virtual File *openFile(const char *path, uint32_t flags) { return nullptr; }
	virtual size_t loadData(util::Data &output, const char *path);
	virtual size_t loadData(void *output, size_t length, const char *path);
	virtual size_t saveData(const void *input, size_t length, const char *path);

	size_t loadTIM(gpu::Image &output, const char *path);
	size_t loadVAG(spu::Sound &output, const char *path);
	size_t saveVRAMBMP(gpu::RectWH &rect, const char *path);
};

/* String table parser */

static constexpr int TABLE_BUCKET_COUNT = 256;

struct StringTableEntry {
public:
	uint32_t hash;
	uint16_t offset, chained;
};

class StringTable : public util::Data {
public:
	inline const char *operator[](util::Hash id) const {
		return get(id);
	}

	const char *get(util::Hash id) const;
	size_t format(char *buffer, size_t length, util::Hash id, ...) const;
};

}
