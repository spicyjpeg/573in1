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
#include "common/storage/device.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* ISO9660 data types */

template<typename T> struct [[gnu::packed]] ISOInt {
	T le, be;
};

struct [[gnu::packed]] ISODate {
	uint8_t year, month, day, hour, minute, second, timezone;
};

using ISOUint16 = ISOInt<uint16_t>;
using ISOUint32 = ISOInt<uint32_t>;
using ISOCharA  = uint8_t;
using ISOCharD  = uint8_t;

/* ISO9660 data structures (see https://wiki.osdev.org/ISO_9660) */

static constexpr size_t ISO9660_MAX_RECORD_DATA_LENGTH = 512;

enum ISOSUSPEntryType : uint16_t {
	ISO_SUSP_ATTRIBUTES     = util::concat2('P', 'X'),
	ISO_SUSP_DEVICE_NUMBER  = util::concat2('P', 'N'),
	ISO_SUSP_SYMBOLIC_LINK  = util::concat2('S', 'L'),
	ISO_SUSP_ALTERNATE_NAME = util::concat2('N', 'M'),
	ISO_SUSP_CHILD_LINK     = util::concat2('C', 'L'),
	ISO_SUSP_PARENT_LINK    = util::concat2('P', 'L'),
	ISO_SUSP_RELOCATED_DIR  = util::concat2('R', 'E'),
	ISO_SUSP_TIMESTAMP      = util::concat2('T', 'F'),
	ISO_SUSP_SPARSE_FILE    = util::concat2('S', 'F')
};

enum ISOSUSNameEntryFlag : uint8_t {
	ISO_SUSP_NAME_CONTINUE = 1 << 0,
	ISO_SUSP_NAME_CURRENT  = 1 << 1,
	ISO_SUSP_NAME_PARENT   = 1 << 2
};

class [[gnu::packed]] ISOSUSPEntry {
public:
	uint16_t magic;   // 0x0-0x1
	uint8_t  length;  // 0x2
	uint8_t  version; // 0x3

	inline size_t getDataLength(void) const {
		return length - sizeof(ISOSUSPEntry);
	}
	inline const uint8_t *getData(void) const {
		return &version + 1;
	}
};

class [[gnu::packed]] ISOXAEntry {
public:
	uint16_t groupID;      // 0x0
	uint16_t userID;       // 0x2
	uint16_t attributes;   // 0x4
	uint16_t magic;        // 0x6
	uint8_t  fileNumber;   // 0x8
	uint8_t  _reserved[5]; // 0x9-0xd

	inline bool validateMagic(void) const {
		return !groupID && !userID && (magic == util::concat2('X', 'A'));
	}
};

enum ISORecordFlag : uint8_t {
	ISO_RECORD_EXISTENCE    = 1 << 0,
	ISO_RECORD_DIRECTORY    = 1 << 1,
	ISO_RECORD_ASSOCIATED   = 1 << 2,
	ISO_RECORD_EXT_ATTR     = 1 << 3,
	ISO_RECORD_PROTECTION   = 1 << 4,
	ISO_RECORD_MULTI_EXTENT = 1 << 7
};

class [[gnu::packed]] ISORecord {
public:
	uint8_t   recordLength;        // 0x00
	uint8_t   extendedAttrLength;  // 0x01
	ISOUint32 lba;                 // 0x02-0x09
	ISOUint32 length;              // 0x0a-0x11
	ISODate   date;                // 0x12-0x18
	uint8_t   flags;               // 0x19
	uint8_t   interleaveLength;    // 0x1a
	uint8_t   interleaveGapLength; // 0x1b
	ISOUint16 volumeNumber;        // 0x1c-0x1f
	uint8_t   nameLength;          // 0x20

	inline size_t getNameLength(void) const {
		// The name is always padded to an *odd* number of bytes (so that the
		// record's entire length is even).
		return nameLength | 1;
	}
	inline size_t getSystemUseLength(void) const {
		return recordLength - (sizeof(ISORecord) + getNameLength());
	}
	inline const ISOCharD *getName(void) const {
		return &nameLength + 1;
	}
	inline const uint8_t *getSystemUseData(void) const {
		return getName() + getNameLength();
	}

	size_t parseName(char *output, size_t maxLength) const;
	size_t comparePath(const char *path) const;
};

