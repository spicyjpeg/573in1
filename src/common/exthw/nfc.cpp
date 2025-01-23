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
#include <stdint.h>
#include "common/exthw/nfc.hpp"
#include "common/util/log.hpp"
#include "common/util/templates.hpp"
#include "ps1/system.h"

/*
 * This is a simple driver for a PN532 NFC reader module connected to the 573's
 * serial port. Currently the only supported feature is reading the ID of a
 * Mifare or FeliCa card. The module can be wired to the cartridge slot, or to
 * the unpopulated CN24 header on main board revisions that have it, as follows:
 *
 * | CN24 pin | Cart slot pin  | Module pin                                   |
 * | -------: |- ------------: | :------------------------------------------- |
 * |          | 21, 22, 41, 42 | `VCC` (via 3.3V regulator, see note)         |
 * |        1 |              5 | `SCL`/`HSU_RX` (via level shifter, see note) |
 * |        2 |              6 | `SDA`/`HSU_TX` (via level shifter, see note) |
 * |     3, 4 |     1, 2, 8, 9 | `GND`, `I0`, `I1`                            |
 * |     5, 6 |         43, 44 | None (short pins together on 573 side)       |
 *
 * The PN532 operates at 3.3V so a voltage regulator and level shifter are
 * required to adapt it to the 573's 5V signals (some modules already include
 * them and can thus be wired directly). The module must be configured for HSU
 * (serial) mode through the appropriate jumpers or DIP switches, or by
 * grounding the `I0` and `I1` pins.
 *
 * Alternatively the PN532 module may be connected through an RS-232 level
 * translator to the "network" port on the security cartridge (if any):
 *
 * | "Network" pin | Module pin                              |
 * | ------------: | :-------------------------------------- |
 * |             1 | `SCL`/`HSU_RX` (via RS-232 transceiver) |
 * |             2 | `SDA`/`HSU_TX` (via RS-232 transceiver) |
 * |             5 | `GND`, `I0`, `I1`                       |
 *
 * The module and transceiver will have to be powered from an external source as
 * the "network" port is galvanically isolated from the rest of the system.
 */

