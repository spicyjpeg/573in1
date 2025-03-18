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
#include "common/fs/iso9660.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* Utilities */

static void _copyPVDString(char *output, const ISOCharA *input, size_t length) {
	// The strings in the PVD are padded with spaces. To make them printable,
	// any span of consecutive space characters at the end is replaced with null
	// bytes.
	bool isPadding = true;

	output += length;
	input  += length;
	*output = 0;

	for (; length; length--) {
		char ch = *(--input);

		if (isPadding && !__builtin_isgraph(ch))
			ch = 0;
		else
			isPadding = false;

		*(--output) = ch;
	}
}

static bool _recordToFileInfo(FileInfo &output, const ISORecord &record) {
	if (!record.parseName(output.name, sizeof(output.name)))
		return false;

	output.size       = record.length.le;
	output.attributes = READ_ONLY | ARCHIVE;

	if (!(record.flags & ISO_RECORD_EXISTENCE))
		output.attributes |= HIDDEN;
	if (record.flags & ISO_RECORD_DIRECTORY)
		output.attributes |= DIRECTORY;
	return true;
}

size_t ISORecord::parseName(char *output, size_t maxLength) const {
	size_t actualLength = 0;

	// Skip any CD-XA attributes and iterate over all SUSP entries to find all
	// Rock Ridge "NM" entries. Note that the name may be split across multiple
	// entries.
	auto xaEntry = reinterpret_cast<const ISOXAEntry *>(getSystemUseData());

	if (xaEntry->validateMagic())
		xaEntry++;

	auto ptr     = reinterpret_cast<const uint8_t *>(xaEntry);
	auto dataEnd = reinterpret_cast<const uint8_t *>(this) + recordLength;

	while ((ptr < dataEnd) && maxLength) {
		if (!(*ptr)) {
			ptr++;
			continue;
		}

		auto entry = reinterpret_cast<const ISOSUSPEntry *>(ptr);
		ptr       += entry->length;

		if (entry->magic != ISO_SUSP_ALTERNATE_NAME)
			continue;

		auto chunkData   = entry->getData();
		auto chunkLength = util::min(entry->getDataLength() - 1, maxLength);
		auto chunkFlags  = *(chunkData++);

		// Ignore entries representing the current and parent directories.
		if (chunkFlags & (ISO_SUSP_NAME_CURRENT | ISO_SUSP_NAME_PARENT))
			return 0;

		__builtin_memcpy(output, chunkData, chunkLength);
		output       += chunkLength;
		actualLength += chunkLength;
		maxLength    -= chunkLength;

		if (!(chunkFlags & ISO_SUSP_NAME_CONTINUE))
			break;
	}

	if (actualLength) {
		*output = 0;
		return actualLength;
	}

	// If no Rock Ridge name was found, fall back to the ISO9660 record name.
	auto isoName    = getName();
	auto isoNameEnd = isoName + nameLength;

	// Ignore names "\x00" and "\x01", which represent the current and parent
	// directories respectively.
	if ((*isoName == 0x00) || (*isoName == 0x01))
		return 0;

	while ((isoName < isoNameEnd) && maxLength) {
		// Files with no extension still have a trailing period, which needs to
		// be stripped.
		if (*isoName == ';')
			break;
		if ((isoName[0] == '.') && (isoName[1] == ';'))
			break;

		*(output++) = *(isoName++);
		actualLength++;
		maxLength--;
	}

	*output = 0;
	return actualLength;
}

size_t ISORecord::comparePath(const char *path) const {
	size_t prefixLength = 0;
	size_t actualLength = 0;

	while ((*path == '/') || (*path == '\\')) {
		prefixLength++;
		path++;
	}

	// This is pretty much the same code as parseName().
	auto xaEntry = reinterpret_cast<const ISOXAEntry *>(getSystemUseData());

	if (xaEntry->validateMagic())
		xaEntry++;

	auto ptr     = reinterpret_cast<const uint8_t *>(xaEntry);
	auto dataEnd = reinterpret_cast<const uint8_t *>(this) + recordLength;

	while (ptr < dataEnd) {
		if (!(*ptr)) {
			ptr++;
			continue;
		}

		auto entry = reinterpret_cast<const ISOSUSPEntry *>(ptr);
		ptr       += entry->length;

		if (entry->magic != ISO_SUSP_ALTERNATE_NAME)
			continue;

		auto chunkData   = entry->getData();
		auto chunkLength = entry->getDataLength() - 1;
		auto chunkFlags  = *(chunkData++);

		if (chunkFlags & (ISO_SUSP_NAME_CURRENT | ISO_SUSP_NAME_PARENT))
			return 0;

		for (size_t i = chunkLength; i; i--) {
			if (
				__builtin_toupper(*(chunkData++)) !=
				__builtin_toupper(*(path++))
			)
				return 0;
		}

		actualLength += chunkLength;

		if (!(chunkFlags & ISO_SUSP_NAME_CONTINUE))
			break;
	}

	if (actualLength)
		return prefixLength + actualLength;

	auto isoName = getName();
	actualLength = nameLength;

	if ((*isoName == 0x00) || (*isoName == 0x01))
		return 0;

	for (size_t i = actualLength; i; i--) {
		if (*isoName == ';')
			break;
		if ((isoName[0] == '.') && (isoName[1] == ';'))
			break;

		if (
			__builtin_toupper(*(isoName++)) !=
			__builtin_toupper(*(path++))
		)
			return 0;
	}

	return prefixLength + actualLength;
}

