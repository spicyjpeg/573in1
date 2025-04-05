/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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
#include "common/blkdev/idebase.hpp"
#include "common/blkdev/memorycard.hpp"
#include "common/fs/fat.hpp"
#include "common/fs/host.hpp"
#include "common/fs/iso9660.hpp"
#include "common/fs/memorycard.hpp"
#include "common/fs/package.hpp"
#include "common/util/templates.hpp"
#include "main/app/fileio.hpp"

/* Storage and filesystem manager */

const char *const IDE_MOUNT_POINTS[]{ "ide0:", "ide1:" };
const char *const MC_MOUNT_POINTS[] { "mc0:",  "mc1:" };

FileIOManager::FileIOManager(void)
: _resourceFile(nullptr), resourcePtr(nullptr), resourceLength(0) {}

FileIOManager::~FileIOManager(void) {
	// The resource package's mount point must be destroyed first, followed by
	// the resource file currently in use (if any) then all other mount points.
	closeResourceFile();

	for (auto &mp : mountPoints)
		deleteMountPoint(&mp);
}

bool FileIOManager::deleteMountPoint(fs::VFSMountPoint *mp) {
	if (!mp)
		return false;

	if (mp->provider) {
		mp->provider->close();
		delete mp->provider;
	}

	if (mp->dev)
		delete mp->dev;

	return fs::VFSProvider::deleteMountPoint(mp);
}

void FileIOManager::init(void) {
	loadResourceFile(nullptr);

#ifdef ENABLE_PCDRV
	auto mp = newMountPoint("host:");

	if (mp) {
		auto provider = new fs::HostProvider();
		mp->provider  = provider;

#if 0
		provider->init();
#endif
	}
#endif
}

bool FileIOManager::loadResourceFile(const char *path) {
	closeResourceFile();

	auto mp = newMountPoint("res:");

	if (!mp)
		return false;

	auto provider = new fs::PackageProvider();
	mp->provider  = provider;

	if (path)
		_resourceFile = openFile(path, fs::READ);

	if (_resourceFile) {
		if (provider->init(_resourceFile))
			return true;

		_resourceFile->close();
		delete _resourceFile;
		_resourceFile = nullptr;
	}

	// Fall back to the default in-memory resource package in case of failure.
	provider->init(resourcePtr, resourceLength);
	return false;
}

void FileIOManager::closeResourceFile(void) {
	deleteMountPoint("res:");

	if (_resourceFile) {
		_resourceFile->close();
		delete _resourceFile;
		_resourceFile = nullptr;
	}
}

int FileIOManager::mountIDE(void) {
	unmountIDE();

	int mounted = 0;

	for (size_t i = 0; i < util::countOf(IDE_MOUNT_POINTS); i++) {
		auto &dev = blkdev::ideDevice(i);

		if (!dev.type)
			continue;

		auto       mp = newMountPoint(IDE_MOUNT_POINTS[i]);
		const char *alias;

		if (!mp)
			continue;

		mp->dev = &dev;

		// TODO: actually detect the filesystem, rather than assuming FAT or
		// ISO9660 depending on drive type
		if (dev.type == blkdev::ATAPI) {
			auto iso = new fs::ISO9660Provider();
			alias    = "cdrom:";

			if (iso->init(dev))
				mp->provider = iso;
			else
				delete iso;
		} else {
			auto fat = new fs::FATProvider();
			alias    = "hdd:";

			if (fat->init(dev, i))
				mp->provider = fat;
			else
				delete fat;
		}

		// Note that calling addAlias() multiple times will not update existing
		// aliases, so if two hard drives or CD-ROMs are present the hdd:/cdrom:
		// prefix will be assigned to the first one.
		addAlias(alias, mp);
		mounted++;
	}

	return mounted;
}

void FileIOManager::unmountIDE(void) {
	for (auto prefix : IDE_MOUNT_POINTS)
		deleteMountPoint(prefix);
}

int FileIOManager::mountMemoryCards(void) {
	unmountMemoryCards();

	int mounted = 0;

	for (size_t i = 0; i < util::countOf(MC_MOUNT_POINTS); i++) {
		auto &dev = blkdev::memoryCards[i];

		if (dev.enumerate())
			continue;

		auto mp = newMountPoint(MC_MOUNT_POINTS[i]);

		if (!mp)
			continue;

		mp->dev       = &dev;
		auto provider = new fs::MemoryCardProvider();

		if (!provider->init(dev))
			mp->provider = provider;
		else
			delete provider;

		addAlias("mc", mp);
		mounted++;
	}

	return mounted;
}

void FileIOManager::unmountMemoryCards(void) {
	for (auto prefix : MC_MOUNT_POINTS)
		deleteMountPoint(prefix);
}
