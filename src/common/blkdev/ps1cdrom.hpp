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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/blkdev/device.hpp"
#include "ps1/cdrom.h"

namespace blkdev {

/* PS1 CD-ROM block device class */

class PS1CDROMDevice : public Device {
private:
	DeviceError _waitForIRQ(CDROMIRQType type);
	DeviceError _issueCommand(
		uint8_t       cmd,
		const uint8_t *param          = nullptr,
		size_t        paramLength     = 0,
		bool          waitForComplete = false
	);
	DeviceError _startRead(uint32_t lba);
	bool _issueUnlock(void);

public:
	uint8_t lastStatusData[16];
	size_t  lastStatusLength;

	DeviceError enumerate(void);
	DeviceError poll(void);
	void handleInterrupt(void);

	DeviceError read(void *data, uint64_t lba, size_t count);
	DeviceError readStream(
		StreamCallback callback,
		uint64_t       lba,
		size_t         count,
		void           *arg
	);

	DeviceError goIdle(bool standby = false);
};

extern PS1CDROMDevice cdrom;

}
