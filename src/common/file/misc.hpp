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
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"
#include "ps1/pcdrv.h"

namespace file {

/* PCDRV driver */

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

class HostDirectory : public Directory {
	friend class HostProvider;

private:
	int           _fd;
	PCDRVDirEntry _entry;

public:
	bool getEntry(FileInfo &output);
};

class HostProvider : public Provider {
public:
	bool init(void);

	bool getFileInfo(FileInfo &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

/* Virtual filesystem driver */

static constexpr char   VFS_PREFIX_SEPARATOR = ':';
static constexpr size_t MAX_VFS_MOUNT_POINTS = 8;

struct VFSMountPoint {
public:
	util::Hash prefix;
	size_t     pathOffset;
	Provider   *provider;
};

class VFSProvider : public Provider {
private:
	VFSMountPoint _mountPoints[MAX_VFS_MOUNT_POINTS];

	VFSMountPoint *_getMounted(const char *path);

public:
	inline VFSProvider(void) {
		type = VFS;

		__builtin_memset(_mountPoints, 0, sizeof(_mountPoints));
	}

	bool mount(const char *prefix, Provider *provider, bool force = false);
	bool unmount(const char *prefix);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
	size_t loadData(util::Data &output, const char *path);
	size_t loadData(void *output, size_t length, const char *path);
	size_t saveData(const void *input, size_t length, const char *path);
};

}
