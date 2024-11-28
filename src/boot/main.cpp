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
#include "common/fs/package.hpp"
#include "common/util/hash.hpp"
#include "common/util/misc.hpp"
#include "common/util/string.hpp"
#include "common/io.hpp"
#include "ps1/system.h"

extern "C" const uint8_t _resourcePackage[];
extern "C" const size_t  _resourcePackageLength;

static char _ptrArg[]{ "resource.ptr=xxxxxxxx\0" };
static char _lengthArg[]{ "resource.length=xxxxxxxx\0" };

int main(int argc, const char **argv) {
	disableInterrupts();
	io::init();

	auto header = \
		reinterpret_cast<const fs::PackageIndexHeader *>(_resourcePackage);
	auto entry  = util::getHashTableEntry(
		reinterpret_cast<const fs::PackageIndexEntry *>(header + 1),
		header->numBuckets,
		"binaries/main.psexe"_h
	);
	auto ptr    = &_resourcePackage[entry->offset];

	// Decompress only the header to determine where to place the binary in
	// memory, then rerun the decompressor on the entire executable.
	util::ExecutableHeader exeHeader;

	util::decompressLZ4(
		reinterpret_cast<uint8_t *>(&exeHeader),
		ptr,
		sizeof(exeHeader),
		entry->compLength
	);

	auto offset = exeHeader.textOffset - util::EXECUTABLE_BODY_OFFSET;

	util::decompressLZ4(
		reinterpret_cast<uint8_t *>(offset),
		ptr,
		entry->uncompLength,
		entry->compLength
	);
	io::clearWatchdog();

	util::ExecutableLoader loader(
		exeHeader.getEntryPoint(),
		exeHeader.getInitialGP(),
		exeHeader.getStackPtr()
	);

	util::hexValueToString(
		&_ptrArg[13], reinterpret_cast<uint32_t>(_resourcePackage), 8
	);
	loader.addArgument(_ptrArg);
	util::hexValueToString(&_lengthArg[16], _resourcePackageLength, 8);
	loader.addArgument(_lengthArg);

#ifdef ENABLE_ARGV_PARSER
	for (; argc > 0; argc--) {
		if (!loader.copyArgument(*(argv++)))
			break;
	}
#endif

	flushCache();
	io::clearWatchdog();

	loader.run();
	return 0;
}