bool ISOVolumeDesc::validateMagic(void) const {
	return (util::hash(magic, sizeof(magic)) == "CD001"_h) && (version == 1);
}

/* ISO9660 file and directory classes */

bool ISO9660File::_loadSector(uint32_t lba) {
	if (lba == _bufferedLBA)
		return true;
	if (_dev->read(_sectorBuffer, lba, 1))
		return false;

	_bufferedLBA = lba;
	return true;
}

size_t ISO9660File::read(void *output, size_t length) {
	auto ptr    = reinterpret_cast<uintptr_t>(output);
	auto offset = uint32_t(_offset);

	// Do not read any data past the end of the file.
	length = util::min<size_t>(length, size_t(size) - offset);

	for (auto remaining = length; remaining > 0;) {
		auto sectorOffset = offset / _dev->sectorLength;
		auto ptrOffset    = offset % _dev->sectorLength;

		auto   lba    = _startLBA + sectorOffset;
		auto   buffer = reinterpret_cast<void *>(ptr);
		size_t readLength;

		if (
			!ptrOffset &&
			(remaining >= _dev->sectorLength) &&
			blkdev::isBufferAligned(buffer)
		) {
			// If the read offset is on a sector boundary, at least one sector's
			// worth of data needs to be read and the pointer satisfies any DMA
			// alignment requirements, read as many full sectors as possible
			// directly into the output buffer.
			auto numSectors = remaining / _dev->sectorLength;
			auto remainder  = remaining % _dev->sectorLength;
			readLength      = remaining - remainder;

			if (_dev->read(buffer, lba, numSectors))
				return 0;
		} else {
			// In all other cases, read one sector at a time into the sector
			// buffer and copy the requested data over.
			readLength =
				util::min<size_t>(remaining, _dev->sectorLength - ptrOffset);

			if (!_loadSector(lba))
				return 0;

			__builtin_memcpy(buffer, &_sectorBuffer[ptrOffset], readLength);
		}

		ptr       += readLength;
		offset    += readLength;
		remaining -= readLength;
	}

	_offset += length;
	return length;
}

uint64_t ISO9660File::seek(uint64_t offset) {
	_offset = util::min(offset, size);

	return _offset;
}

uint64_t ISO9660File::tell(void) const {
	return _offset;
}

bool ISO9660Directory::getEntry(FileInfo &output) {
	while (_ptr < _dataEnd) {
		auto record = reinterpret_cast<const ISORecord *>(_ptr);

		// Skip any null padding bytes inserted between entries to prevent them
		// from crossing sector boundaries.
		if (!(record->recordLength)) {
			_ptr += 2;
			continue;
		}

		_ptr += record->recordLength;

		if (_recordToFileInfo(output, *record))
			return true;
	}

	return false;
}

void ISO9660Directory::close(void) {
	_records.destroy();
}

/* ISO9660 filesystem provider */

static constexpr uint32_t _VOLUME_DESC_START_LBA = 0x10;
static constexpr uint32_t _VOLUME_DESC_END_LBA   = 0x20;

bool ISO9660Provider::_readData(
	util::Data &output, uint32_t lba, size_t numSectors
) const {
	if (!output.allocate(numSectors * _dev->sectorLength))
		return false;
	if (_dev->read(output.ptr, lba, numSectors))
		return false;

	return true;
}

