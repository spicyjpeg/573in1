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
#include "common/blkdev/device.hpp"
#include "common/fs/file.hpp"
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* Virtual filesystem driver */

static constexpr char   VFS_PREFIX_SEPARATOR = ':';
static constexpr size_t MAX_VFS_MOUNT_POINTS = 8;
static constexpr size_t MAX_VFS_ALIASES      = 8;

struct VFSMountPoint {
public:
	util::Hash     prefix;
	size_t         pathOffset;
	blkdev::Device *dev;
	Provider       *provider;
};

struct VFSAlias {
public:
	util::Hash    prefix;
	size_t        pathOffset;
	VFSMountPoint *target;
};

class VFSProvider : public Provider {
public:
	VFSMountPoint mountPoints[MAX_VFS_MOUNT_POINTS];
	VFSAlias      aliases[MAX_VFS_ALIASES];

	inline VFSProvider(void) {
		type = VFS;

		util::clear(mountPoints);
		util::clear(aliases);
	}

	inline bool deleteMountPoint(const char *path) {
		return deleteMountPoint(getMountPoint(path));
	}
	inline bool addAlias(
		const char *prefix, const char *path, bool force = false
	) {
		return addAlias(prefix, getMountPoint(path), force);
	}

	VFSMountPoint *newMountPoint(const char *prefix, bool force = false);
	bool deleteMountPoint(VFSMountPoint *mp);
	bool addAlias(const char *prefix, VFSMountPoint *target, bool force = false);
	VFSMountPoint *getMountPoint(const char *path);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);
	bool createDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
	bool deleteFile(const char *path);
	size_t loadData(util::Data &output, const char *path);
	size_t loadData(void *output, size_t length, const char *path);
	size_t saveData(const void *input, size_t length, const char *path);
};

}
