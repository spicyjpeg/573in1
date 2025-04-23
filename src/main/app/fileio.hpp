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

#pragma once

#include <stddef.h>
#include "common/fs/file.hpp"
#include "common/fs/vfs.hpp"

/* Storage and filesystem manager */

extern const char *const IDE_MOUNT_POINTS[];
extern const char *const MC_MOUNT_POINTS[];

class FileIOManager : public fs::VFSProvider {
private:
	fs::File *_resourceFile;

public:
	const void *resourcePtr;
	size_t     resourceLength;

	inline bool deleteMountPoint(const char *path) {
		return deleteMountPoint(getMountPoint(path));
	}

	FileIOManager(void);
	~FileIOManager(void);
	bool deleteMountPoint(fs::VFSMountPoint *mp);

	void init(void);
	bool loadResourceFile(const char *path);
	void closeResourceFile(void);

	int mountIDE(void);
	bool mountPS1CDROM(void);
	int mountMemoryCards(void);
	void unmountAll(void);
};