bool ISO9660Provider::_getRecord(
	ISORecordBuffer &output, const ISORecord &root, const char *path
) const {
	if (!type)
		return false;

	if (!(*path)) {
		__builtin_memcpy(&output, &root, root.recordLength);
		return true;
	}

	util::Data records;
	auto       numSectors =
		(root.length.le + _dev->sectorLength - 1) / _dev->sectorLength;

	if (!_readData(records, root.lba.le, numSectors))
		return false;

	// Iterate over all records in the directory.
	auto ptr     = reinterpret_cast<uintptr_t>(records.ptr);
	auto dataEnd = ptr + root.length.le;

	while (ptr < dataEnd) {
		auto record = reinterpret_cast<const ISORecord *>(ptr);

		// Skip any null padding bytes inserted between entries to prevent them
		// from crossing sector boundaries.
		if (!(record->recordLength)) {
			ptr += 2;
			continue;
		}

		auto nameLength = record->comparePath(path);

		if (!nameLength) {
			ptr += record->recordLength;
			continue;
		}

		// If the name matches, move onto the next component of the path and
		// recursively search the subdirectory.
		path      += nameLength;
		auto found = _getRecord(output, *record, path);

		return found;
	}

	LOG_FS("not found: %s", path);
	return false;
}

bool ISO9660Provider::init(blkdev::Device &dev) {
	if (type)
		return false;

	// Locate and parse the primary volume descriptor.
	size_t numPVDSectors = util::min(
		sizeof(ISOPrimaryVolumeDesc) / _dev->sectorLength, size_t(1)
	);

	for (
		uint32_t lba = _VOLUME_DESC_START_LBA; lba < _VOLUME_DESC_END_LBA; lba++
	) {
		uint8_t pvdBuffer[blkdev::MAX_SECTOR_LENGTH];

		if (_dev->read(pvdBuffer, lba, numPVDSectors))
			return false;

		auto pvd = reinterpret_cast<const ISOPrimaryVolumeDesc *>(pvdBuffer);

		if (!pvd->validateMagic()) {
			LOG_FS("invalid ISO descriptor, lba=0x%x", lba);
			return false;
		}
		if (pvd->type == ISO_TYPE_TERMINATOR)
			break;
		if (pvd->type != ISO_TYPE_PRIMARY)
			continue;

		if (pvd->isoVersion != 1) {
			LOG_FS("unsupported ISO version 0x%02x", pvd->isoVersion);
			return false;
		}
		if (pvd->sectorLength.le != _dev->sectorLength) {
			LOG_FS("mismatching ISO sector size: %d", pvd->sectorLength.le);
			return false;
		}

		_copyPVDString(volumeLabel, pvd->volume, sizeof(pvd->volume));
		util::copy(_root, pvd->root);

		type     = ISO9660;
		capacity = uint64_t(pvd->volumeLength.le) * _dev->sectorLength;
		_dev     = &dev;

		LOG_FS("mounted ISO: %s", volumeLabel);
		return true;
	}

	LOG_FS("no ISO PVD found");
	return false;
}

void ISO9660Provider::close(void) {
	type     = NONE;
	capacity = 0;
	_dev     = nullptr;
}

bool ISO9660Provider::getFileInfo(FileInfo &output, const char *path) {
	ISORecordBuffer record;

	if (!_getRecord(record, _root, path))
		return false;

	return _recordToFileInfo(output, record);
}

bool ISO9660Provider::getFileFragments(
	FileFragmentTable &output, const char *path
) {
	ISORecordBuffer record;

	if (!_getRecord(record, _root, path))
		return false;

	// ISO9660 files are always contiguous and non-fragmented, so only a single
	// fragment is needed.
	if (!output.allocate<FileFragment>(1))
		return false;

	auto fragment    = output.as<FileFragment>();
	fragment->lba    = record.lba.le;
	fragment->length =
		(record.length.le + _dev->sectorLength - 1) / _dev->sectorLength;
	return true;
}

Directory *ISO9660Provider::openDirectory(const char *path) {
	ISORecordBuffer record;

	if (!_getRecord(record, _root, path))
		return nullptr;
	if (!(record.flags & ISO_RECORD_DIRECTORY))
		return nullptr;

	auto   dir       = new ISO9660Directory();
	size_t dirLength =
		(record.length.le + _dev->sectorLength - 1) / _dev->sectorLength;

	if (!_readData(dir->_records, record.lba.le, dirLength)) {
		LOG_FS("read failed: %s", path);
		delete dir;
		return nullptr;
	}

	dir->_ptr     = reinterpret_cast<uintptr_t>(dir->_records.ptr);
	dir->_dataEnd = dir->_ptr + record.length.le;
	return dir;
}

File *ISO9660Provider::openFile(const char *path, uint32_t flags) {
	ISORecordBuffer record;

	if (flags & (WRITE | FORCE_CREATE))
		return nullptr;
	if (!_getRecord(record, _root, path))
		return nullptr;
	if (record.flags & ISO_RECORD_DIRECTORY)
		return nullptr;

	auto file = new ISO9660File();

	file->_dev         = _dev;
	file->_startLBA    = record.lba.le;
	file->_offset      = 0;
	file->_bufferedLBA = 0;
	file->size         = record.length.le;
	return file;
}

}
