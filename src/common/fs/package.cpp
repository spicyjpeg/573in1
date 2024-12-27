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
#include "common/fs/file.hpp"
#include "common/fs/package.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/util/string.hpp"

namespace fs {

/* Package filesystem provider */

const PackageIndexEntry *PackageProvider::_getEntry(const char *path) const {
	// Any leading path separators must be stripped manually.
	while ((*path == '/') || (*path == '\\'))
		path++;

	auto header = _index.as<PackageIndexHeader>();

	return util::getHashTableEntry(
		reinterpret_cast<const PackageIndexEntry *>(header + 1),
		header->numBuckets,
		util::hash(path)
	);
}

bool PackageProvider::init(File *file) {
	if (type)
		return false;

	_file = file;

	// Parse the package's header to obtain the size of the index, then reread
	// the entire index section.
	PackageIndexHeader header;

	if (file->read(&header, sizeof(header)) < sizeof(header))
		return false;

	size_t indexLength = header.indexLength;

	if (!_index.allocate(indexLength))
		return false;

	if (
		file->seek(0) ||
		(file->read(_index.ptr, indexLength) != indexLength)
	) {
		_index.destroy();
		return false;
	}

	type     = PACKAGE_FILE;
	capacity = file->size - indexLength;

	LOG_FS("mounted package file");
	return true;
}

bool PackageProvider::init(const void *packageData, size_t length) {
	if (type)
		return false;

	auto header = reinterpret_cast<const PackageIndexHeader *>(packageData);

	_file               = nullptr;
	_index.ptr          = reinterpret_cast<void *>(uintptr_t(packageData));
	_index.length       = header->indexLength;
	_index.destructible = false;

	type     = PACKAGE_MEMORY;
	capacity = length - header->indexLength;

	LOG_FS("mounted package: 0x%08x", packageData);
	return true;
}

void PackageProvider::close(void) {
	if (!type)
		return;

	_index.destroy();

#if 0
	if (_file) {
		_file->close();
		delete _file;
	}
#endif

	type     = NONE;
	capacity = 0;
}

bool PackageProvider::getFileInfo(FileInfo &output, const char *path) {
	auto blob  = _index.as<char>();
	auto entry = _getEntry(path);

	if (!entry)
		return false;

#if 0
	const char *ptr = __builtin_strrchr(&blob[entry->nameOffset], '/');

	if (ptr)
		ptr++;
	else
		ptr = &blob[entry->nameOffset];
#else
	auto ptr = &blob[entry->nameOffset];
#endif

	__builtin_strncpy(output.name, ptr, sizeof(output.name));
	output.size       = entry->uncompLength;
	output.attributes = READ_ONLY | ARCHIVE;

	return true;
}

size_t PackageProvider::loadData(util::Data &output, const char *path) {
	auto blob  = _index.as<uint8_t>();
	auto entry = _getEntry(path);

	if (!entry)
		return 0;

	auto   offset       = entry->offset;
	size_t compLength   = entry->compLength;
	size_t uncompLength = entry->uncompLength;

	if (_file) {
		if (compLength) {
			// Package on disk, file compressed
			auto margin = util::getLZ4InPlaceMargin(compLength);

			if (!output.allocate(uncompLength + margin))
				return 0;

			auto compPtr = output.as<uint8_t>() + margin;

			if (
				(_file->seek(offset) != offset) ||
				(_file->read(compPtr, compLength) < compLength)
			) {
				output.destroy();
				return 0;
			}

			util::decompressLZ4(
				output.as<uint8_t>(),
				compPtr,
				uncompLength,
				compLength
			);
		} else {
			// Package on disk, file not compressed
			if (!output.allocate(uncompLength))
				return 0;

			if (_file->seek(offset) != offset) {
				output.destroy();
				return 0;
			}

			return _file->read(output.ptr, uncompLength);
		}
	} else {
		if (compLength) {
			// Package in RAM, file compressed
			if (!output.allocate(uncompLength))
				return 0;

			util::decompressLZ4(
				output.as<uint8_t>(),
				&blob[offset],
				uncompLength,
				compLength
			);
		} else {
			// Package in RAM, file not compressed (return in-place pointer)
			output.ptr          = &blob[offset];
			output.length       = uncompLength;
			output.destructible = false;
		}
	}

	return uncompLength;
}

size_t PackageProvider::loadData(void *output, size_t length, const char *path) {
	auto blob  = _index.as<uint8_t>();
	auto entry = _getEntry(path);

	if (!entry)
		return 0;

	auto   offset       = entry->offset;
	size_t compLength   = entry->compLength;
	size_t uncompLength = util::min(length, size_t(entry->uncompLength));

	if (_file) {
		if (_file->seek(offset) != offset)
			return 0;

		if (compLength) {
			// Package on disk, file compressed
			auto margin  = util::getLZ4InPlaceMargin(compLength);
			auto compPtr = reinterpret_cast<uint8_t *>(output) + margin;

			if (_file->read(compPtr, compLength) < compLength)
				return 0;

			util::decompressLZ4(
				reinterpret_cast<uint8_t *>(output),
				compPtr,
				uncompLength,
				compLength
			);
		} else {
			// Package on disk, file not compressed
			if (_file->read(output, uncompLength) < uncompLength)
				return 0;
		}
	} else {
		if (compLength)
			// Package in RAM, file compressed
			util::decompressLZ4(
				reinterpret_cast<uint8_t *>(output),
				&blob[offset],
				uncompLength,
				compLength
			);
		else
			// Package in RAM, file not compressed
			__builtin_memcpy(output, &blob[offset], uncompLength);
	}

	return uncompLength;
}

}
