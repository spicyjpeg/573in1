
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/gpu.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "vendor/ff.h"
#include "vendor/miniz.h"

namespace file {

/* File classes */

static constexpr size_t MAX_PATH_LENGTH = 256;

// The first 4 of these map to the FS_* enum used by FatFs.
enum FileSystemType {
	NONE       = 0,
	FAT12      = 1,
	FAT16      = 2,
	FAT32      = 3,
	HOST       = 4,
	ZIP_MEMORY = 5,
	ZIP_FILE   = 6
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
	char     name[MAX_PATH_LENGTH];
	uint64_t length;
	uint32_t attributes;
};

class File {
public:
	uint64_t length;

	virtual ~File(void);

	virtual size_t read(void *output, size_t length) { return 0; }
	virtual size_t write(const void *input, size_t length) { return 0; }
	virtual uint64_t seek(uint64_t offset) { return 0; }
	virtual uint64_t tell(void) const { return 0; }
	virtual void close(void) {}
};

class HostFile : public File {
	friend class HostProvider;

private:
	int _fd;

public:
	size_t read(void *output, size_t length);
	size_t write(const void *input, size_t length);
	uint64_t seek(uint64_t offset);
	uint64_t tell(void) const;
	void close(void);
};

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

/* Directory classes */

class Directory {
public:
	virtual ~Directory(void);

	virtual bool getEntry(
		FileInfo &output, uint32_t attrMask, uint32_t attrValue
	) { return false; }
	virtual void close(void) {}
};

class FATDirectory : public Directory {
	friend class FATProvider;

private:
	DIR _fd;

public:
	bool getEntry(FileInfo &output, uint32_t attrMask, uint32_t attrValue);
	void close(void);
};

/* File and asset provider classes */

// TODO: move this (and loadTIM/loadVAG) somewhere else
extern uint32_t currentSPUOffset;

class Provider {
public:
	template<class T> inline size_t loadStruct(T &output, const char *path) {
		return loadData(&output, sizeof(output), path);
	}
	template<class T> inline size_t saveStruct(const T &input, const char *path) {
		return saveData(&input, sizeof(input), path);
	}

	virtual ~Provider(void);

	virtual void close(void) {}

	virtual FileSystemType getFileSystemType(void) { return NONE; }
	virtual uint64_t getCapacity(void) { return 0; }
	virtual uint64_t getFreeSpace(void) { return 0; }

	virtual bool getFileInfo(FileInfo &output, const char *path) { return false; }
	virtual Directory *openDirectory(const char *path) { return nullptr; }
	virtual bool createDirectory(const char *path) { return false; }

	virtual File *openFile(const char *path, uint32_t flags) { return nullptr; }
	virtual size_t loadData(util::Data &output, const char *path);
	virtual size_t loadData(void *output, size_t length, const char *path);
	virtual size_t saveData(const void *input, size_t length, const char *path);
	size_t loadTIM(gpu::Image &output, const char *path);
	size_t loadVAG(spu::Sound &output, const char *path);
};

class HostProvider : public Provider {
public:
	bool init(void);

	FileSystemType getFileSystemType(void);

	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

class FATProvider : public Provider {
private:
	FATFS _fs;
	char  _drive[8];

	bool _selectDrive(void);

public:
	inline FATProvider(void) {
		_fs.fs_type = 0;
		_drive[0]   = 0;
	}

	bool init(const char *drive);
	void close(void);

	FileSystemType getFileSystemType(void);
	uint64_t getCapacity(void);
	uint64_t getFreeSpace(void);
	size_t getVolumeLabel(char *output, size_t length);
	uint32_t getSerialNumber(void);

	bool getFileInfo(FileInfo &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

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

/* String table parser */

static constexpr int TABLE_BUCKET_COUNT = 256;

struct [[gnu::packed]] StringTableEntry {
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
