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
#include "common/util/hash.hpp"
#include "common/util/templates.hpp"

namespace fs {

/* Package index parser */

struct PackageIndexHeader {
public:
	uint32_t indexLength;
	uint16_t numBuckets, numEntries;
};

class PackageIndexEntry {
public:
	util::Hash id;
	uint16_t   nameOffset, chained;
	uint64_t   offset;
	uint32_t   compLength, uncompLength;

	inline util::Hash getHash(void) const {
		return id;
	}
	inline uint16_t getChained(void) const {
		return chained;
	}
};

/* Package filesystem provider */

// The current implementation only supports loading an entire file at once.
class PackageProvider : public Provider {
private:
	util::Data _index;
	File       *_file;

	const PackageIndexEntry *_getEntry(const char *path) const;

public:
	bool init(File *file);
	bool init(const void *packageData, size_t length);
	void close(void);

	bool getFileInfo(FileInfo &output, const char *path);

	size_t loadData(util::Data &output, const char *path);
	size_t loadData(void *output, size_t length, const char *path);
};

}
