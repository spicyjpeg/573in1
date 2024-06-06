
#include <stddef.h>
#include <stdint.h>
#include "common/file/file.hpp"
#include "common/file/iso9660.hpp"
#include "common/ide.hpp"
#include "common/util.hpp"

namespace file {

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

		if (__builtin_toupper(*(isoName++)) != __builtin_toupper(*(path++)))
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
	if (_device->readData(_sectorBuffer, lba, 1))
		return false;

	_bufferedLBA = lba;
	return true;
}

size_t ISO9660File::read(void *output, size_t length) {
	auto offset   = uint32_t(_offset);
	auto fileSize = size_t(size);

	// Do not read any data past the end of the file.
	if (!length || (offset >= fileSize))
		return 0;
	if (offset > (fileSize - length))
		length = fileSize - offset;

	auto ptr       = reinterpret_cast<uintptr_t>(output);
	auto remaining = length;

	while (remaining > 0) {
		auto     currentPtr   = reinterpret_cast<void *>(ptr);
		uint32_t lba          = offset / ide::ATAPI_SECTOR_SIZE + _startLBA;
		size_t   sectorOffset = offset % ide::ATAPI_SECTOR_SIZE;

		// If the output pointer is on a sector boundary and satisfies the IDE
		// driver's alignment requirements, read as many full sectors as
		// possible without going through the sector buffer.
		if (!sectorOffset && _device->isPointerAligned(currentPtr)) {
			auto numSectors = remaining  / ide::ATAPI_SECTOR_SIZE;
			auto spanLength = numSectors * ide::ATAPI_SECTOR_SIZE;

			if (numSectors > 0) {
				if (_device->readData(currentPtr, lba, numSectors))
					return false;

				offset    += spanLength;
				ptr       += spanLength;
				remaining -= spanLength;
				continue;
			}
		}

		// In all other cases, read one sector at a time into the buffer and
		// copy it over.
		auto chunkLength =
			util::min(remaining, ide::ATAPI_SECTOR_SIZE - sectorOffset);

		if (!_loadSector(lba))
			return false;

		__builtin_memcpy(currentPtr, &_sectorBuffer[sectorOffset], chunkLength);

		offset    += chunkLength;
		ptr       += chunkLength;
		remaining -= chunkLength;
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
) {
	if (!output.allocate(numSectors * ide::ATAPI_SECTOR_SIZE))
		return false;
	if (_device->readData(output.ptr, lba, numSectors))
		return false;

	return true;
}

bool ISO9660Provider::_getRecord(
	ISORecordBuffer &output, const ISORecord &root, const char *path
) {
	if (!type)
		return false;

	if (!(*path)) {
		__builtin_memcpy(&output, &root, root.recordLength);
		return true;
	}

	util::Data records;
	auto       numSectors =
		(root.length.le + ide::ATAPI_SECTOR_SIZE - 1) / ide::ATAPI_SECTOR_SIZE;

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

		records.destroy();
		return found;
	}

	records.destroy();
	LOG_FS("not found: %s", path);
	return false;
}

bool ISO9660Provider::init(int drive) {
	_device = &ide::devices[drive];

	// Locate and parse the primary volume descriptor.
	ISOPrimaryVolumeDesc pvd;

	for (
		uint32_t lba = _VOLUME_DESC_START_LBA; lba < _VOLUME_DESC_END_LBA; lba++
	) {
		if (_device->readData(&pvd, lba, 1))
			return false;
		if (!pvd.validateMagic()) {
			LOG_FS("invalid ISO descriptor, lba=0x%x", lba);
			return false;
		}

		if (pvd.type == ISO_TYPE_TERMINATOR)
			break;
		if (pvd.type != ISO_TYPE_PRIMARY)
			continue;

		if (pvd.isoVersion != 1) {
			LOG_FS("unsupported ISO version 0x%02x", pvd.isoVersion);
			return false;
		}
		if (pvd.sectorLength.le != ide::ATAPI_SECTOR_SIZE) {
			LOG_FS("unsupported ISO sector size %d", pvd.sectorLength.le);
			return false;
		}

		_copyPVDString(volumeLabel, pvd.volume, sizeof(pvd.volume));
		__builtin_memcpy(&_root, &pvd.root, sizeof(_root));

		type     = ISO9660;
		capacity = uint64_t(pvd.volumeLength.le) * ide::ATAPI_SECTOR_SIZE;

		LOG_FS("mounted ISO: %d", drive);
		return true;
	}

	LOG_FS("no ISO PVD found");
	return false;
}

void ISO9660Provider::close(void) {
	type     = NONE;
	capacity = 0;
	_device  = nullptr;
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
		(record.length.le + ide::ATAPI_SECTOR_SIZE - 1) / ide::ATAPI_SECTOR_SIZE;
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
		(record.length.le + ide::ATAPI_SECTOR_SIZE - 1) / ide::ATAPI_SECTOR_SIZE;

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

	return new ISO9660File(_device, record);
}

}