class [[gnu::packed]] ISORecordBuffer : public ISORecord {
public:
	uint8_t recordData[ISO9660_MAX_RECORD_DATA_LENGTH];
};

enum ISOVolumeDescType : uint8_t {
	ISO_TYPE_BOOT_RECORD      = 0x00,
	ISO_TYPE_PRIMARY          = 0x01,
	ISO_TYPE_SUPPLEMENTAL     = 0x02,
	ISO_TYPE_VOLUME_PARTITION = 0x03,
	ISO_TYPE_TERMINATOR       = 0xff
};

class [[gnu::packed]] ISOVolumeDesc {
public:
	uint8_t type;     // 0x000
	uint8_t magic[5]; // 0x001-0x005
	uint8_t version;  // 0x006

	bool validateMagic(void) const;
};

struct [[gnu::packed]] ISOPrimaryVolumeDesc : public ISOVolumeDesc {
public:
	uint8_t   _reserved;
	ISOCharA  system[32];            // 0x008-0x027
	ISOCharD  volume[32];            // 0x028-0x047
	uint8_t   _reserved2[8];
	ISOUint32 volumeLength;          // 0x050-0x057
	uint8_t   _reserved3[32];
	ISOUint16 numVolumes;            // 0x078-0x07b
	ISOUint16 volumeNumber;          // 0x07c-0x07f
	ISOUint16 sectorLength;          // 0x080-0x083
	ISOUint32 pathTableLength;       // 0x084-0x08b
	uint32_t  pathTableLEOffsets[2]; // 0x08c-0x093
	uint32_t  pathTableBEOffsets[2]; // 0x094-0x09b
	ISORecord root;                  // 0x09c-0x0bc
	uint8_t   rootName;              // 0x09d
	ISOCharD  volumeSet[128];        // 0x0be-0x13d
	ISOCharA  publisher[128];        // 0x13e-0x1bd
	ISOCharA  dataPreparer[128];     // 0x1be-0x23d
	ISOCharA  application[128];      // 0x23e-0x2bd
	ISOCharD  copyrightFile[37];     // 0x2be-0x2e2
	ISOCharD  abstractFile[37];      // 0x2e3-0x307
	ISOCharD  bibliographicFile[37]; // 0x308-0x32c
	uint8_t   creationDate[17];      // 0x32d-0x33d
	uint8_t   modificationDate[17];  // 0x33e-0x34e
	uint8_t   expirationDate[17];    // 0x34f-0x35f
	uint8_t   effectiveDate[17];     // 0x360-0x370
	uint8_t   isoVersion;            // 0x371
	uint8_t   _reserved4;
	uint8_t   extensionData[512];    // 0x373-0x572
	uint8_t   _reserved5[653];
};

/* ISO9660 file and directory classes */

class ISO9660File : public File {
	friend class ISO9660Provider;

private:
	storage::Device *_dev;
	uint32_t        _startLBA;

	uint64_t _offset;
	uint32_t _bufferedLBA;
	uint8_t  _sectorBuffer[storage::MAX_SECTOR_LENGTH];

	bool _loadSector(uint32_t lba);

public:
	inline ISO9660File(storage::Device *dev, const ISORecord &record)
	: _dev(dev), _startLBA(record.lba.le), _offset(0), _bufferedLBA(0) {
		size = record.length.le;
	}

	size_t read(void *output, size_t length);
	uint64_t seek(uint64_t offset);
	uint64_t tell(void) const;
};

class ISO9660Directory : public Directory {
	friend class ISO9660Provider;

private:
	util::Data _records;
	uintptr_t  _ptr, _dataEnd;

public:
	bool getEntry(FileInfo &output);
	void close(void);
};

/* ISO9660 filesystem provider */

class ISO9660Provider : public Provider {
private:
	storage::Device *_dev;
	ISORecord       _root;

	bool _readData(util::Data &output, uint32_t lba, size_t numSectors);
	bool _getRecord(
		ISORecordBuffer &output, const ISORecord &root, const char *path
	);

public:
	inline ISO9660Provider(void)
	: _dev(nullptr) {}

	bool init(storage::Device &dev);
	void close(void);

	bool getFileInfo(FileInfo &output, const char *path);
	bool getFileFragments(FileFragmentTable &output, const char *path);
	Directory *openDirectory(const char *path);

	File *openFile(const char *path, uint32_t flags);
};

}