namespace exthw {

/* PN532 packet structures */

void PN532PacketHeader::updateChecksum(void) {
#if 0
	lengthChecksum = -int(length) & 0xff;
#endif

	auto data    = &address;
	data[length] = -int(util::sum(data, length)) & 0xff;
}

bool PN532PacketHeader::validateChecksum(void) const {
	if (lengthChecksum != (-int(length) & 0xff))
		return false;

	auto    data  = &address;
	uint8_t value = -int(util::sum(data, length)) & 0xff;

	if (value != data[length]) {
		LOG_IO("mismatch, exp=0x%02x, got=0x%02x", value, data[length]);
		return false;
	}

	return true;
}

void PN532PacketHeader::encodeCommand(int paramLength) {
	paramLength += 2;

	preamble       = PN532_PACKET_PREAMBLE;
	startCode[0]   = PN532_PACKET_START1;
	startCode[1]   = PN532_PACKET_START2;
	length         = paramLength  & 0xff;
	lengthChecksum = -paramLength & 0xff;
	address        = PN532_ADDR_DEVICE;

	updateChecksum();
}

bool PN532PacketHeader::decodeResponse(void) const {
	if (
		(startCode[0] != PN532_PACKET_START1) ||
		(startCode[1] != PN532_PACKET_START2)
	)
		return false;
	if (address != PN532_ADDR_HOST)
		return false;

	return validateChecksum();
}

bool PN532PacketHeader::isErrorResponse(void) const {
	if (
		(startCode[0] != PN532_PACKET_START1) ||
		(startCode[1] != PN532_PACKET_START2)
	)
		return false;
	if ((length != 1) || (lengthChecksum != 0xff))
		return false;

	return (address == PN532_ADDR_ERROR);
}

void PN532ExtPacketHeader::updateChecksum(void) {
#if 0
	lengthChecksum = -int(length[0] + length[1]) & 0xff;
#endif

	auto data    = &address;
	auto length  = getDataLength();
	data[length] = -int(util::sum(data, length)) & 0xff;
}

bool PN532ExtPacketHeader::validateChecksum(void) const {
	if (lengthChecksum != (-int(length[0] + length[1]) & 0xff))
		return false;

	auto    data    = &address;
	auto    _length = getDataLength();
	uint8_t value   = -int(util::sum(data, _length)) & 0xff;

	if (value != data[_length]) {
		LOG_IO("mismatch, exp=0x%02x, got=0x%02x", value, data[_length]);
		return false;
	}

	return true;
}

void PN532ExtPacketHeader::encodeCommand(int paramLength) {
	paramLength += 2;

	preamble       = PN532_PACKET_PREAMBLE;
	startCode[0]   = PN532_PACKET_START1;
	startCode[1]   = PN532_PACKET_START2;
	packetMagic[0] = 0xff;
	packetMagic[1] = 0xff;
	length[0]      = (paramLength >> 8) & 0xff;
	length[1]      = (paramLength >> 0) & 0xff;
	lengthChecksum = -int(length[0] + length[1]) & 0xff;
	address        = PN532_ADDR_DEVICE;

	updateChecksum();
}

bool PN532ExtPacketHeader::decodeResponse(void) const {
	if (
		(startCode[0] != PN532_PACKET_START1) ||
		(startCode[1] != PN532_PACKET_START2)
	)
		return false;
	if ((packetMagic[0] != 0xff) || (packetMagic[1] != 0xff))
		return false;
	if (address != PN532_ADDR_HOST)
		return false;

	return validateChecksum();
}

void PN532ACKPacket::encodeACK(bool isACK) {
	preamble       = PN532_PACKET_PREAMBLE;
	startCode[0]   = PN532_PACKET_START1;
	startCode[1]   = PN532_PACKET_START2;
	packetMagic[0] = isACK ? 0x00 : 0xff;
	packetMagic[1] = isACK ? 0xff : 0x00;
	postamble      = PN532_PACKET_PREAMBLE;
}

bool PN532ACKPacket::decodeACK(void) const {
	if (
		(startCode[0] != PN532_PACKET_START1) ||
		(startCode[1] != PN532_PACKET_START2)
	)
		return false;
	if ((packetMagic[0] != 0x00) || (packetMagic[1] != 0xff))
		return false;

	return true;
}

/* PN532 driver */

static constexpr int _POWER_DOWN_DELAY = 1000;
static constexpr int _WAKEUP_DELAY     = 1000;
static constexpr int _ACK_TIMEOUT      = 1000;
static constexpr int _RESPONSE_TIMEOUT = 3000000;

static constexpr int _DEFAULT_BAUD_RATE    = 115200;
static constexpr int _MAX_SEND_ATTEMPTS    = 3;
static constexpr int _MAX_RECEIVE_ATTEMPTS = 3;

bool PN532Driver::_transact(
	const PN532PacketHeader &request,
	PN532PacketHeader       &response,
	size_t                  maxRespLength
) {
	if (!_serial.isConnected()) {
		LOG_IO("serial port not connected");
		return false;
	}
	if (_isIdle) {
		// If the PN532 is powered down, it must be woken up by sending at least
		// 5 rising edges on TX before it can accept a new command.
		_serial.writeByte(PN532_PACKET_PREAMBLE);
		delayMicroseconds(_WAKEUP_DELAY);

		_isIdle = false;
	}

	// Keep sending the request until an acknowledge packet is received.
	for (int i = _MAX_SEND_ATTEMPTS; i; i--) {
		_serial.writeBytes(&request.preamble, request.getPacketLength());

		PN532ACKPacket ack;

		if (
			_serial.readBytes(&ack.preamble, sizeof(ack), _ACK_TIMEOUT)
			< sizeof(ack)
		)
			continue;
		if (!ack.decodeACK())
			continue;

		// Wait for a response, then validate it and send a NACK to request
		// retransmission if needed.
		for (int i = _MAX_RECEIVE_ATTEMPTS; i; i--) {
			if (_serial.readBytes(
				&response.preamble,
				maxRespLength,
				_RESPONSE_TIMEOUT
			) >= sizeof(response)) {
				if (response.decodeResponse())
					return true;

				if (response.isErrorResponse()) {
					LOG_IO("PN532 error");
					return false;
				}
			}

			ack.encodeACK(false);
			_serial.writeBytes(&ack.preamble, sizeof(ack));
		}

		LOG_IO("too many receive attempts failed");
		return false;
	}

	LOG_IO("too many send attempts failed");
	return false;
}

bool PN532Driver::init(void) {
	_serial.init(_DEFAULT_BAUD_RATE);
	_isIdle = true;

	PN532Packet<4> packet;

	packet.command = PN532_GET_FIRMWARE_VERSION;
	packet.encodeCommand(0);

	if (!_transact(packet, packet))
		return false;

	if (packet.param[0] != 0x32) {
		LOG_IO("unsupported NFC chip, id=0x%02x", packet.param[0]);
		return false;
	}

	LOG_IO("found PN532 v%d.%d", packet.param[1], packet.param[2]);

	// This command is required to exit "low VBAT" mode.
	packet.command  = PN532_SAM_CONFIG;
	packet.param[0] = PN532_SAM_MODE_NORMAL;
	packet.encodeCommand(1);

	if (!_transact(packet, packet))
		return false;

	return setMaxRetries(0);
}

bool PN532Driver::setMaxRetries(int count) {
	if (count < 0)
		count = 0xff;

	PN532Packet<4> packet;

	packet.command  = PN532_RF_CONFIG;
	packet.param[0] = PN532_RF_CONFIG_MAX_RTY_COM;
	packet.param[1] = count;
	packet.encodeCommand(2);

	if (!_transact(packet, packet))
		return false;

	packet.command  = PN532_RF_CONFIG;
	packet.param[0] = PN532_RF_CONFIG_MAX_RETRIES;
	packet.param[1] = count;
	packet.param[2] = count;
	packet.param[3] = count;
	packet.encodeCommand(4);

	return _transact(packet, packet);
}

bool PN532Driver::goIdle(void) {
	PN532Packet<1> packet;

	packet.command  = PN532_POWER_DOWN;
	packet.param[0] = PN532_WAKEUP_SOURCE_HSU;
	packet.encodeCommand(1);

	if (!_transact(packet, packet))
		return false;

	delayMicroseconds(_POWER_DOWN_DELAY);
	_isIdle = true;
	return true;
}

size_t PN532Driver::readISO14443CardID(uint8_t *output) {
	PN532Packet<1 + 11> packet;

	packet.command  = PN532_IN_LIST_TARGETS;
	packet.param[0] = 1;
	packet.param[1] = PN532_LIST_TARGETS_ISO14443A;
	packet.encodeCommand(2);

	if (!_transact(packet, packet))
		return 0;
	if (!packet.param[0])
		return 0;

	auto idLength = packet.param[4];
#if 0
	auto atqa     = util::concat2(packet.param[2], packet.param[1]);
	auto saq      = packet.param[3];

	char buffer[32];

	util::hexToString(buffer, &packet.param[5], idLength, '-');
	LOG_IO("%s", buffer);
	LOG_IO("atqa=0x%04x, saq=0x%02x", atqa, saq);
#endif

	__builtin_memcpy(output, &packet.param[5], idLength);
	return idLength;
}

size_t PN532Driver::readFeliCaCardID(uint8_t *output, uint16_t systemCode) {
	PN532Packet<1 + 20> packet;

	packet.command  = PN532_IN_LIST_TARGETS;
	packet.param[0] = 1;
	packet.param[1] = PN532_LIST_TARGETS_FELICA_212;
	packet.param[2] = FELICA_POLL;
	packet.param[3] = (systemCode >> 8) & 0xff;
	packet.param[4] = (systemCode >> 0) & 0xff;
	packet.param[5] = FELICA_REQ_CODE_NONE;
	packet.param[6] = 0;
	packet.encodeCommand(7);

	if (!_transact(packet, packet))
		return 0;
	if (!packet.param[0])
		return 0;

	auto respLength = packet.param[1];
	auto respCode   = packet.param[2];

	if ((respLength != 18) && (respLength != 20)) {
		LOG_IO("invalid response length: 0x%02x", respLength);
		return 0;
	}
	if (respCode != 0x01) {
		LOG_IO("invalid response code: 0x%02x", respCode);
		return 0;
	}

#if 0
	char buffer[24];

	util::hexToString(buffer, &packet.param[3], 8, '-');
	LOG_IO("%s", buffer);
#endif

	__builtin_memcpy(output, &packet.param[3], 8);
	return 8;
}

}
