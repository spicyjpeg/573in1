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
#include "common/fs/fat.hpp"
#include "common/fs/file.hpp"
#include "common/storage/device.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/io.hpp"
#include "vendor/diskio.h"
#include "vendor/ff.h"

namespace fs {

static const char *const _FATFS_ERROR_NAMES[]{
	"OK",
	"DISK_ERR",
	"INT_ERR",
	"NOT_READY",
	"NO_FILE",
	"NO_PATH",
	"INVALID_NAME",
	"DENIED",
	"EXIST",
	"INVALID_OBJECT",
	"WRITE_PROTECTED",
	"INVALID_DRIVE",
	"NOT_ENABLED",
	"NO_FILESYSTEM",
	"MKFS_ABORTED",
	"TIMEOUT",
	"LOCKED",
	"NOT_ENOUGH_CORE",
	"TOO_MANY_OPEN_FILES",
	"INVALID_PARAMETER"
};

/* FAT file and directory classes */

size_t FATFile::read(void *output, size_t length) {
	size_t actualLength;
	auto   error = f_read(&_fd, output, length, &actualLength);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return 0;
	}

	return uint64_t(actualLength);
}

size_t FATFile::write(const void *input, size_t length) {
	size_t actualLength;
	auto   error = f_write(&_fd, input, length, &actualLength);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return 0;
	}

	return uint64_t(actualLength);
}

uint64_t FATFile::seek(uint64_t offset) {
	auto error = f_lseek(&_fd, offset);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return 0;
	}

	return f_tell(&_fd);
}

uint64_t FATFile::tell(void) const {
	return f_tell(&_fd);
}

void FATFile::close(void) {
	f_close(&_fd);
}

bool FATDirectory::getEntry(FileInfo &output) {
	FILINFO info;
	auto    error = f_readdir(&_fd, &info);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return false;
	}
	if (!info.fname[0])
		return false;

	__builtin_strncpy(output.name, info.fname, sizeof(output.name));
	output.size       = info.fsize;
	output.attributes = info.fattrib;

	return true;
}

void FATDirectory::close(void) {
	f_closedir(&_fd);
}

/* FAT filesystem provider */

bool FATProvider::init(storage::Device &dev, int mutexID) {
	if (type)
		return false;

	auto error = f_mount(&_fs, &dev, mutexID, 1);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], dev.model);
		return false;
	}

	type     = FileSystemType(_fs.fs_type);
	capacity = uint64_t(_fs.n_fatent - 2) * _fs.csize * _fs.ssize;

	f_getlabel(&_fs, volumeLabel, &serialNumber);

	LOG_FS("mounted FAT: %s", volumeLabel);
	return true;
}

void FATProvider::close(void) {
	if (!type)
		return;

	auto error = f_unmount(&_fs);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return;
	}

	type     = NONE;
	capacity = 0;

	LOG_FS("unmounted FAT: %s", volumeLabel);
}

uint64_t FATProvider::getFreeSpace(void) {
	if (!_fs.fs_type)
		return 0;

	uint32_t count;
	auto     error = f_getfree(&_fs, &count);

	if (error) {
		LOG_FS("%s", _FATFS_ERROR_NAMES[error]);
		return 0;
	}

	size_t clusterSize = size_t(_fs.csize) * size_t(_fs.ssize);

	return uint64_t(count) * uint64_t(clusterSize);
}

