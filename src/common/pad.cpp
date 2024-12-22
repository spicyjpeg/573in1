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
#include "common/util/templates.hpp"
#include "common/pad.hpp"
#include "ps1/registers.h"
#include "ps1/system.h"

namespace pad {

static constexpr int _BAUD_RATE   = 250000;
static constexpr int _CS_DELAY    = 60;
static constexpr int _ACK_TIMEOUT = 120;

/* Basic API */

void init(void) {
	SIO_CTRL(0) = SIO_CTRL_RESET;

	SIO_MODE(0) = SIO_MODE_BAUD_DIV1 | SIO_MODE_DATA_8;
	SIO_BAUD(0) = F_CPU / _BAUD_RATE;
	SIO_CTRL(0) = 0;
}

uint8_t exchangeByte(uint8_t value) {
	while (!(SIO_STAT(0) & SIO_STAT_TX_NOT_FULL))
		__asm__ volatile("");

	SIO_CTRL(0) |= SIO_CTRL_ACKNOWLEDGE;
	SIO_DATA(0)  = value;

	while (!(SIO_STAT(0) & SIO_STAT_RX_NOT_EMPTY))
		__asm__ volatile("");

	return SIO_DATA(0);
}

size_t exchangeBytes(
	const uint8_t *request,
	uint8_t       *response,
	size_t        reqLength,
	size_t        maxRespLength,
	bool          hasLastACK
) {
	size_t respLength = 0;

	while (respLength < maxRespLength) {
		uint8_t byte = exchangeByte(reqLength ? *(request++) : 0);
		respLength++;

		if (reqLength)
			reqLength--;
		if (response)
			*(response++) = byte;

		// Devices will not trigger /ACK after the last response byte.
		if (hasLastACK || (respLength < maxRespLength)) {
			if (!waitForInterrupt(IRQ_SIO0, _ACK_TIMEOUT))
				break;
		}
	}

	return respLength;
}

/* Controller port class */

Port ports[2]{
	(SIO_CTRL_TX_ENABLE | SIO_CTRL_RX_ENABLE | SIO_CTRL_DSR_IRQ_ENABLE
		| SIO_CTRL_CS_PORT_1),
	(SIO_CTRL_TX_ENABLE | SIO_CTRL_RX_ENABLE | SIO_CTRL_DSR_IRQ_ENABLE
		| SIO_CTRL_CS_PORT_2)
};

bool Port::start(uint8_t address) const {
	SIO_CTRL(0) = sioFlags | SIO_CTRL_DTR | SIO_CTRL_ACKNOWLEDGE;
	delayMicroseconds(_CS_DELAY);

	IRQ_STAT    = ~(1 << IRQ_SIO0);
	SIO_DATA(0) = address;

	// The controller only pulses /ACK for a brief period of time and the DSR
	// status bit in the SIO_STAT register is not latched, so the only way to
	// detect the pulse reliably is to have it trigger a dummy (latched) IRQ and
	// check for it.
	if (!waitForInterrupt(IRQ_SIO0, _ACK_TIMEOUT))
		return false;
	while (SIO_STAT(0) & SIO_STAT_RX_NOT_EMPTY)
		SIO_DATA(0);

	return true;
}

void Port::stop(void) const {
	delayMicroseconds(_CS_DELAY);
	SIO_CTRL(0) = sioFlags;
}

PortError Port::pollController(void) {
	PortLock lock(*this, ADDR_CONTROLLER);

	auto lastType  = controllerType;
	controllerType = TYPE_NONE;
	buttons        = 0;
	leftAnalog.x   = 0;
	leftAnalog.y   = 0;
	rightAnalog.x  = 0;
	rightAnalog.y  = 0;

	if (!lock.locked)
		return NO_DEVICE;

	const uint8_t request[4]{ CMD_POLL, 0, 0, 0 };
	uint8_t       response[8];

	size_t respLength = exchangeBytes(
		request,
		response,
		sizeof(request),
		sizeof(response)
	);

	if (respLength < 4)
		return INVALID_RESPONSE;
	if (response[1] != PREFIX_CONTROLLER)
		return UNSUPPORTED_DEVICE;

	controllerType = ControllerType(response[0] >> 4);
	buttons        = ~util::concat2(response[2], response[3]);

	// The PS1 mouse outputs signed motion deltas while all other controllers
	// use unsigned values.
	int offset = (controllerType == TYPE_MOUSE) ? 0 : 128;

	if (respLength >= 6) {
		rightAnalog.y = response[4] - offset;
		rightAnalog.x = response[5] - offset;
	}
	if (respLength >= 8) {
		leftAnalog.y = response[6] - offset;
		leftAnalog.x = response[7] - offset;
	}

	return (controllerType == lastType) ? NO_ERROR : CONTROLLER_CHANGED;
}

PortError Port::memoryCardRead(void *data, uint16_t lba) const {
	PortLock lock(*this, ADDR_MEMORY_CARD);

	if (!lock.locked)
		return NO_DEVICE;

	uint8_t lbaHigh = (lba >> 8) & 0xff;
	uint8_t lbaLow  = (lba >> 0) & 0xff;

	const uint8_t request[9]{
		CMD_READ_SECTOR, 0, 0, lbaHigh, lbaLow, 0, 0, 0, 0
	};
	uint8_t response[9];

	if (exchangeBytes(
		request,
		response,
		sizeof(request),
		sizeof(response),
		true
	) < sizeof(response))
		return INVALID_RESPONSE;
	if (
		(response[2] != PREFIX_MEMORY_CARD) ||
		(response[7] != lbaHigh) ||
		(response[8] != lbaLow)
	)
		return INVALID_RESPONSE;

#if 0
	if (response[0] & (1 << 3)) {
		// TODO: the "new card" flag must be cleared by issuing a dummy write
		return CARD_CHANGED;
	}
#endif

	auto ptr = reinterpret_cast<uint8_t *>(data);

	if (exchangeBytes(
		nullptr,
		ptr,
		0,
		MEMORY_CARD_SECTOR_LENGTH,
		true
	) < MEMORY_CARD_SECTOR_LENGTH)
		return INVALID_RESPONSE;

	uint8_t ackResponse[2];

	if (exchangeBytes(
		nullptr,
		ackResponse,
		0,
		sizeof(ackResponse)
	) < sizeof(ackResponse))
		return INVALID_RESPONSE;
	if (ackResponse[1] != 'G')
		return CARD_ERROR;

	uint8_t checksum = 0
		^ lbaHigh
		^ lbaLow
		^ util::bitwiseXOR(ptr, MEMORY_CARD_SECTOR_LENGTH);

	return (checksum == ackResponse[0]) ? NO_ERROR : CHECKSUM_MISMATCH;
}

PortError Port::memoryCardWrite(const void *data, uint16_t lba) const {
	PortLock lock(*this, ADDR_MEMORY_CARD);

	if (!lock.locked)
		return NO_DEVICE;

	uint8_t lbaHigh = (lba >> 8) & 0xff;
	uint8_t lbaLow  = (lba >> 0) & 0xff;

	const uint8_t request[5]{ CMD_WRITE_SECTOR, 0, 0, lbaHigh, lbaLow };
	uint8_t       response[5];

	if (exchangeBytes(
		request,
		response,
		sizeof(request),
		sizeof(response),
		true
	) < sizeof(response))
		return INVALID_RESPONSE;
	if (response[2] != PREFIX_MEMORY_CARD)
		return INVALID_RESPONSE;

	auto    ptr      = reinterpret_cast<const uint8_t *>(data);
	uint8_t checksum = 0
		^ lbaHigh
		^ lbaLow
		^ util::bitwiseXOR(ptr, MEMORY_CARD_SECTOR_LENGTH);

	if (exchangeBytes(
		ptr,
		nullptr,
		MEMORY_CARD_SECTOR_LENGTH,
		MEMORY_CARD_SECTOR_LENGTH,
		true
	) < MEMORY_CARD_SECTOR_LENGTH)
		return INVALID_RESPONSE;

	uint8_t ackResponse[4];

	if (exchangeBytes(
		&checksum,
		ackResponse,
		sizeof(checksum),
		sizeof(ackResponse)
	) < sizeof(ackResponse))
		return INVALID_RESPONSE;

	switch (ackResponse[3]) {
		case 'G':
			return NO_ERROR;

		case 'N':
			return CHECKSUM_MISMATCH;

		case 0xff:
			return CARD_ERROR;

		default:
			return INVALID_RESPONSE;
	}
}

}
