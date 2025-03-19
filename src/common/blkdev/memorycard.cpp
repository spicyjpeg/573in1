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
#include "common/blkdev/memorycard.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "common/pad.hpp"

namespace blkdev {

static constexpr uint64_t _CAPACITY         = 1024;
static constexpr size_t   _SECTOR_LENGTH    = 128;
static constexpr uint32_t _DUMMY_SECTOR_LBA = 0x3f;

/* PS1 memory card block device class */

MemoryCardDevice memoryCards[2]{ (0), (1) };

DeviceError MemoryCardDevice::enumerate(void) {
	type         = MEMORY_CARD;
	capacity     = _CAPACITY;
	sectorLength = _SECTOR_LENGTH;

	return poll();
}

DeviceError MemoryCardDevice::poll(void) {
	// Bit 3 in the card status byte is set when the card is first inserted and
	// cleared once a write command is issued, allowing for reliable detection
	// of card swaps. The "official" way to clear the flag is to overwrite
	// sector 0x3f with a dummy header; this implementation is slightly less
	// crude and tries to preserve the sector's contents.
	uint8_t sector[_SECTOR_LENGTH];

	auto error = read(sector, _DUMMY_SECTOR_LBA, 1);

	if (error)
		return error;
	if (!(_lastStatus & (1 << 3)))
		return NO_ERROR;

	error = write(sector, _DUMMY_SECTOR_LBA, 1);

	return error ? error : DISC_CHANGED;
}

DeviceError MemoryCardDevice::read(void *data, uint64_t lba, size_t count) {
	pad::PortLock lock(pad::ports[getDeviceIndex()], pad::ADDR_MEMORY_CARD);

	if (!lock.locked)
		return NO_DRIVE;

	uint8_t lbaHigh = (lba >> 8) & 0xff;
	uint8_t lbaLow  = (lba >> 0) & 0xff;

	const uint8_t request[9]{
		pad::CMD_READ_SECTOR, 0, 0, lbaHigh, lbaLow, 0, 0, 0, 0
	};
	uint8_t response[9];

	if (pad::exchangeBytes(
		request,
		response,
		sizeof(request),
		sizeof(response),
		true
	) < sizeof(response))
		return STATUS_TIMEOUT;

	if (
		(response[2] != pad::PREFIX_MEMORY_CARD) ||
		(response[7] != lbaHigh) ||
		(response[8] != lbaLow)
	)
		return COMMAND_ERROR;

	_lastStatus = response[0];

	auto ptr = reinterpret_cast<uint8_t *>(data);

	if (pad::exchangeBytes(
		nullptr,
		ptr,
		0,
		_SECTOR_LENGTH,
		true
	) < _SECTOR_LENGTH)
		return STATUS_TIMEOUT;

	uint8_t ackResponse[2];

	if (pad::exchangeBytes(
		nullptr,
		ackResponse,
		0,
		sizeof(ackResponse)
	) < sizeof(ackResponse))
		return STATUS_TIMEOUT;

	if (ackResponse[1] != 'G') {
		LOG_BLKDEV(
			"card error, code=0x%02x, st=0x%02x", ackResponse[1], _lastStatus
		);
		return DRIVE_ERROR;
	}

	uint8_t checksum = 0
		^ lbaHigh
		^ lbaLow
		^ util::bitwiseXOR(ptr, _SECTOR_LENGTH);

	if (checksum != ackResponse[0]) {
		LOG_BLKDEV(
			"mismatch, exp=0x%02x, got=0x%02x", checksum, ackResponse[0]
		);
		return CHECKSUM_MISMATCH;
	}

	return NO_ERROR;
}

DeviceError MemoryCardDevice::write(
	const void *data, uint64_t lba, size_t count
) {
	pad::PortLock lock(pad::ports[getDeviceIndex()], pad::ADDR_MEMORY_CARD);

	if (!lock.locked)
		return NO_DRIVE;

	uint8_t lbaHigh = (lba >> 8) & 0xff;
	uint8_t lbaLow  = (lba >> 0) & 0xff;

	const uint8_t request[5]{ pad::CMD_WRITE_SECTOR, 0, 0, lbaHigh, lbaLow };
	uint8_t       response[5];

	if (pad::exchangeBytes(
		request,
		response,
		sizeof(request),
		sizeof(response),
		true
	) < sizeof(response))
		return STATUS_TIMEOUT;

	if (response[2] != pad::PREFIX_MEMORY_CARD)
		return COMMAND_ERROR;

	_lastStatus = response[0];

	auto    ptr      = reinterpret_cast<const uint8_t *>(data);
	uint8_t checksum = 0
		^ lbaHigh
		^ lbaLow
		^ util::bitwiseXOR(ptr, _SECTOR_LENGTH);

	if (pad::exchangeBytes(
		ptr,
		nullptr,
		_SECTOR_LENGTH,
		_SECTOR_LENGTH,
		true
	) < _SECTOR_LENGTH)
		return STATUS_TIMEOUT;

	uint8_t ackResponse[4];

	if (pad::exchangeBytes(
		&checksum,
		ackResponse,
		sizeof(checksum),
		sizeof(ackResponse)
	) < sizeof(ackResponse))
		return STATUS_TIMEOUT;

	switch (ackResponse[3]) {
		case 'G':
			return NO_ERROR;

		case 'N':
			LOG_BLKDEV("card reported mismatch, sent=0x%02x", checksum);
			return CHECKSUM_MISMATCH;

		default:
			LOG_BLKDEV(
				"card error, code=0x%02x, st=0x%02x", ackResponse[3],
				_lastStatus
			);
			return DRIVE_ERROR;
	}
}

}
