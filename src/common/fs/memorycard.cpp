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
#include "common/blkdev/device.hpp"
#include "common/fs/file.hpp"
#include "common/fs/memorycard.hpp"
#include "common/fs/memorycardbase.hpp"
#include "common/util/log.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"

namespace fs {

static constexpr int _MUTEX_TIMEOUT = 30000000;

/* Utilities */

static void _recordToFileInfo(
	FileInfo &output, const MemoryCardRecord &record
) {
	__builtin_strncpy(output.name, record.name, sizeof(output.name));
	output.size       = record.length;
	output.attributes = 0;
}

/* Memory card file and directory classes */

bool MemoryCardFile::_loadSector(uint32_t lba) {
	if (lba == _bufferedLBA)
		return true;
	if (!_provider->_io.readRelocated(_sectorBuffer, lba))
		return false;

	_bufferedLBA = lba;
	return true;
}

bool MemoryCardFile::_updateRecords(void) const {
	auto records     = _provider->_records.as<MemoryCardRecord>();
	auto lastCluster = (size_t(size) / MC_CLUSTER_LENGTH) - 1;

	const char *name;

	for (size_t i = 0; i <= lastCluster; i++) {
		auto &record = records[_clusters[i]];

		if (!i) {
			name = record.name;

			record.flags      = MC_RECORD_TYPE_FIRST | MC_RECORD_STATE_USED;
			record.chainIndex = _clusters[i + 1];
		} else {
			util::clear(record);
			__builtin_strncpy(record.name, name, sizeof(record.name));

			if (i == lastCluster) {
				record.flags      = MC_RECORD_TYPE_LAST   | MC_RECORD_STATE_USED;
				record.chainIndex = -1;
			} else {
				record.flags      = MC_RECORD_TYPE_MIDDLE | MC_RECORD_STATE_USED;
				record.chainIndex = _clusters[i + 1];
			}
		}

		record.length = size_t(size);
		record.updateChecksum();

		if (!_provider->_flushRecord(_clusters[i]))
			return false;
	}

	return true;
}

bool MemoryCardFile::_extend(size_t targetSize) {
	auto numClusters = size_t(size) / MC_CLUSTER_LENGTH;

	while (size_t(size) < targetSize) {
		// FIXME: prevent resizing of files not opened in write mode
		int cluster = _provider->_getFreeCluster();

		if (cluster < 0) {
			LOG_FS("no space left");
			return false;
		}

		_clusters[numClusters++] = cluster;
		size                    += MC_CLUSTER_LENGTH;
	}

	return _updateRecords();
}

size_t MemoryCardFile::read(void *output, size_t length) {
	auto ptr    = reinterpret_cast<uintptr_t>(output);
	auto offset = uint32_t(_offset);

#if 0
	_extend(size_t(size) + length);
#endif
	length = util::min<size_t>(length, size_t(size) - offset);

	for (auto remaining = length; remaining > 0;) {
		auto clusterOffset = offset / MC_CLUSTER_LENGTH;
		auto sectorOffset  =
			(offset / MC_SECTOR_LENGTH) % MC_SECTORS_PER_CLUSTER;
		auto ptrOffset     = offset % MC_SECTOR_LENGTH;

		auto   lba    = _clusters[clusterOffset] + sectorOffset;
		auto   buffer = reinterpret_cast<void *>(ptr);
		size_t readLength;

		if (!ptrOffset && (remaining >= MC_SECTOR_LENGTH)) {
			// If the read offset is on a sector boundary and at least one
			// sector's worth of data needs to be read, read a full sector
			// directly into the output buffer.
			readLength = MC_SECTOR_LENGTH;

			if (!_provider->_io.readRelocated(buffer, lba))
				return 0;
		} else {
			// In all other cases, read one sector at a time into the sector
			// buffer and copy the requested data over.
			readLength =
				util::min<size_t>(remaining, MC_SECTOR_LENGTH - ptrOffset);

			if (!_loadSector(lba))
				return 0;

			__builtin_memcpy(buffer, &_sectorBuffer[ptrOffset], readLength);
		}

		offset    += readLength;
		ptr       += readLength;
		remaining -= readLength;
	}

	_offset += length;
	return length;
}

size_t MemoryCardFile::write(const void *input, size_t length) {
	auto ptr    = reinterpret_cast<uintptr_t>(input);
	auto offset = uint32_t(_offset);

	_extend(size_t(size) + length);
	length = util::min<size_t>(length, size_t(size) - offset);

	for (auto remaining = length; remaining > 0;) {
		auto clusterOffset = offset / MC_CLUSTER_LENGTH;
		auto sectorOffset  =
			(offset / MC_SECTOR_LENGTH) % MC_SECTORS_PER_CLUSTER;
		auto ptrOffset     = offset % MC_SECTOR_LENGTH;

		auto   lba    = _clusters[clusterOffset] + sectorOffset;
		auto   buffer = reinterpret_cast<const void *>(ptr);
		size_t writeLength;

		if (!ptrOffset && (remaining >= MC_SECTOR_LENGTH)) {
			writeLength = MC_SECTOR_LENGTH;

			if (!_provider->_io.writeRelocated(buffer, lba))
				return 0;
		} else {
			// Use the sector buffer as a read-modify-write area for partial
			// sector writes.
			writeLength =
				util::min<size_t>(remaining, MC_SECTOR_LENGTH - ptrOffset);

			if (!_loadSector(lba))
				return 0;

			__builtin_memcpy(&_sectorBuffer[ptrOffset], buffer, writeLength);

			if (!_provider->_io.writeRelocated(_sectorBuffer, lba))
				return 0;
		}

		offset    += writeLength;
		ptr       += writeLength;
		remaining -= writeLength;
	}

	_offset += length;
	return length;
}

uint64_t MemoryCardFile::seek(uint64_t offset) {
	_extend(size_t(offset));
	_offset = util::min(offset, size);

	return _offset;
}

uint64_t MemoryCardFile::tell(void) const {
	return _offset;
}

bool MemoryCardDirectory::getEntry(FileInfo &output) {
	for (; _record < _recordEnd; _record++) {
		if (!_record->isFirstCluster())
			continue;
		if (!_record->validateChecksum())
			continue;

		_recordToFileInfo(output, *_record);
		return true;
	}

	return false;
}

/* Memory card filesystem provider */

int MemoryCardProvider::_getFirstCluster(const char *name) const {
	auto record = _records.as<const MemoryCardRecord>();

	for (size_t i = 0; i < MC_MAX_CLUSTERS; i++, record++) {
		if (!record->isFirstCluster())
			continue;
		if (__builtin_strncmp(record->name, name, sizeof(record->name)))
			continue;
		if (!record->validateChecksum())
			continue;

		return i;
	}

	return -1;
}

int MemoryCardProvider::_getFreeCluster(void) const {
	auto record = _records.as<const MemoryCardRecord>();

	for (size_t i = 0; i < MC_MAX_CLUSTERS; i++, record++) {
		if (record->isUsed())
			continue;
		if (!record->validateChecksum())
			continue;

		return i;
	}

	return -1;
}

bool MemoryCardProvider::_flushRecord(int cluster) {
	auto record = _records.as<MemoryCardRecord>() + cluster;

	if (_io.writeDirect(record, MC_LBA_RECORD_TABLE + cluster))
		return true;
	if (_io.readDirect(record, MC_LBA_RECORD_TABLE + cluster))
		LOG_FS("write failed, id=%d", cluster);
	else
		LOG_FS("write failed (not reloaded), id=%d", cluster);

	return false;
}

bool MemoryCardProvider::_truncate(const char *name, bool purgeFirst) {
	// Find and clear all records with the given name in order to truncate the
	// respective file to one record or delete it entirely.
	auto record  = _records.as<MemoryCardRecord>();
	bool deleted = false;

	for (size_t i = 0; i < MC_MAX_CLUSTERS; i++, record++) {
		if (__builtin_strncmp(record->name, name, sizeof(record->name)))
			continue;
#if 0
		if (!record.validateChecksum())
			continue;
#endif

		if (purgeFirst || !record->isFirstCluster()) {
			record->clear();
		} else {
			record->length     = MC_CLUSTER_LENGTH;
			record->chainIndex = -1;
			record->updateChecksum();
		}

		if (!_flushRecord(i))
			return false;

		deleted = true;
	}

	return deleted;
}

bool MemoryCardProvider::init(blkdev::Device &dev) {
	if (type)
		return false;
	if (dev.sectorLength != MC_SECTOR_LENGTH)
		return false;
	if (!_records.allocate<MemoryCardRecord>(MC_MAX_CLUSTERS))
		return false;

	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return 0;
	}

