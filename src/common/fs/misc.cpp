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
#include "common/fs/misc.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/pcdrv.h"

namespace fs {

/* PCDRV utilities */

static void _dirEntryToFileInfo(FileInfo &output, const PCDRVDirEntry &entry) {
	__builtin_strncpy(output.name, entry.name, sizeof(output.name));
	output.size       = entry.size;
	output.attributes = entry.attributes;
}

/* PCDRV file and directory classes */

size_t HostFile::read(void *output, size_t length) {
	int actualLength = pcdrvRead(_fd, output, length);

	if (actualLength < 0) {
		LOG_FS("PCDRV error %d, fd=%d", actualLength, _fd);
		return 0;
	}

	return size_t(actualLength);
}

size_t HostFile::write(const void *input, size_t length) {
	int actualLength = pcdrvWrite(_fd, input, length);

	if (actualLength < 0) {
		LOG_FS("PCDRV error %d, fd=%d", actualLength, _fd);
		return 0;
	}

	return size_t(actualLength);
}

uint64_t HostFile::seek(uint64_t offset) {
	int actualOffset = pcdrvSeek(_fd, int(offset), PCDRV_SEEK_SET);

	if (actualOffset < 0) {
		LOG_FS("PCDRV error %d, fd=%d", actualOffset, _fd);
		return 0;
	}

	return uint64_t(actualOffset);
}

uint64_t HostFile::tell(void) const {
	int actualOffset = pcdrvSeek(_fd, 0, PCDRV_SEEK_CUR);

	if (actualOffset < 0) {
		LOG_FS("PCDRV error %d, fd=%d", actualOffset, _fd);
		return 0;
	}

	return uint64_t(actualOffset);
}

void HostFile::close(void) {
	pcdrvClose(_fd);
}

bool HostDirectory::getEntry(FileInfo &output) {
	if (_fd < 0)
		return false;

	// Return the last entry fetched while also fetching the next one (if any).
	_dirEntryToFileInfo(output, _entry);
	if (pcdrvFindNext(_fd, &_entry) < 0)
		_fd = -1;

	return true;
}

/* PCDRV filesystem provider */

bool HostProvider::init(void) {
	int error = pcdrvInit();

	if (error < 0) {
		LOG_FS("PCDRV error %d", error);
		return false;
	}

	type = HOST;
	__builtin_strncpy(volumeLabel, "PCDRV", sizeof(volumeLabel));

	return true;
}

bool HostProvider::getFileInfo(FileInfo &output, const char *path) {
	PCDRVDirEntry entry;

	int fd = pcdrvFindFirst(path, &entry);

	if (fd < 0) {
		LOG_FS("PCDRV error %d: %s", fd, path);
		return false;
	}

	_dirEntryToFileInfo(output, entry);
	return true;
}

Directory *HostProvider::openDirectory(const char *path) {
	char pattern[MAX_PATH_LENGTH];
	char *ptr = pattern;

	while (*path)
		*(ptr++) = *(path++);

	*(ptr++) = '/';
	*(ptr++) = '*';
	*(ptr++) = 0;

	auto dir = new HostDirectory();
	int  fd  = pcdrvFindFirst(pattern, &(dir->_entry));

	if (fd < 0) {
		LOG_FS("PCDRV error %d: %s", fd, path);
		delete dir;
		return nullptr;
	}

	return dir;
}

bool HostProvider::createDirectory(const char *path) {
	int error = pcdrvCreateDir(path);

	if (error < 0) {
		LOG_FS("PCDRV error %d: %s", error, path);
		return false;
	}

	return true;
}

File *HostProvider::openFile(const char *path, uint32_t flags) {
	PCDRVOpenMode mode = PCDRV_MODE_READ;

	if ((flags & (READ | WRITE)) == (READ | WRITE))
		mode = PCDRV_MODE_READ_WRITE;
	else if (flags & WRITE)
		mode = PCDRV_MODE_WRITE;

	int fd = pcdrvOpen(path, mode);

	if (fd < 0) {
		LOG_FS("PCDRV error %d: %s", fd, path);
		return nullptr;
	}

	auto file  = new HostFile();
	file->_fd  = fd;
	file->size = pcdrvSeek(fd, 0, PCDRV_SEEK_END);

	pcdrvSeek(fd, 0, PCDRV_SEEK_SET);
	return file;
}

bool HostProvider::deleteFile(const char *path) {
	int error = pcdrvUnlink(path);

	if (error < 0) {
		LOG_FS("PCDRV error %d: %s", error, path);
		return false;
	}

	return true;
}

/* Virtual filesystem driver */

VFSMountPoint *VFSProvider::_getMounted(const char *path) {
	auto hash = util::hash(path, VFS_PREFIX_SEPARATOR);

	for (auto &mp : _mountPoints) {
		if (mp.prefix == hash)
			return &mp;
	}

	LOG_FS("unknown device: %s", path);
	return nullptr;
}

bool VFSProvider::mount(const char *prefix, Provider *provider, bool force) {
	auto hash = util::hash(prefix, VFS_PREFIX_SEPARATOR);

	VFSMountPoint *freeMP = nullptr;

	for (auto &mp : _mountPoints) {
		if (!mp.prefix) {
			freeMP = &mp;
		} else if (mp.prefix == hash) {
			if (force) {
				freeMP = &mp;
				break;
			}

			LOG_FS("%s was already mapped", prefix);
			return false;
		}
	}

	if (!freeMP) {
		LOG_FS("no mount points left for %s", prefix);
		return false;
	}

	freeMP->prefix     = hash;
	freeMP->pathOffset =
		(__builtin_strchr(prefix, VFS_PREFIX_SEPARATOR) - prefix) + 1;
	freeMP->provider   = provider;

	LOG_FS("mapped %s", prefix);
	return true;
}

bool VFSProvider::unmount(const char *prefix) {
	auto hash = util::hash(prefix, VFS_PREFIX_SEPARATOR);

	for (auto &mp : _mountPoints) {
		if (mp.prefix != hash)
			continue;

		mp.prefix     = 0;
		mp.pathOffset = 0;
		mp.provider   = nullptr;

		LOG_FS("unmapped %s", prefix);
		return true;
	}

	LOG_FS("%s was not mapped", prefix);
	return false;
}

bool VFSProvider::unmount(Provider *provider) {
	for (auto &mp : _mountPoints) {
		if (mp.provider != provider)
			continue;

		mp.prefix     = 0;
		mp.pathOffset = 0;
		mp.provider   = nullptr;

		return true;
	}

	LOG_FS("FS was not mapped");
	return false;
}

bool VFSProvider::getFileInfo(FileInfo &output, const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return false;

	return mp->provider->getFileInfo(output, &path[mp->pathOffset]);
}

bool VFSProvider::getFileFragments(
	FileFragmentTable &output, const char *path
) {
	auto mp = _getMounted(path);

	if (!mp)
		return false;

	return mp->provider->getFileFragments(output, &path[mp->pathOffset]);
}

Directory *VFSProvider::openDirectory(const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return nullptr;

	return mp->provider->openDirectory(&path[mp->pathOffset]);
}

bool VFSProvider::createDirectory(const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return false;

	return mp->provider->createDirectory(&path[mp->pathOffset]);
}

File *VFSProvider::openFile(const char *path, uint32_t flags) {
	auto mp = _getMounted(path);

	if (!mp)
		return nullptr;

	return mp->provider->openFile(&path[mp->pathOffset], flags);
}

bool VFSProvider::deleteFile(const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return false;

	return mp->provider->deleteFile(&path[mp->pathOffset]);
}

size_t VFSProvider::loadData(util::Data &output, const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return 0;

	return mp->provider->loadData(output, &path[mp->pathOffset]);
}

size_t VFSProvider::loadData(void *output, size_t length, const char *path) {
	auto mp = _getMounted(path);

	if (!mp)
		return 0;

	return mp->provider->loadData(output, length, &path[mp->pathOffset]);
}

size_t VFSProvider::saveData(
	const void *input, size_t length, const char *path
) {
	auto mp = _getMounted(path);

	if (!mp)
		return 0;

	return mp->provider->saveData(input, length, &path[mp->pathOffset]);
}

}
