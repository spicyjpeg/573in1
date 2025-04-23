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

#include <stddef.h>
#include <stdint.h>
#include "common/fs/file.hpp"
#include "common/fs/vfs.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* Virtual filesystem driver */

VFSMountPoint *VFSProvider::newMountPoint(const char *prefix, bool force) {
	auto          hash       = util::hash(prefix, VFS_PREFIX_SEPARATOR);
	VFSMountPoint *allocated = nullptr;

	for (auto &mp : mountPoints) {
		if (!mp.prefix) {
			allocated = &mp;
			break;
		}
		if (mp.prefix == hash) {
			if (force) {
				allocated = &mp;
				break;
			}

			LOG_FS("already present: %s", prefix);
			return nullptr;
		}
	}

	if (!allocated) {
		LOG_FS("no slots left: %s", prefix);
		return nullptr;
	}

	allocated->prefix     = hash;
	allocated->pathOffset =
		(__builtin_strchr(prefix, VFS_PREFIX_SEPARATOR) - prefix) + 1;
	allocated->dev        = nullptr;
	allocated->provider   = nullptr;
	return allocated;
}

bool VFSProvider::deleteMountPoint(VFSMountPoint *mp) {
	if (!mp)
		return false;

	// Clear any aliases associated to the mount point.
	for (auto &alias : aliases) {
		if (alias.target == mp)
			util::clear(alias);
	}

	util::clear(*mp);
	return true;
}

bool VFSProvider::addAlias(
	const char    *prefix,
	VFSMountPoint *target,
	bool          force
) {
	auto     hash       = util::hash(prefix, VFS_PREFIX_SEPARATOR);
	VFSAlias *allocated = nullptr;

	for (auto &alias : aliases) {
		if (!alias.prefix) {
			allocated = &alias;
			break;
		}
		if (alias.prefix == hash) {
			if (alias.target == target)
				return true;

			if (force) {
				allocated = &alias;
				break;
			}

			LOG_FS("already present: %s", prefix);
			return false;
		}
	}

	if (!allocated) {
		LOG_FS("no slots left: %s", prefix);
		return false;
	}

	allocated->prefix     = hash;
	allocated->pathOffset =
		(__builtin_strchr(prefix, VFS_PREFIX_SEPARATOR) - prefix) + 1;
	allocated->target     = target;
	return true;
}

VFSMountPoint *VFSProvider::getMountPoint(const char *path) {
	auto hash = util::hash(path, VFS_PREFIX_SEPARATOR);

	for (auto &alias : aliases) {
		if (alias.prefix == hash)
			return alias.target;
	}
	for (auto &mp : mountPoints) {
		if (mp.prefix == hash)
			return &mp;
	}

	LOG_FS("unknown prefix: %s", path);
	return nullptr;
}

bool VFSProvider::getFileInfo(FileInfo &output, const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return false;
	if (!mp->provider)
		return false;

	return mp->provider->getFileInfo(output, &path[mp->pathOffset]);
}

bool VFSProvider::getFileFragments(
	FileFragmentTable &output,
	const char        *path
) {
	auto mp = getMountPoint(path);

	if (!mp)
		return false;
	if (!mp->provider)
		return false;

	return mp->provider->getFileFragments(output, &path[mp->pathOffset]);
}

Directory *VFSProvider::openDirectory(const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return nullptr;
	if (!mp->provider)
		return nullptr;

	return mp->provider->openDirectory(&path[mp->pathOffset]);
}

bool VFSProvider::createDirectory(const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return false;
	if (!mp->provider)
		return false;

	return mp->provider->createDirectory(&path[mp->pathOffset]);
}

File *VFSProvider::openFile(const char *path, uint32_t flags) {
	auto mp = getMountPoint(path);

	if (!mp)
		return nullptr;
	if (!mp->provider)
		return nullptr;

	return mp->provider->openFile(&path[mp->pathOffset], flags);
}

bool VFSProvider::deleteFile(const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return false;
	if (!mp->provider)
		return false;

	return mp->provider->deleteFile(&path[mp->pathOffset]);
}

size_t VFSProvider::loadData(util::Data &output, const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return 0;
	if (!mp->provider)
		return 0;

	return mp->provider->loadData(output, &path[mp->pathOffset]);
}

size_t VFSProvider::loadData(void *output, size_t length, const char *path) {
	auto mp = getMountPoint(path);

	if (!mp)
		return 0;
	if (!mp->provider)
		return 0;

	return mp->provider->loadData(output, length, &path[mp->pathOffset]);
}

size_t VFSProvider::saveData(
	const void *input,
	size_t     length,
	const char *path
) {
	auto mp = getMountPoint(path);

	if (!mp)
		return 0;
	if (!mp->provider)
		return 0;

	return mp->provider->saveData(input, length, &path[mp->pathOffset]);
}

}
