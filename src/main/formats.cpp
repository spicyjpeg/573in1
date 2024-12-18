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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "common/util/hash.hpp"
#include "main/formats.hpp"

namespace formats {

/* Game database parser */

// TODO: implement

/* String table parser */

static const char _ERROR_STRING[]{ "missingno" };

const char *StringTable::get(util::Hash id) const {
	auto header = as<StringTableHeader>();
	auto blob   = as<char>();
	auto entry  = util::getHashTableEntry(
		reinterpret_cast<const StringTableEntry *>(header + 1),
		header->numBuckets,
		id
	);

	return entry ? &blob[entry->offset] : _ERROR_STRING;
}

size_t StringTable::format(
	char *buffer, size_t length, util::Hash id, ...
) const {
	va_list ap;

	va_start(ap, id);
	size_t outLength = vsnprintf(buffer, length, get(id), ap);
	va_end(ap);

	return outLength;
}

}
