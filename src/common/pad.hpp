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

namespace pad {

/* Definitions */

static constexpr size_t MEMORY_CARD_SECTOR_LENGTH = 128;

enum Address : uint8_t {
	ADDR_CONTROLLER   = 0x01,
	ADDR_PS2_IR       = 0x21,
	ADDR_PS2_MULTITAP = 0x61,
	ADDR_MEMORY_CARD  = 0x81
};

enum ResponsePrefix : uint8_t {
	PREFIX_CONTROLLER  = 0x5a,
	PREFIX_MEMORY_CARD = 0x5d
};

enum Command : uint8_t {
	// Basic controller commands
	CMD_POLL   = 'B',
	CMD_CONFIG = 'C',

	// Configuration mode commands
	CMD_INIT_PRESSURE = '@', // DualShock 2 only
	CMD_RESP_INFO     = 'A', // DualShock 2 only
	CMD_SET_ANALOG    = 'D',
	CMD_GET_ANALOG    = 'E',
	CMD_MOTOR_INFO    = 'F',
	CMD_MOTOR_LIST    = 'G',
	CMD_MOTOR_STATE   = 'H',
	CMD_GET_MODES     = 'L',
	CMD_REQ_CONFIG    = 'M',
	CMD_RESP_CONFIG   = 'O', // DualShock 2 only

	// Memory card commands
	CMD_READ_SECTOR   = 'R',
	CMD_IDENTIFY_CARD = 'S', // OEM cards only
	CMD_WRITE_SECTOR  = 'W'
};

enum ControllerType : uint8_t {
	TYPE_NONE         =  0,
	TYPE_MOUSE        =  1,
	TYPE_NEGCON       =  2,
	TYPE_IRQ10_GUN    =  3,
	TYPE_DIGITAL      =  4,
	TYPE_ANALOG_STICK =  5,
	TYPE_GUNCON       =  6,
	TYPE_ANALOG       =  7,
	TYPE_MULTITAP     =  8,
	TYPE_JOGCON       = 14,
	TYPE_CONFIG_MODE  = 15
};

enum ControllerButton : uint16_t {
	// Standard controllers
	BTN_SELECT   = 1 <<  0,
	BTN_L3       = 1 <<  1,
	BTN_R3       = 1 <<  2,
	BTN_START    = 1 <<  3,
	BTN_UP       = 1 <<  4,
	BTN_RIGHT    = 1 <<  5,
	BTN_DOWN     = 1 <<  6,
	BTN_LEFT     = 1 <<  7,
	BTN_L2       = 1 <<  8,
	BTN_R2       = 1 <<  9,
	BTN_L1       = 1 << 10,
	BTN_R1       = 1 << 11,
	BTN_TRIANGLE = 1 << 12,
	BTN_CIRCLE   = 1 << 13,
	BTN_CROSS    = 1 << 14,
	BTN_SQUARE   = 1 << 15,

	// Mouse
	BTN_MOUSE_RIGHT = 1 << 10,
	BTN_MOUSE_LEFT  = 1 << 11,

	// neGcon
	BTN_NEGCON_START = 1 <<  3,
	BTN_NEGCON_UP    = 1 <<  4,
	BTN_NEGCON_RIGHT = 1 <<  5,
	BTN_NEGCON_DOWN  = 1 <<  6,
	BTN_NEGCON_LEFT  = 1 <<  7,
	BTN_NEGCON_R     = 1 << 11,
	BTN_NEGCON_B     = 1 << 12,
	BTN_NEGCON_A     = 1 << 13,

	// Guncon
	BTN_GUNCON_A       = 1 <<  3,
	BTN_GUNCON_TRIGGER = 1 << 13,
	BTN_GUNCON_B       = 1 << 14,

	// IRQ10 lightgun
	BTN_IRQ10_GUN_START   = 1 <<  3,
	BTN_IRQ10_GUN_BACK    = 1 << 14,
	BTN_IRQ10_GUN_TRIGGER = 1 << 15
};

/* Basic API */

void init(void);
uint8_t exchangeByte(uint8_t value);
size_t exchangeBytes(
	const uint8_t *request,
	uint8_t       *response,
	size_t        reqLength,
	size_t        maxRespLength,
	bool          hasLastACK = false
);

/* Controller port class */

enum PortError {
	NO_ERROR           = 0,
	NO_DEVICE          = 1,
	UNSUPPORTED_DEVICE = 2,
	INVALID_RESPONSE   = 3,
	CHECKSUM_MISMATCH  = 4,
	CARD_ERROR         = 5,
	CONTROLLER_CHANGED = 6,
	CARD_CHANGED       = 7
};

struct AnalogState {
public:
	int8_t x, y;
};

class Port {
public:
	uint16_t sioFlags;

	ControllerType controllerType;
	uint16_t       buttons;
	AnalogState    leftAnalog, rightAnalog;

	inline Port(uint16_t sioFlags)
	: sioFlags(sioFlags), controllerType(TYPE_NONE), buttons(0) {}

	bool start(uint8_t address) const;
	void stop(void) const;

	PortError pollController(void);
	PortError memoryCardRead(void *data, uint16_t lba) const;
	PortError memoryCardWrite(const void *data, uint16_t lba) const;
};

class PortLock {
private:
	const Port &_port;

public:
	bool locked;

	inline PortLock(const Port &port, uint8_t address)
	: _port(port) {
		locked = _port.start(address);
	}
	inline ~PortLock(void) {
		_port.stop();
	}
};

extern Port ports[2];

}
