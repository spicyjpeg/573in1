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
#include "common/fs/file.hpp"
#include "common/fs/memorycardbase.hpp"
#include "common/storage/device.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* Memory card file header structures */

using ShiftJISChar = uint16_t;

enum MemoryCardIconFlags : uint8_t {
	MC_ICON_FRAMES_BITMASK = 15 << 0,
	MC_ICON_VALID          =  1 << 4
};

class [[gnu::packed]] PocketStationInfo {
public:
	uint16_t numFileIcons;       // 0x50-0x51
	uint32_t magic;              // 0x52-0x55
	uint8_t  numAppIcons;        // 0x56
	uint8_t  numCommandHandlers; // 0x57
	uint32_t _reserved;          // 0x58-0x5b
	uint32_t entryPoint;         // 0x5c-0x5f

	inline bool validateMagic(void) const {
		return (magic == "MCX0"_c) || (magic == "MCX1"_c);
	}
};

class [[gnu::packed]] MemoryCardFileHeader {
public:
	uint16_t          magic;             // 0x00-0x01
	uint8_t           iconFlags;         // 0x02
	uint8_t           headerBlockOffset; // 0x03
	ShiftJISChar      displayName[32];   // 0x04-0x43
	uint8_t           _reserved[12];     // 0x44-0x4f
	PocketStationInfo pocketStation;     // 0x50-0x5f
	uint16_t          iconCLUT[16];      // 0x60-0x7f

	inline bool validateMagic(void) const {
		return (magic == "SC"_c);
	}
};

/* Memory card file and directory classes */

class MemoryCardProvider;

class MemoryCardFile : public File {
	friend class MemoryCardProvider;

private:
	MemoryCardProvider *_provider;
	uint8_t             _clusters[MC_MAX_CLUSTERS];

	uint32_t _offset, _bufferedLBA;
	uint8_t  _sectorBuffer[MC_SECTOR_LENGTH];

	bool _loadSector(uint32_t lba);
	bool _updateRecords(void) const;
	bool _extend(size_t targetSize);

public:
	size_t read(void *output, size_t length);
	size_t write(const void *input, size_t length);
	uint64_t seek(uint64_t offset);
	uint64_t tell(void) const;
};

class MemoryCardDirectory : public Directory {
	friend class MemoryCardProvider;

private:
	const MemoryCardRecord *_record, *_recordEnd;

public:
	bool getEntry(FileInfo &output);
};

/* Memory card filesystem provider */

class MemoryCardProvider : public Provider {
	friend class MemoryCardFile;

private:
	MemoryCardIOHandler        _io;
	util::MutexFlags<uint32_t> _mutex;

	util::Data _records;

	int _getFirstCluster(const char *name) const;
	int _getFreeCluster(void) const;

	bool _flushRecord(int cluster);
	bool _truncate(const char *name, bool purgeFirst = false);

public:
	bool init(storage::Device &dev);
	void close(void);
	uint64_t getFreeSpace(void);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
	bool deleteFile(const char *path);
};

}
