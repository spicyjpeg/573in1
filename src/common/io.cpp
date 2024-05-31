
#include <stddef.h>
#include <stdint.h>
#include "common/io.hpp"
#include "common/util.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace io {

uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

/* System initialization */

static constexpr int _RESET_DELAY     = 5000;
static constexpr int _FPGA_INIT_DELAY = 1000;

void init(void) {
	// Remapping the base address is required in order for IDE DMA to work
	// properly, as the BIU will output it over the address lines during a DMA
	// transfer. It does not affect non-DMA access since the BIU will replace
	// the bottommost N bits, where N is the number of address lines used, with
	// the respective CPU address bits.
	BIU_DEV0_ADDR = reinterpret_cast<uint32_t>(SYS573_IDE_CS0_BASE) & 0x1fffffff;
	BIU_DEV0_CTRL = 0
		| (7 << 0) // Write delay
		| (4 << 4) // Read delay
		| BIU_CTRL_RECOVERY
		| BIU_CTRL_HOLD
		| BIU_CTRL_FLOAT
		| BIU_CTRL_PRESTROBE
		| BIU_CTRL_WIDTH_16
		| BIU_CTRL_AUTO_INCR
		| (23 << 16) // Number of address lines
		| ( 4 << 24) // DMA read/write delay
		| BIU_CTRL_DMA_DELAY;

	// Revision D of the main board has footprints for either eight 8-bit RAM
	// chips wired as two 32-bit banks, or two 16-bit chips wired as a single
	// bank. Normally the kernel takes care of setting up the memory controller
	// appropriately, but this makes sure the configuration is correct if e.g.
	// the tool is booted through OpenBIOS instead.
	DRAM_CTRL = isDualBankRAM() ? 0x0c80 : 0x4788;

	_bankSwitchReg = 0;
	_cartOutputReg = 0;
	_miscOutputReg = 0
		| SYS573_MISC_OUT_ADC_MOSI
		| SYS573_MISC_OUT_ADC_CS
		| SYS573_MISC_OUT_ADC_SCK
		| SYS573_MISC_OUT_JVS_RESET;

	SYS573_BANK_CTRL = _bankSwitchReg;
	SYS573_CART_OUT  = _cartOutputReg;
	SYS573_MISC_OUT  = _miscOutputReg;

	clearWatchdog();
}

void initIOBoard(void) {
	// Some of the digital I/O board's light outputs are controlled by the FPGA
	// and cannot be turned off until the FPGA is initialized.
	if (isDigitalIOPresent()) {
		//SYS573D_CPLD_LIGHTS_B0 = 0xf000;
		SYS573D_CPLD_LIGHTS_C0 = 0xf000;
		SYS573D_CPLD_LIGHTS_C1 = 0xf000;
	} else {
		SYS573A_LIGHTS_A = 0x00ff;
		SYS573A_LIGHTS_B = 0x00ff;
		SYS573A_LIGHTS_C = 0x00ff;
		SYS573A_LIGHTS_D = 0x00ff;
	}
}

void resetIDEDevices(void) {
	SYS573_IDE_RESET = 0;
	delayMicroseconds(_RESET_DELAY);

	SYS573_IDE_RESET = 1;
	delayMicroseconds(_RESET_DELAY);
}

/* JAMMA and RTC functions */

uint32_t getJAMMAInputs(void) {
	uint32_t inputs;

	inputs  =  SYS573_JAMMA_MAIN;
	inputs |= (SYS573_JAMMA_EXT1 & 0x0f00) << 8;
	inputs |= (SYS573_JAMMA_EXT2 & 0x0f00) << 12;
	inputs |= (SYS573_MISC_IN    & 0x1f00) << 16;

	return inputs ^ 0x1fffffff;
}

