
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "vendor/ff.h"
#include "vendor/miniz.h"
#include "gpu.hpp"
#include "spu.hpp"
#include "util.hpp"

namespace file {

/* File classes */

// These are functionally equivalent to the FA_* flags used by FatFs.
enum FileModeFlag {
	READ         = 1 << 0,
	WRITE        = 1 << 1,
	FORCE_CREATE = 1 << 3, // Create file if missing, truncate if it exists
	ALLOW_CREATE = 1 << 4  // Create file if missing
};

class File {
public:
	uint64_t length;

	inline ~File(void) {
		close();
	}

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

/* File and asset provider classes */

extern uint32_t currentSPUOffset;

class Provider {
public:
	inline ~Provider(void) {
		close();
	}

	template<class T> inline size_t loadStruct(T &output, const char *path) {
		return loadData(&output, sizeof(output), path);
	}
	template<class T> inline size_t saveStruct(const T &input, const char *path) {
		return saveData(&input, sizeof(input), path);
	}

	virtual void close(void) {}

	virtual bool fileExists(const char *path);
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

	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

class FATProvider : public Provider {
private:
	FATFS _fs;
	char _drive[16];

public:
	bool init(const char *drive);
	void close(void);

	bool fileExists(const char *path);
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

	bool fileExists(const char *path);

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
