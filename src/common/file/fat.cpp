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
#include "common/file/fat.hpp"
#include "common/file/file.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/ide.hpp"
#include "common/io.hpp"
#include "ps1/system.h"
#include "vendor/diskio.h"
#include "vendor/ff.h"

namespace file {

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

bool FATProvider::init(int drive) {
	if (type)
		return false;

	_drive[0] = drive + '0';

	auto error = f_mount(&_fs, _drive, 1);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], _drive);
		return false;
	}

	type     = FileSystemType(_fs.fs_type);
	capacity = uint64_t(_fs.n_fatent - 2) * _fs.csize * _fs.ssize;

	f_getlabel(_drive, volumeLabel, &serialNumber);

	LOG_FS("mounted FAT: %s", _drive);
	return true;
}

void FATProvider::close(void) {
	if (!type)
		return;

	auto error = f_unmount(_drive);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], _drive);
		return;
	}

	type     = NONE;
	capacity = 0;

	LOG_FS("unmounted FAT: %s", _drive);
}

uint64_t FATProvider::getFreeSpace(void) {
	if (!_fs.fs_type)
		return 0;

	uint32_t count;
	FATFS    *dummy;
	auto     error = f_getfree(_drive, &count, &dummy);

	if (error) {
		LOG_FS("%s: %s", _FATFS_ERROR_NAMES[error], _drive);
		return 0;
	}

	size_t clusterSize = size_t(_fs.csize) * size_t(_fs.ssize);

	return uint64_t(count) * uint64_t(clusterSize);
}

bool FATProvider::_selectDrive(void) {
	if (!_fs.fs_type)
		return false;

	return !f_chdrive(_drive);
}

bool FATProvider::getFileInfo(FileInfo &output, const char *path) {
	if (!_selectDrive())
		return false;

	FILINFO info;
	auto    error = f_stat(path, &info);

	if (error) {
		//LOG_FS("%s: %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
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
	if (!_selectDrive())
		return false;

	FIL  fd;
	auto error = f_open(&fd, path, READ);

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

	LOG_FS("%s, %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
	return false;
}

Directory *FATProvider::openDirectory(const char *path) {
	if (!_selectDrive())
		return nullptr;

	auto dir   = new FATDirectory();
	auto error = f_opendir(&(dir->_fd), path);

	if (error) {
		LOG_FS("%s: %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
		delete dir;
		return nullptr;
	}

	return dir;
}

bool FATProvider::createDirectory(const char *path) {
	if (!_selectDrive())
		return false;

	auto error = f_mkdir(path);

	if (error) {
		LOG_FS("%s: %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
		return false;
	}

	return true;
}

File *FATProvider::openFile(const char *path, uint32_t flags) {
	if (!_selectDrive())
		return nullptr;

	auto _file = new FATFile();
	auto error = f_open(&(_file->_fd), path, uint8_t(flags));

	if (error) {
		LOG_FS("%s: %s%s", _FATFS_ERROR_NAMES[error], _drive, path);
		delete _file;
		return nullptr;
	}

	_file->size = f_size(&(_file->_fd));
	return _file;
}

/* FatFs library API glue */

static constexpr int _MUTEX_TIMEOUT = 30000000;

static uint32_t _fatMutex = 0;

extern "C" DSTATUS disk_initialize(uint8_t drive) {
#if 0
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY)) {
		if (dev.enumerate())
			return RES_NOTRDY;
	}
#endif

	return disk_status(drive);
}

extern "C" DSTATUS disk_status(uint8_t drive) {
	auto     &dev  = ide::devices[drive];
	uint32_t flags = 0;

	if (!(dev.flags & ide::DEVICE_READY))
		flags |= STA_NOINIT;
	if (!dev.capacity)
		flags |= STA_NODISK;
	if (dev.flags & ide::DEVICE_READ_ONLY)
		flags |= STA_PROTECT;

	return flags;
}

extern "C" DRESULT disk_read(
	uint8_t drive, uint8_t *data, LBA_t lba, size_t count
) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;
	if (dev.readData(data, lba, count))
		return RES_ERROR;

	return RES_OK;
}

extern "C" DRESULT disk_write(
	uint8_t drive, const uint8_t *data, LBA_t lba, size_t count
) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;
	if (dev.flags & ide::DEVICE_READ_ONLY)
		return RES_WRPRT;
	if (dev.writeData(data, lba, count))
		return RES_ERROR;

	return RES_OK;
}

extern "C" DRESULT disk_ioctl(uint8_t drive, uint8_t cmd, void *data) {
	auto &dev = ide::devices[drive];

	if (!(dev.flags & ide::DEVICE_READY))
		return RES_NOTRDY;

	switch (cmd) {
#ifdef ENABLE_FULL_IDE_DRIVER
		case CTRL_SYNC:
			return dev.flushCache() ? RES_ERROR : RES_OK;
#endif

		case GET_SECTOR_COUNT:
			__builtin_memcpy(data, &dev.capacity, sizeof(LBA_t));
			return RES_OK;

		case GET_SECTOR_SIZE:
		//case GET_BLOCK_SIZE:
			*reinterpret_cast<uint16_t *>(data) = dev.getSectorSize();
			return RES_OK;

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
	uint32_t mask = 1 << id;

	for (int timeout = _MUTEX_TIMEOUT; timeout > 0; timeout -= 10) {
		{
			util::ThreadCriticalSection sec;
			__atomic_signal_fence(__ATOMIC_ACQUIRE);

			auto locked = _fatMutex & mask;

			if (!locked) {
				_fatMutex |= mask;
				flushWriteQueue();
				return true;
			}
		}

		delayMicroseconds(10);
	}

	return false;
}

extern "C" void ff_mutex_give(int id) {
	util::CriticalSection sec;

	_fatMutex &= ~(1 << id);
	flushWriteQueue();
}

}
