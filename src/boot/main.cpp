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
#include "common/util/misc.hpp"
#include "common/util/string.hpp"
#include "common/util/templates.hpp"
#include "common/io.hpp"
#include "ps1/system.h"

extern "C" const uint8_t _resourceArchive[];
extern "C" const size_t  _resourceArchiveLength;

static char _ptrArg[]{ "resource.ptr=xxxxxxxx\0" };
static char _lengthArg[]{ "resource.length=xxxxxxxx\0" };

struct [[gnu::packed]] ZIPFileHeader {
public:
	uint32_t magic;
	uint16_t version, flags, compType;
	uint16_t fileTime, fileDate;
	uint32_t crc, compLength, uncompLength;
	uint16_t nameLength, extraLength;

	inline bool validateMagic(void) const {
		return (magic == util::concat4('P', 'K', 0x03, 0x04));
	}
	inline size_t getHeaderLength(void) const {
		return sizeof(ZIPFileHeader) + nameLength + extraLength;
	}
};

int main(int argc, const char **argv) {
	disableInterrupts();
	io::init();

	// Parse the header of the archive's first entry manually. This avoids
	// pulling in miniz and bloating the binary.
	// NOTE: this assumes the main executable is always the first file in the
	// archive.
	auto zipHeader  = reinterpret_cast<const ZIPFileHeader *>(_resourceArchive);
	auto ptr        = &_resourceArchive[zipHeader->getHeaderLength()];
	auto compLength = zipHeader->compLength;

	//assert(zipHeader->validateMagic());
	//assert(!zipHeader->compType);

	// Decompress only the header to determine where to place the binary in
	// memory, then rerun the decompressor on the entire executable.
	util::ExecutableHeader exeHeader;

	util::decompressLZ4(
		reinterpret_cast<uint8_t *>(&exeHeader), ptr, sizeof(exeHeader),
		compLength
	);

	auto offset = exeHeader.textOffset - util::EXECUTABLE_BODY_OFFSET;
	auto length = exeHeader.textLength + util::EXECUTABLE_BODY_OFFSET;

	util::decompressLZ4(
		reinterpret_cast<uint8_t *>(offset), ptr, length, compLength
	);
	io::clearWatchdog();

	util::ExecutableLoader loader(
		exeHeader.getEntryPoint(), exeHeader.getInitialGP(),
		exeHeader.getStackPtr()
	);

	util::hexValueToString(
		&_ptrArg[13], reinterpret_cast<uint32_t>(_resourceArchive), 8
	);
	loader.addArgument(_ptrArg);
	util::hexValueToString(&_lengthArg[16], _resourceArchiveLength, 8);
	loader.addArgument(_lengthArg);

#ifdef ENABLE_ARGV
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