bool FATProvider::getFileInfo(FileInfo &output, const char *path) {
	FILINFO info;
	auto    error = f_stat(&_fs, path, &info);

	if (error) {
#if 0
		LOG_FS("%s: %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
#endif
		return false;
	}

	__builtin_strncpy(output.name, info.fname, sizeof(output.name));
	output.size       = info.fsize;
	output.attributes = info.fattrib;

	return true;
}

bool FATProvider::getFileFragments(
	FileFragmentTable &output, const char *path
) {
	FIL  fd;
	auto error = f_open(&_fs, &fd, path, READ);

	if (!error) {
		size_t length;

		// Note that this function is not normally part of FatFs.
		error = f_getlbas(&fd, nullptr, 0, &length);

		if (!error) {
			bool allocated = output.allocate<uint64_t>(length);

			if (allocated)
				f_getlbas(&fd, output.as<uint64_t>(), 0, &length);

			f_close(&fd);
			return allocated;
		}

		f_close(&fd);
	}

	LOG_FS("%s, %s", _FATFS_ERROR_NAMES[error], path);
	return false;
}

Directory *FATProvider::openDirectory(const char *path) {
	auto dir   = new FATDirectory();
	auto error = f_opendir(&_fs, &(dir->_fd), path);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], path);
		delete dir;
		return nullptr;
	}

	return dir;
}

bool FATProvider::createDirectory(const char *path) {
	auto error = f_mkdir(&_fs, path);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], path);
		return false;
	}

	return true;
}

File *FATProvider::openFile(const char *path, uint32_t flags) {
	auto file  = new FATFile();
	auto error = f_open(&_fs, &(file->_fd), path, uint8_t(flags));

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], path);
		delete file;
		return nullptr;
	}

	file->size = f_size(&(file->_fd));
	return file;
}

/* FatFs library API glue */

static constexpr int _MUTEX_TIMEOUT = 30000000;

static util::MutexFlags<uint32_t> _fatMutex;

extern "C" DSTATUS disk_status(PDRV_t drive) {
	auto     dev   = reinterpret_cast<storage::Device *>(drive);
	uint32_t flags = 0;

	if (!(dev->type))
		flags |= STA_NOINIT;
	if (!dev->capacity)
		flags |= STA_NODISK;
	if (dev->flags & storage::READ_ONLY)
		flags |= STA_PROTECT;

	return flags;
}

extern "C" DRESULT disk_read(
	PDRV_t drive, uint8_t *data, LBA_t lba, size_t count
) {
	auto dev = reinterpret_cast<storage::Device *>(drive);

	return dev->read(data, lba, count) ? RES_ERROR : RES_OK;
}

extern "C" DRESULT disk_write(
	PDRV_t drive, const uint8_t *data, LBA_t lba, size_t count
) {
	auto dev = reinterpret_cast<storage::Device *>(drive);

	if (dev->flags & storage::READ_ONLY)
		return RES_WRPRT;

	return dev->write(data, lba, count) ? RES_ERROR : RES_OK;
}

extern "C" DRESULT disk_ioctl(PDRV_t drive, uint8_t cmd, void *data) {
	auto dev  = reinterpret_cast<storage::Device *>(drive);
	auto lbas = reinterpret_cast<LBA_t *>(data);

	if (!(dev->type))
		return RES_NOTRDY;

	switch (cmd) {
		case CTRL_SYNC:
			return dev->flushCache() ? RES_ERROR : RES_OK;

		case GET_SECTOR_COUNT:
			*lbas = dev->capacity;
			return RES_OK;

		case GET_SECTOR_SIZE:
			*reinterpret_cast<uint16_t *>(data) = dev->sectorLength;
			return RES_OK;

		case GET_BLOCK_SIZE:
			*reinterpret_cast<uint32_t *>(data) = dev->sectorLength;
			return RES_OK;

		case CTRL_TRIM:
			return dev->trim(lbas[0], lbas[1] - lbas[0]) ? RES_ERROR : RES_OK;

		default:
			return RES_PARERR;
	}
}

extern "C" uint32_t get_fattime(void) {
	util::Date date;

	io::getRTCTime(date);
	return date.toDOSTime();
}

extern "C" int ff_mutex_create(int id) {
	return true;
}

extern "C" void ff_mutex_delete(int id) {}

extern "C" int ff_mutex_take(int id) {
	bool locked = _fatMutex.lock(1 << id, _MUTEX_TIMEOUT);

	if (!locked)
		LOG_FS("mutex %d timeout", id);

	return locked;
}

extern "C" void ff_mutex_give(int id) {
	_fatMutex.unlock(1 << id);
}

}