	MemoryCardHeader header;

	if (dev.read(&header, MC_LBA_HEADER, 1))
		return false;
	if (!header.validateMagic() || !header.validateChecksum()) {
		LOG_FS("invalid memory card header");
		return false;
	}

	if (!_io.init(dev))
		return false;
	if (dev.read(_records.ptr, MC_LBA_RECORD_TABLE, MC_MAX_CLUSTERS))
		return false;

	type           = MEMORY_CARD;
	capacity       = MC_CLUSTER_LENGTH * MC_MAX_CLUSTERS;
	volumeLabel[0] = 0;

	// The no$psx BIOS supports assigning custom labels to memory cards and
	// stores them as part of its own configuration sector.
	MemoryCardNocashConfig config;

	if (!dev.read(&config, MC_LBA_NOCASH_CONFIG, 1)) {
		if (config.validateMagic() && config.validateChecksum())
			__builtin_strncpy(
				volumeLabel, config.cardLabel, sizeof(volumeLabel)
			);
	}

	LOG_FS("mounted card: %s", volumeLabel);
	return true;
}

void MemoryCardProvider::close(void) {
	_records.destroy();

	type     = NONE;
	capacity = 0;
}

uint64_t MemoryCardProvider::getFreeSpace(void) {
	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return 0;
	}

	auto     record    = _records.as<const MemoryCardRecord>();
	uint32_t freeSpace = 0;

	for (size_t i = MC_MAX_CLUSTERS; i; i--, record++) {
		if (record->isUsed())
			continue;
		if (!record->validateChecksum())
			continue;

		freeSpace += MC_CLUSTER_LENGTH;
	}

	return freeSpace;
}

