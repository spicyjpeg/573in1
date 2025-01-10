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
#include "common/util/templates.hpp"
#include "common/io.hpp"

namespace exthw {

/* PN532 command definitions */

enum PN532Command : uint8_t {
	PN532_DIAGNOSE             = 0x00,
	PN532_GET_FIRMWARE_VERSION = 0x02,
	PN532_GET_GENERAL_STATUS   = 0x04,
	PN532_READ_REG             = 0x06,
	PN532_WRITE_REG            = 0x08,
	PN532_READ_GPIO            = 0x0c,
	PN532_WRITE_GPIO           = 0x0e,
	PN532_SET_BAUD_RATE        = 0x10,
	PN532_SET_PARAMETERS       = 0x12,
	PN532_SAM_CONFIG           = 0x14,
	PN532_POWER_DOWN           = 0x16,
	PN532_RF_CONFIG            = 0x32,
	PN532_IN_DATA_EXCHANGE     = 0x40,
	PN532_IN_COMMUNICATE_THRU  = 0x42,
	PN532_IN_DESELECT          = 0x44,
	PN532_IN_JUMP_FOR_PSL      = 0x46,
	PN532_IN_LIST_TARGETS      = 0x4a,
	PN532_IN_PSL               = 0x4e,
	PN532_IN_ATR               = 0x50,
	PN532_IN_RELEASE           = 0x52,
	PN532_IN_SELECT            = 0x54,
	PN532_IN_JUMP_FOR_DEP      = 0x56,
	PN532_RF_REGULATION_TEST   = 0x58,
	PN532_IN_AUTO_POLL         = 0x60,
	PN532_TG_GET_DATA          = 0x86,
	PN532_TG_GET_COMMAND       = 0x88,
	PN532_TG_GET_TARGET_STATUS = 0x8a,
	PN532_TG_INIT              = 0x8c,
	PN532_TG_SET_DATA          = 0x8e,
	PN532_TG_SEND_RESPONSE     = 0x90,
	PN532_TG_SET_GENERAL_BYTES = 0x92,
	PN532_TG_SET_METADATA      = 0x94
};

/* PN532 parameter definitions */

enum PN532SAMMode : uint8_t {
	PN532_SAM_MODE_NORMAL       = 0x01,
	PN532_SAM_MODE_VIRTUAL_CARD = 0x02,
	PN532_SAM_MODE_WIRED_CARD   = 0x03,
	PN532_SAM_MODE_DUAL_CARD    = 0x04
};

enum PN532RFConfigItem : uint8_t {
	PN532_RF_CONFIG_FIELD          = 0x01,
	PN532_RF_CONFIG_TIMEOUTS       = 0x02,
	PN532_RF_CONFIG_MAX_RTY_COM    = 0x04,
	PN532_RF_CONFIG_MAX_RETRIES    = 0x05,
	PN532_RF_CONFIG_CIU_ISO14443A  = 0x0a,
	PN532_RF_CONFIG_CIU_FELICA     = 0x0b,
	PN532_RF_CONFIG_CIU_ISO14443B  = 0x0c,
	PN532_RF_CONFIG_CIU_ISO14443_4 = 0x0d
};

enum PN532ListTargetsType : uint8_t {
	PN532_LIST_TARGETS_ISO14443A  = 0x00,
	PN532_LIST_TARGETS_FELICA_212 = 0x01,
	PN532_LIST_TARGETS_FELICA_414 = 0x02,
	PN532_LIST_TARGETS_ISO14443B  = 0x03
};

enum PN532WakeupSource : uint8_t {
	PN532_WAKEUP_SOURCE_INT0 = 1 << 0,
	PN532_WAKEUP_SOURCE_INT1 = 1 << 1,
	PN532_WAKEUP_SOURCE_RF   = 1 << 3,
	PN532_WAKEUP_SOURCE_HSU  = 1 << 4,
	PN532_WAKEUP_SOURCE_SPI  = 1 << 5,
	PN532_WAKEUP_SOURCE_GPIO = 1 << 6,
	PN532_WAKEUP_SOURCE_I2C  = 1 << 7
};

/* FeliCa definitions */

enum FeliCaCommand : uint8_t {
	FELICA_POLL                = 0x00,
	FELICA_REQUEST_SERVICE     = 0x02,
	FELICA_REQUEST_RESPONSE    = 0x04,
	FELICA_READ_WITHOUT_ENC    = 0x06,
	FELICA_WRITE_WITHOUT_ENC   = 0x08,
	FELICA_REQUEST_SYSTEM_CODE = 0x0c
};

enum FeliCaRequestCode : uint8_t {
	FELICA_REQ_CODE_NONE          = 0x00,
	FELICA_REQ_CODE_SYSTEM_CODE   = 0x01,
	FELICA_REQ_CODE_COMMUNICATION = 0x00
};

/* PN532 packet structures */

static constexpr uint8_t PN532_PACKET_PREAMBLE = 0x55;
static constexpr uint8_t PN532_PACKET_START1   = 0x00;
static constexpr uint8_t PN532_PACKET_START2   = 0xff;

enum PN532PacketAddress : uint8_t {
	PN532_ADDR_ERROR  = 0x7f,
	PN532_ADDR_DEVICE = 0xd4,
	PN532_ADDR_HOST   = 0xd5
};

class PN532PacketHeader {
public:
	uint8_t preamble, startCode[2];
	uint8_t length, lengthChecksum;
	uint8_t address;

