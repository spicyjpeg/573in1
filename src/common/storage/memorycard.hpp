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
#include "common/storage/device.hpp"

namespace storage {

/* PS1 memory card block device class */

class MemoryCardDevice : public Device {
private:
	uint8_t _lastStatus;

public:
	inline MemoryCardDevice(int index)
	: Device(index * IS_SECONDARY) {}

	DeviceError enumerate(void);
	DeviceError poll(void);

	DeviceError read(void *data, uint64_t lba, size_t count);
	DeviceError write(const void *data, uint64_t lba, size_t count);
};

MemoryCardDevice *newMemoryCardDevice(int index);

}