void getRTCTime(util::Date &output) {
	SYS573_RTC_CTRL |= SYS573_RTC_CTRL_READ;

	auto second = SYS573_RTC_SECOND, minute = SYS573_RTC_MINUTE;
	auto hour   = SYS573_RTC_HOUR,   day    = SYS573_RTC_DAY;
	auto month  = SYS573_RTC_MONTH,  year   = SYS573_RTC_YEAR;

	SYS573_RTC_CTRL &= ~SYS573_RTC_CTRL_READ;

	output.year   = (year   & 15) + 10 * ((year   >> 4) & 15); // 0-99
	output.month  = (month  & 15) + 10 * ((month  >> 4) &  1); // 1-12
	output.day    = (day    & 15) + 10 * ((day    >> 4) &  3); // 1-31
	output.hour   = (hour   & 15) + 10 * ((hour   >> 4) &  3); // 0-23
	output.minute = (minute & 15) + 10 * ((minute >> 4) &  7); // 0-59
	output.second = (second & 15) + 10 * ((second >> 4) &  7); // 0-59

	output.year += (output.year < 70) ? 2000 : 1900;
}

void setRTCTime(const util::Date &value, bool stop) {
	//assert((value.year >= 1970) && (value.year <= 2069));

	int _year   = value.year % 100;
	int weekday = value.getDayOfWeek() + 1;

	int year    = (_year        % 10) | (((_year        / 10) & 15) << 4);
	int month   = (value.month  % 10) | (((value.month  / 10) &  1) << 4);
	int day     = (value.day    % 10) | (((value.day    / 10) &  3) << 4);
	int hour    = (value.hour   % 10) | (((value.hour   / 10) &  3) << 4);
	int minute  = (value.minute % 10) | (((value.minute / 10) &  7) << 4);
	int second  = (value.second % 10) | (((value.second / 10) &  7) << 4);

	SYS573_RTC_CTRL |= SYS573_RTC_CTRL_WRITE;

	SYS573_RTC_SECOND  = second
		| (stop ? SYS573_RTC_SECOND_STOP : 0);
	SYS573_RTC_MINUTE  = minute;
	SYS573_RTC_HOUR    = hour;
	SYS573_RTC_WEEKDAY = weekday
		| SYS573_RTC_WEEKDAY_CENTURY
		| SYS573_RTC_WEEKDAY_CENTURY_ENABLE;
	SYS573_RTC_DAY     = day
		| SYS573_RTC_DAY_BATTERY_MONITOR;
	SYS573_RTC_MONTH   = month;
	SYS573_RTC_YEAR    = year;

	SYS573_RTC_CTRL &= ~SYS573_RTC_CTRL_WRITE;
}

bool isRTCBatteryLow(void) {
	SYS573_RTC_DAY |= SYS573_RTC_DAY_BATTERY_MONITOR;

	return (SYS573_RTC_DAY / SYS573_RTC_DAY_LOW_BATTERY) & 1;
}

/* Digital I/O board driver */

enum BitstreamTagType : uint8_t {
	_TAG_SOURCE_FILE = 'a',
	_TAG_PART_NAME   = 'b',
	_TAG_BUILD_DATE  = 'c',
	_TAG_BUILD_TIME  = 'd',
	_TAG_DATA        = 'e'
};

static void _writeBitstreamLSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++);

		for (int i = 8; i; i--, bits >>= 1)
			SYS573D_CPLD_BITSTREAM = (bits & 1) << 15;
	}
}

static void _writeBitstreamMSB(const uint8_t *data, size_t length) {
	for (; length; length--) {
		uint16_t bits = *(data++) << 8;

		for (int i = 8; i; i--, bits <<= 1)
			SYS573D_CPLD_BITSTREAM = bits & (1 << 15);
	}
}

