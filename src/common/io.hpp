
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ps1/registers.h"

namespace io {

/* Register and bit definitions */

enum JAMMAInput : uint32_t {
	// SYS573_JAMMA_MAIN
	JAMMA_P2_LEFT    = 1 <<  0,
	JAMMA_P2_RIGHT   = 1 <<  1,
	JAMMA_P2_UP      = 1 <<  2,
	JAMMA_P2_DOWN    = 1 <<  3,
	JAMMA_P2_BUTTON1 = 1 <<  4,
	JAMMA_P2_BUTTON2 = 1 <<  5,
	JAMMA_P2_BUTTON3 = 1 <<  6,
	JAMMA_P2_START   = 1 <<  7,
	JAMMA_P1_LEFT    = 1 <<  8,
	JAMMA_P1_RIGHT   = 1 <<  9,
	JAMMA_P1_UP      = 1 << 10,
	JAMMA_P1_DOWN    = 1 << 11,
	JAMMA_P1_BUTTON1 = 1 << 12,
	JAMMA_P1_BUTTON2 = 1 << 13,
	JAMMA_P1_BUTTON3 = 1 << 14,
	JAMMA_P1_START   = 1 << 15,

	// SYS573_JAMMA_EXT1
	JAMMA_P1_BUTTON4 = 1 << 16,
	JAMMA_P1_BUTTON5 = 1 << 17,
	JAMMA_TEST       = 1 << 18,
	JAMMA_P1_BUTTON6 = 1 << 19,

	// SYS573_JAMMA_EXT2
	JAMMA_P2_BUTTON4 = 1 << 20,
	JAMMA_P2_BUTTON5 = 1 << 21,
	JAMMA_UNKNOWN    = 1 << 22,
	JAMMA_P2_BUTTON6 = 1 << 23,

	// SYS573_MISC_IN
	JAMMA_COIN1      = 1 << 24,
	JAMMA_COIN2      = 1 << 25,
	JAMMA_PCMCIA_CD1 = 1 << 26,
	JAMMA_PCMCIA_CD2 = 1 << 27,
	JAMMA_SERVICE    = 1 << 28
};

enum CartInputPin {
	IN_1WIRE = 6
};

enum CartOutputPin {
	OUT_SDA   = 0,
	OUT_SCL   = 1,
	OUT_CS    = 2,
	OUT_RESET = 3,
	OUT_1WIRE = 4
};

enum MiscOutputPin {
	MISC_ADC_MOSI    = 0,
	MISC_ADC_CS      = 1,
	MISC_ADC_SCK     = 2,
	MISC_COIN_COUNT1 = 3,
	MISC_COIN_COUNT2 = 4,
	MISC_AMP_ENABLE  = 5,
	MISC_CDDA_ENABLE = 6,
	MISC_SPU_ENABLE  = 7,
	MISC_JVS_STAT    = 8
};

/* Inputs */

static inline void clearWatchdog(void) {
	SYS573_WATCHDOG = 0;
}

static inline uint32_t getDIPSwitches(void) {
	return SYS573_DIP_CART & 0xf;
}

static inline bool getCartInsertionStatus(void) {
	return (SIO_STAT(1) / SIO_STAT_DSR) & 1;
}

static inline bool getCartSerialStatus(void) {
	SIO_CTRL(1) |= SIO_CTRL_RTS;
	return (SIO_STAT(1) / SIO_STAT_CTS) & 1;
}

/* Bitbanged I/O */

extern uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

static inline bool getCartInput(CartInputPin pin) {
	return (SYS573_DIP_CART >> (8 + pin)) & 1;
}

static inline bool getCartSDA(void) {
	return (SYS573_MISC_IN / SYS573_MISC_IN_CART_SDA) & 1;
}

static inline void setCartOutput(CartOutputPin pin, bool value) {
	if (value)
		_cartOutputReg |= 1 << pin;
	else
		_cartOutputReg &= ~(1 << pin);

	SYS573_CART_OUT = _cartOutputReg;
}

static inline void setFlashBank(int bank) {
	_bankSwitchReg = (_bankSwitchReg & (3 << 6)) | bank;

	SYS573_BANK_CTRL = _bankSwitchReg;
}

static inline void setCartSDADir(bool dir) {
	if (dir)
		_bankSwitchReg |= 1 << 6;
	else
		_bankSwitchReg &= ~(1 << 6);

	SYS573_BANK_CTRL = _bankSwitchReg;
}

static inline void setMiscOutput(MiscOutputPin pin, bool value) {
	if (value)
		_miscOutputReg |= 1 << pin;
	else
		_miscOutputReg &= ~(1 << pin);

	SYS573_MISC_OUT = _miscOutputReg;
}

/* Digital I/O board driver */

// TODO: these do not seem to actually be LDC and HDC...
static inline bool isDigitalIOPresent(void) {
	return (
		(SYS573D_CPLD_STAT & (SYS573D_CPLD_STAT_LDC | SYS573D_CPLD_STAT_HDC))
		== SYS573D_CPLD_STAT_HDC
	);
}

static inline bool getDIO1Wire(void) {
	return (SYS573D_FPGA_DS2401 >> 12) & 1;
}

static inline void setDIO1Wire(bool value) {
	SYS573D_FPGA_DS2401 = (value ^ 1) << 12;
}

/* Other APIs */

void init(void);
uint32_t getJAMMAInputs(void);
uint32_t getRTCTime(void);
bool isRTCBatteryLow(void);

bool loadBitstream(const uint8_t *data, size_t length);
void initKonamiBitstream(void);

void i2cStart(void);
void i2cStartWithCS(int csDelay = 0);
void i2cStop(void);
void i2cStopWithCS(int csDelay = 0);

uint8_t i2cReadByte(void);
void i2cWriteByte(uint8_t value);
void i2cSendACK(bool ack);
bool i2cGetACK(void);
void i2cReadBytes(uint8_t *data, size_t length);
bool i2cWriteBytes(const uint8_t *data, size_t length, int lastACKDelay = 0);

uint32_t i2cResetX76(void);
uint32_t i2cResetZS01(void);

bool dsCartReset(void);
bool dsDIOReset(void);
uint8_t dsCartReadByte(void);
uint8_t dsDIOReadByte(void);
void dsCartWriteByte(uint8_t value);
void dsDIOWriteByte(uint8_t value);

}
