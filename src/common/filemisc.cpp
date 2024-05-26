
#include <stddef.h>
#include <stdint.h>
#include "common/file.hpp"
#include "common/filemisc.hpp"
#include "common/util.hpp"
#include "ps1/pcdrv.h"

namespace file {

/* PCDRV file class */

size_t HostFile::read(void *output, size_t length) {
	int actualLength = pcdrvRead(_fd, output, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}

size_t HostFile::write(const void *input, size_t length) {
	int actualLength = pcdrvWrite(_fd, input, length);

	if (actualLength < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualLength, this);
		return 0;
	}

	return size_t(actualLength);
}

uint64_t HostFile::seek(uint64_t offset) {
	int actualOffset = pcdrvSeek(_fd, int(offset), PCDRV_SEEK_SET);

	if (actualOffset < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualOffset, this);
		return 0;
	}

	return uint64_t(actualOffset);
}

uint64_t HostFile::tell(void) const {
	int actualOffset = pcdrvSeek(_fd, 0, PCDRV_SEEK_CUR);

	if (actualOffset < 0) {
		LOG("PCDRV error, code=%d, file=0x%08x", actualOffset, this);
		return 0;
	}

	return uint64_t(actualOffset);
}

void HostFile::close(void) {
	pcdrvClose(_fd);
}

/* PCDRV filesystem provider */

bool HostProvider::init(void) {
	int error = pcdrvInit();

	if (error < 0) {
		LOG("PCDRV error, code=%d", error);
		return false;
	}

	type = HOST;
	return true;
}

bool HostProvider::createDirectory(const char *path) {
	int fd = pcdrvCreate(path, DIRECTORY);

	if (fd < 0) {
		LOG("PCDRV error, code=%d", fd);
		return false;
	}

	pcdrvClose(fd);
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
		LOG("PCDRV error, code=%d", fd);
		return nullptr;
	}

	auto file = new HostFile();

	file->_fd  = fd;
	file->size = pcdrvSeek(fd, 0, PCDRV_SEEK_END);
	pcdrvSeek(fd, 0, PCDRV_SEEK_SET);

	return file;
}

/* Virtual filesystem driver */

VFSMountPoint *VFSProvider::_getMounted(const char *path) {
	auto hash = util::hash(path, VFS_PREFIX_SEPARATOR);

	for (auto &mp : _mountPoints) {
		if (mp.prefix == hash)
			return &mp;
	}

	return nullptr;
}

bool VFSProvider::mount(const char *prefix, Provider *provider) {
	for (auto &mp : _mountPoints) {
		if (mp.provider)
			continue;

		mp.prefix     = util::hash(prefix, VFS_PREFIX_SEPARATOR);
		mp.pathOffset = 0;
		mp.provider   = provider;

		while (prefix[mp.pathOffset] != VFS_PREFIX_SEPARATOR)
			mp.pathOffset++;

		mp.pathOffset++;
		return true;
	}

	return false;
}

bool VFSProvider::unmount(const char *prefix) {
	auto hash = util::hash(prefix, VFS_PREFIX_SEPARATOR);

	for (auto &mp : _mountPoints) {
		if (mp.prefix != hash)
			continue;

		mp.prefix     = 0;
		mp.pathOffset = 0;
		mp.provider   = nullptr;
		return true;
	}

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