bool loadBitstream(const uint8_t *data, size_t length) {
	// Konami's bitstreams are always stored LSB-first and with no headers,
	// however Xilinx tools export .bit files which contain MSB-first bitstreams
	// wrapped in a TLV container. In order to upload the bitstream properly,
	// the bit order and presence of a header must be autodetected. See
	// https://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm and
	// the "Data Stream Format" section in the XCS40XL datasheet for details.
	if (data[0] == 0xff)
		return loadRawBitstream(data, length);

	auto     dataEnd      = &data[length];
	uint16_t headerLength = (data[0] << 8) | data[1];

	data += headerLength + 4;

	while (data < dataEnd) {
		size_t tagLength;

		switch (data[0]) {
			case _TAG_DATA:
				tagLength = 0
					| (data[1] << 24)
					| (data[2] << 16)
					| (data[3] <<  8)
					| data[4];
				data     += 5;

				return loadRawBitstream(data, tagLength);

			default:
				tagLength = (data[1] << 8) | data[2];
				data     += 3;
		}

		data += tagLength;
	}

	return false;
}

bool loadRawBitstream(const uint8_t *data, size_t length) {
	if (data[0] != 0xff)
		return false;

	uint8_t id1 = data[1], id2 = data[4];
	void    (*writeFunc)(const uint8_t *, size_t);

	if (((id1 & 0xf0) == 0x20) && ((id2 & 0x0f) == 0x0f))
		writeFunc = &_writeBitstreamMSB;
	else if (((id1 & 0x0f) == 0x04) && ((id2 & 0xf0) == 0xf0))
		writeFunc = &_writeBitstreamLSB;
	else
		return false;

	for (int i = 3; i; i--) {
		SYS573D_CPLD_UNK_RESET = 0;

		SYS573D_CPLD_CTRL = SYS573D_CPLD_CTRL_UNKNOWN;
		SYS573D_CPLD_CTRL = 0
			| SYS573D_CPLD_CTRL_PROGRAM
			| SYS573D_CPLD_CTRL_UNKNOWN;
		SYS573D_CPLD_CTRL = 0
			| SYS573D_CPLD_CTRL_INIT
			| SYS573D_CPLD_CTRL_DONE
			| SYS573D_CPLD_CTRL_PROGRAM
			| SYS573D_CPLD_CTRL_UNKNOWN;

		delayMicroseconds(_RESET_DELAY);

		if (!(SYS573D_CPLD_STAT & SYS573D_CPLD_STAT_INIT))
			continue;

		writeFunc(data, length);

		const uint16_t mask = SYS573D_CPLD_STAT_INIT | SYS573D_CPLD_STAT_DONE;

		for (int j = 15; j; j--) {
			if ((SYS573D_CPLD_STAT & mask) == mask)
				return true;

			delayMicroseconds(_FPGA_INIT_DELAY);
		}
	}

	return false;
}

void initKonamiBitstream(void) {
	SYS573D_FPGA_INIT = 0xf000;
	SYS573D_FPGA_INIT = 0x0000;
	delayMicroseconds(_FPGA_INIT_DELAY);

	SYS573D_FPGA_INIT = 0xf000;
	delayMicroseconds(_FPGA_INIT_DELAY);

	// Turn off all lights including the ones that were left on by init().
	SYS573D_FPGA_LIGHTS_A0 = 0xf000;
	SYS573D_FPGA_LIGHTS_A1 = 0xf000;
	SYS573D_CPLD_LIGHTS_B0 = 0xf000;
	SYS573D_FPGA_LIGHTS_B1 = 0xf000;
	SYS573D_CPLD_LIGHTS_C0 = 0xf000;
	SYS573D_CPLD_LIGHTS_C1 = 0xf000;
	SYS573D_FPGA_LIGHTS_D0 = 0xf000;
}

/* I2C driver */

static constexpr int _I2C_BUS_DELAY   = 50;
static constexpr int _I2C_RESET_DELAY = 500;

