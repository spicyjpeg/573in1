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

size_t Port::exchangeBytes(
	const uint8_t *input, uint8_t *output, size_t length
) const {
	size_t remaining = length;

	for (; remaining; remaining--) {
		*(output++) = exchangeByte(*(input++));

		// Controllers do not trigger /ACK on the last byte.
		if (remaining > 1) {
			if (!waitForInterrupt(IRQ_SIO0, _ACK_TIMEOUT))
				break;
		}
	}

	return length - remaining;
}

size_t Port::exchangePacket(
	uint8_t address, const uint8_t *request, uint8_t *response,
	size_t reqLength, size_t maxRespLength
) const {
	size_t respLength = 0;

	if (start(address)) {
		while (respLength < maxRespLength) {
			if (reqLength) {
				*(response++) = exchangeByte(*(request++));
				reqLength--;
			} else {
				*(response++) = exchangeByte(0);
			}

			respLength++;
			if (!waitForInterrupt(IRQ_SIO0, _ACK_TIMEOUT))
				break;
		}
	}

	stop();
	return respLength;
}

bool Port::pollPad(void) {
	const uint8_t request[4]{ CMD_POLL, 0, 0, 0 };
	uint8_t       response[8];

	if (exchangePacket(
		ADDR_CONTROLLER, request, response, sizeof(request), sizeof(response)
	) >= 4) {
		if (response[1] == PREFIX_PAD) {
			padType = PadType(response[0] >> 4);
			buttons = ~(response[2] | (response[3] << 8));

			return true;
		}
	}

	padType = PAD_NONE;
	buttons = 0;

	return false;
}

}
