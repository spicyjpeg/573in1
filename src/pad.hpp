
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pad {

/* Definitions */

enum Address : uint8_t {
	ADDR_CONTROLLER   = 0x01,
	ADDR_PS2_IR       = 0x21,
	ADDR_PS2_MULTITAP = 0x61,
	ADDR_CARD         = 0x81
};

enum ResponsePrefix : uint8_t {
	PREFIX_PAD  = 0x5a,
	PREFIX_CARD = 0x5d
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

enum PadType : uint8_t {
	PAD_NONE         =  0,
	PAD_MOUSE        =  1,
	PAD_NEGCON       =  2,
	PAD_IRQ10_GUN    =  3,
	PAD_DIGITAL      =  4,
	PAD_ANALOG_STICK =  5,
	PAD_GUNCON       =  6,
	PAD_ANALOG       =  7,
	PAD_MULTITAP     =  8,
	PAD_JOGCON       = 14,
	PAD_CONFIG_MODE  = 15
};

enum PadButton : uint16_t {
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

/* API */

void init(void);
uint8_t exchangeByte(uint8_t value);

/* Controller port class */

class Port {
public:
	uint16_t sioFlags;

	PadType  padType;
	uint16_t buttons;

	inline Port(uint16_t sioFlags)
	: sioFlags(sioFlags), padType(PAD_NONE), buttons(0) {}

	bool start(uint8_t address) const;
	void stop(void) const;
	size_t exchangeBytes(
		const uint8_t *input, uint8_t *output, size_t length
	) const;
	size_t exchangePacket(
		uint8_t address, const uint8_t *request, uint8_t *response,
		size_t reqLength, size_t maxRespLength
	) const;

	bool pollPad(void);
};

extern Port ports[2];

}