bool MemoryCardProvider::getFileInfo(FileInfo &output, const char *path) {
	// Any leading path separators must be stripped manually.
	while ((*path == '/') || (*path == '\\'))
		path++;

	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return false;
	}

	auto records = _records.as<const MemoryCardRecord>();
	int  cluster = _getFirstCluster(path);

	if (cluster < 0)
		return false;

	_recordToFileInfo(output, records[cluster]);
	return true;
}

bool MemoryCardProvider::getFileFragments(
	FileFragmentTable &output, const char *path
) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return false;
	}

	auto records = _records.as<const MemoryCardRecord>();
	int  cluster = _getFirstCluster(path);

	if (cluster < 0)
		return false;
	if (!output.allocate<FileFragment>(
		records[cluster].length / MC_CLUSTER_LENGTH
	))
		return false;

	auto fragment = output.as<FileFragment>();

	for (; cluster >= 0; fragment++) {
		fragment->lba    = cluster;
		fragment->length = MC_SECTORS_PER_CLUSTER;
		cluster          = records[cluster].chainIndex;
	}

	return true;
}

Directory *MemoryCardProvider::openDirectory(const char *path) {
	// There are no "directories" other than the card's root.
	while ((*path == '/') || (*path == '\\'))
		path++;
	if (*path)
		return nullptr;

	// FIXME: a copy of all records shall be created here, in order to allow for
	// the directory to be read while also modifying records
	auto records = _records.as<const MemoryCardRecord>();
	auto dir     = new MemoryCardDirectory();

	dir->_record    = records;
	dir->_recordEnd = records + MC_MAX_CLUSTERS;
	return dir;
}

File *MemoryCardProvider::openFile(const char *path, uint32_t flags) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return nullptr;
	}

	auto records = _records.as<MemoryCardRecord>();
	int  cluster = _getFirstCluster(path);

	if (cluster >= 0) {
		// If the file exists, truncate it if necessary.
		if (flags & FORCE_CREATE) {
			if (!_truncate(path))
				return nullptr;
		}
	} else if (flags & (FORCE_CREATE | ALLOW_CREATE)) {
		// If the file was not found but we are allowed to create it, proceed to
		// use the previously found free cluster.
		cluster = _getFreeCluster();

		if (cluster < 0) {
			LOG_FS("no space left: %s", path);
			return nullptr;
		}

		auto &record = records[cluster];

		util::clear(record);
		__builtin_strncpy(record.name, path, sizeof(record.name));

		record.flags      = MC_RECORD_TYPE_FIRST | MC_RECORD_STATE_USED;
		record.length     = MC_CLUSTER_LENGTH;
		record.chainIndex = -1;
		record.updateChecksum();

		if (!_flushRecord(cluster))
			return nullptr;
	} else {
		LOG_FS("not found: %s", path);
		return nullptr;
	}

	auto file     = new MemoryCardFile();
	auto fragment = file->_clusters;

	file->_provider    = this;
	file->_offset      = 0;
	file->_bufferedLBA = 0;
	file->size         = records[cluster].length;

	while (cluster >= 0) {
		*(fragment++) = cluster;
		cluster       = records[cluster].chainIndex;
	}

	return file;
}

bool MemoryCardProvider::deleteFile(const char *path) {
	while ((*path == '/') || (*path == '\\'))
		path++;

	util::MutexLock lock(_mutex, uint32_t(1), _MUTEX_TIMEOUT);

	if (!lock.locked) {
		LOG_FS("record mutex timeout");
		return false;
	}

	return _truncate(path, true);
}

}