// SDA is open-drain so it is toggled by changing pin direction.
#define _SDA(value)   setCartSDADir(!(value))
#define SDA(value)    _SDA(value), delayMicroseconds(_I2C_BUS_DELAY)
#define _SCL(value)   setCartOutput(OUT_SCL, value)
#define SCL(value)    _SCL(value), delayMicroseconds(_I2C_BUS_DELAY)
#define _CS(value)    setCartOutput(OUT_CS, value)
#define CS(value)     _CS(value), delayMicroseconds(_I2C_BUS_DELAY)
#define _RESET(value) setCartOutput(OUT_RESET, value)
#define RESET(value)  _RESET(value), delayMicroseconds(_I2C_BUS_DELAY)

void i2cStart(void) {
	_SDA(true);
	SCL(true);

	SDA(false); // START: SDA falling, SCL high
	SCL(false);
}

void i2cStartWithCS(int csDelay) {
	_SDA(true);
	_SCL(false);
	CS(true);

	CS(false);
	delayMicroseconds(csDelay);
	SCL(true);

	SDA(false); // START: SDA falling, SCL high
	SCL(false);
}

void i2cStop(void) {
	_SDA(false);
	SCL(true);

	SDA(true); // STOP: SDA rising, SCL high
}

void i2cStopWithCS(int csDelay) {
	_SDA(false);
	SCL(true);

	SDA(true); // STOP: SDA rising, SCL high

	SCL(false);
	delayMicroseconds(csDelay);
	CS(true);
}

uint8_t i2cReadByte(void) {
	uint8_t value = 0;

	for (int bit = 7; bit >= 0; bit--) { // MSB first
		SCL(true);
		if (getCartSDA())
			value |= (1 << bit);
		SCL(false);
	}

	delayMicroseconds(_I2C_BUS_DELAY);
	return value;
}

void i2cWriteByte(uint8_t value) {
	for (int bit = 7; bit >= 0; bit--) { // MSB first
		_SDA(value & (1 << bit));
		SCL(true);
		SCL(false);
	}

	SDA(true);
}

void i2cSendACK(bool ack) {
	_SDA(!ack);
	SCL(true);
	SCL(false);
	SDA(true);
}

bool i2cGetACK(void) {
	delayMicroseconds(_I2C_BUS_DELAY); // Required for ZS01
	SCL(true);
	bool ack = !getCartSDA();
	SCL(false);

	delayMicroseconds(_I2C_BUS_DELAY);
	return ack;
}

void i2cReadBytes(uint8_t *data, size_t length) {
	for (; length; length--) {
		*(data++) = i2cReadByte();

		if (length > 1)
			i2cSendACK(true);
	}
}

bool i2cWriteBytes(const uint8_t *data, size_t length, int lastACKDelay) {
	for (; length; length--) {
		i2cWriteByte(*(data++));

		if (length == 1)
			delayMicroseconds(lastACKDelay);
		if (!i2cGetACK())
			return false;
	}

	return true;
}

uint32_t i2cResetX76(void) {
	uint32_t value = 0;

	_SDA(true);
	_SCL(false);
	_CS(false);
	_RESET(false);

	RESET(true);
	SCL(true);
	SCL(false);
	RESET(false);
	delayMicroseconds(_I2C_RESET_DELAY);

	for (int bit = 0; bit < 32; bit++) { // LSB first
		SCL(true);
		if (getCartSDA())
			value |= (1 << bit);
		SCL(false);
	}

	CS(true);
	SCL(true);
	return value;
}

// For whatever reason the ZS01 does not implement the exact same response to
// reset protocol as the X76 chips. The reset pin is also active-low rather
// than active-high, and CS is ignored.
uint32_t i2cResetZS01(void) {
	uint32_t value = 0;

	_SDA(true);
	_SCL(false);
	_CS(false);
	_RESET(true);

	RESET(false);
	RESET(true);
	delayMicroseconds(_I2C_RESET_DELAY);

	SCL(true);
	SCL(false);

	for (int bit = 31; bit >= 0; bit--) { // MSB first
		if (getCartSDA())
			value |= (1 << bit);
		SCL(true);
		SCL(false);
	}

	SCL(true);
	return value;
}