	inline size_t getPacketLength(void) const {
		return (sizeof(PN532PacketHeader) - 1) + length + 2;
	}
	inline uint8_t *getData(void) {
		return reinterpret_cast<uint8_t *>(this + 1);
	}

	void updateChecksum(void);
	bool validateChecksum(void) const;

	void encodeCommand(int paramLength);
	bool decodeResponse(void) const;
	bool isErrorResponse(void) const;
};

template<size_t N> class PN532Packet : public PN532PacketHeader {
public:
	uint8_t command;
	uint8_t param[N];
};

class PN532ExtPacketHeader {
public:
	uint8_t preamble, startCode[2];
	uint8_t packetMagic[2];
	uint8_t length[2], lengthChecksum;
	uint8_t address;

	inline size_t getDataLength(void) const {
		return util::concat2(length[1], length[0]);
	}
	inline size_t getPacketLength(void) const {
		return (sizeof(PN532PacketHeader) - 1) + getDataLength() + 2;
	}
	inline uint8_t *getData(void) {
		return reinterpret_cast<uint8_t *>(this + 1);
	}

	void updateChecksum(void);
	bool validateChecksum(void) const;

	void encodeCommand(int paramLength);
	bool decodeResponse(void) const;
};

template<size_t N> class PN532ExtPacket : public PN532ExtPacketHeader {
public:
	uint8_t command;
	uint8_t param[N];
};

class PN532ACKPacket {
public:
	uint8_t preamble, startCode[2];
	uint8_t packetMagic[2];
	uint8_t postamble;

	void encodeACK(bool isACK = true);
	bool decodeACK(void) const;
};

/* PN532 driver */

class PN532Driver {
private:
	io::UARTDriver &_serial;
	bool           _isIdle;

	template<size_t N> inline bool _transact(
		const PN532PacketHeader &request,
		PN532Packet<N>          &response
	) {
		return _transact(request, response, sizeof(response));
	}

	bool _transact(
		const PN532PacketHeader &request,
		PN532PacketHeader       &response,
		size_t                  maxRespLength
	);

public:
	inline PN532Driver(io::UARTDriver &serial)
	: _serial(serial) {}

	bool init(void);
	bool setMaxRetries(int count);
	bool goIdle(void);

	size_t readISO14443CardID(uint8_t *output);
	size_t readFeliCaCardID(uint8_t *output, uint16_t systemCode = 0xffff);
};

}
