/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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
#include <stdlib.h>
#include "common/util/containers.hpp"

namespace util {

/* Simple managed pointer */

Data::Data(void)
:
	ptr(nullptr),
	length(0),
	destructor(nullptr) {}

Data::Data(Data &&source)
:
	ptr(source.ptr),
	length(source.length),
	destructor(source.destructor) {
	source.ptr        = nullptr;
	source.length     = 0;
	source.destructor = nullptr;
}

void *Data::allocate(size_t _length) {
	destroy();

	if (!length)
		return nullptr;

	ptr        = malloc(_length);
	length     = _length;
	destructor = &free;
	return ptr;
}

void Data::destroy(void) {
	if (!ptr)
		return;
	if (destructor)
		destructor(ptr);

	ptr        = nullptr;
	length     = 0;
	destructor = nullptr;
}

}