/* 1-wire driver */

static constexpr int _DS_RESET_LOW_TIME     = 480;
static constexpr int _DS_RESET_SAMPLE_DELAY = 70;
static constexpr int _DS_RESET_DELAY        = 410;

static constexpr int _DS_READ_LOW_TIME     = 3;
static constexpr int _DS_READ_SAMPLE_DELAY = 10;
static constexpr int _DS_READ_DELAY        = 53;

static constexpr int _DS_ZERO_LOW_TIME  = 65;
static constexpr int _DS_ZERO_HIGH_TIME = 5;
static constexpr int _DS_ONE_LOW_TIME   = 10;
static constexpr int _DS_ONE_HIGH_TIME  = 55;

#define _CART1WIRE(value) setCartOutput(OUT_1WIRE, !(value))
#define _DIO1WIRE(value)  setDIO1Wire(value)

bool dsCartReset(void) {
	_CART1WIRE(false);
	delayMicroseconds(_DS_RESET_LOW_TIME);
	_CART1WIRE(true);

	delayMicroseconds(_DS_RESET_SAMPLE_DELAY);
	bool present = !getCartInput(IN_1WIRE);
	delayMicroseconds(_DS_RESET_DELAY);

	return present;
}

bool dsDIOReset(void) {
	_DIO1WIRE(false);
	delayMicroseconds(_DS_RESET_LOW_TIME);
	_DIO1WIRE(true);

	delayMicroseconds(_DS_RESET_SAMPLE_DELAY);
	bool present = !getDIO1Wire();
	delayMicroseconds(_DS_RESET_DELAY);

	return present;
}

uint8_t dsCartReadByte(void) {
	uint8_t value = 0;

	for (int bit = 0; bit < 8; bit++) { // LSB first
		_CART1WIRE(false);
		delayMicroseconds(_DS_READ_LOW_TIME);
		_CART1WIRE(true);
		delayMicroseconds(_DS_READ_SAMPLE_DELAY);
		if (getCartInput(IN_1WIRE))
			value |= (1 << bit);
		delayMicroseconds(_DS_READ_DELAY);
	}

	return value;
}

uint8_t dsDIOReadByte(void) {
	uint8_t value = 0;

	for (int bit = 0; bit < 8; bit++) { // LSB first
		_DIO1WIRE(false);
		delayMicroseconds(_DS_READ_LOW_TIME);
		_DIO1WIRE(true);
		delayMicroseconds(_DS_READ_SAMPLE_DELAY);
		if (getDIO1Wire())
			value |= (1 << bit);
		delayMicroseconds(_DS_READ_DELAY);
	}

	return value;
}

void dsCartWriteByte(uint8_t value) {
	for (int bit = 0; bit < 8; bit++) { // LSB first
		if (value & (1 << bit)) {
			_CART1WIRE(false);
			delayMicroseconds(_DS_ONE_LOW_TIME);
			_CART1WIRE(true);
			delayMicroseconds(_DS_ONE_HIGH_TIME);
		} else {
			_CART1WIRE(false);
			delayMicroseconds(_DS_ZERO_LOW_TIME);
			_CART1WIRE(true);
			delayMicroseconds(_DS_ZERO_HIGH_TIME);
		}
	}
}

void dsDIOWriteByte(uint8_t value) {
	for (int bit = 0; bit < 8; bit++) { // LSB first
		if (value & (1 << bit)) {
			_DIO1WIRE(false);
			delayMicroseconds(_DS_ONE_LOW_TIME);
			_DIO1WIRE(true);
			delayMicroseconds(_DS_ONE_HIGH_TIME);
		} else {
			_DIO1WIRE(false);
			delayMicroseconds(_DS_ZERO_LOW_TIME);
			_DIO1WIRE(true);
			delayMicroseconds(_DS_ZERO_HIGH_TIME);
		}
	}
}

}
