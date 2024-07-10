
#include <stddef.h>
#include <stdint.h>
#include "common/io.hpp"
#include "common/util.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace io {

uint16_t        _bankSwitchReg, _cartOutputReg, _miscOutputReg;
static uint16_t _digitalIOI2CReg, _digitalIODSBusReg;

/* System initialization */

static constexpr int _IDE_RESET_ASSERT_DELAY = 5000;
static constexpr int _IDE_RESET_CLEAR_DELAY  = 50000;

static constexpr int _FPGA_RESET_DELAY    = 5000;
static constexpr int _FPGA_STARTUP_DELAY  = 50000;
static constexpr int _FPGA_INIT_REG_DELAY = 5000;

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

#if 0
	// Revision D of the main board has footprints for either eight 8-bit RAM
	// chips wired as two 32-bit banks, or two 16-bit chips wired as a single
	// bank.
	DRAM_CTRL = isDualBankRAM() ? 0x0c80 : 0x4788;
#endif

	_bankSwitchReg = 0;
	_cartOutputReg = 0;
	_miscOutputReg = 0
		| SYS573_MISC_OUT_ADC_DI
		| SYS573_MISC_OUT_ADC_CS
		| SYS573_MISC_OUT_ADC_CLK
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
		SYS573D_CPLD_LIGHTS_BL = 0xf000;
		SYS573D_CPLD_LIGHTS_CL = 0xf000;
		SYS573D_CPLD_LIGHTS_CH = 0xf000;
	} else {
		SYS573A_LIGHTS_A = 0x00ff;
		SYS573A_LIGHTS_B = 0x00ff;
		SYS573A_LIGHTS_C = 0x00ff;
		SYS573A_LIGHTS_D = 0x00ff;
	}
}

void resetIDEDevices(void) {
	SYS573_IDE_RESET = 0;
	delayMicroseconds(_IDE_RESET_ASSERT_DELAY);

	SYS573_IDE_RESET = 1;
	delayMicroseconds(_IDE_RESET_CLEAR_DELAY);
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

bool isDigitalIOPresent(void) {
	const uint16_t mask = SYS573D_CPLD_STAT_ID1 | SYS573D_CPLD_STAT_ID2;

	return ((SYS573D_CPLD_STAT & mask) == SYS573D_CPLD_STAT_ID2);
}

bool loadDigitalIOBitstream(const uint8_t *data, size_t length) {
	// Konami's bitstreams are always stored LSB-first and with no headers,
	// however Xilinx tools export .bit files which contain MSB-first bitstreams
	// wrapped in a TLV container. In order to upload the bitstream properly,
	// the bit order and presence of a header must be autodetected. See
	// https://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm and
	// the "Data Stream Format" section in the XCS40XL datasheet for details.
	if (data[0] == 0xff)
		return loadDigitalIORawBitstream(data, length);

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

				return loadDigitalIORawBitstream(data, tagLength);

			default:
				tagLength = (data[1] << 8) | data[2];
				data     += 3;
		}

		data += tagLength;
	}

	return false;
}

bool loadDigitalIORawBitstream(const uint8_t *data, size_t length) {
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

	const uint16_t mask = SYS573D_CPLD_STAT_INIT | SYS573D_CPLD_STAT_DONE;

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
		delayMicroseconds(_FPGA_RESET_DELAY);

		if ((SYS573D_CPLD_STAT & mask) != SYS573D_CPLD_STAT_INIT)
			continue;

		writeFunc(data, length);
		delayMicroseconds(_FPGA_STARTUP_DELAY);

		if ((SYS573D_CPLD_STAT & mask) != mask)
			continue;

		return true;
	}

	return false;
}

void initDigitalIOFPGA(void) {
	SYS573D_FPGA_INIT = 0xf000;
	SYS573D_FPGA_INIT = 0x0000;
	delayMicroseconds(_FPGA_INIT_REG_DELAY);

	SYS573D_FPGA_INIT = 0xf000;
	delayMicroseconds(_FPGA_INIT_REG_DELAY);

	// Turn off all lights including the ones that were left on by init().
	SYS573D_FPGA_LIGHTS_AL = 0xf000;
	SYS573D_FPGA_LIGHTS_AH = 0xf000;
	SYS573D_CPLD_LIGHTS_BL = 0xf000;
	SYS573D_FPGA_LIGHTS_BH = 0xf000;
	SYS573D_CPLD_LIGHTS_CL = 0xf000;
	SYS573D_CPLD_LIGHTS_CH = 0xf000;
	SYS573D_FPGA_LIGHTS_D  = 0xf000;

	_digitalIOI2CReg   = 0
		| SYS573D_FPGA_MP3_I2C_SDA
		| SYS573D_FPGA_MP3_I2C_SCL;
	_digitalIODSBusReg = 0
		| SYS573D_FPGA_DS_BUS_DS2401
		| SYS573D_FPGA_DS_BUS_DS2433;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
	SYS573D_FPGA_DS_BUS  = _digitalIODSBusReg;
}

/* I2C driver */

static constexpr int _I2C_BUS_DELAY   = 50;
static constexpr int _I2C_RESET_DELAY = 500;

void I2CDriver::start(void) const {
	_setSDA(true);
	_setSCL(true, _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::startWithCS(int csDelay) const {
	_setSDA(true);
	_setSCL(false);
	_setCS(true, _I2C_BUS_DELAY);

	_setCS(false, _I2C_BUS_DELAY + csDelay);
	_setSCL(true, _I2C_BUS_DELAY);

	_setSDA(false, _I2C_BUS_DELAY); // START: SDA falling, SCL high
	_setSCL(false, _I2C_BUS_DELAY);
}

void I2CDriver::stop(void) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high
}

void I2CDriver::stopWithCS(int csDelay) const {
	_setSDA(false);

	_setSCL(true, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY); // STOP: SDA rising, SCL high

	_setSCL(false, _I2C_BUS_DELAY + csDelay);
	_setCS(true, _I2C_BUS_DELAY);
}

bool I2CDriver::getACK(void) const {
	delayMicroseconds(_I2C_BUS_DELAY); // Required for ZS01

	_setSCL(true, _I2C_BUS_DELAY);
	bool ack = _getSDA();
	_setSCL(false, _I2C_BUS_DELAY * 2);

	return ack ^ 1;
}

void I2CDriver::sendACK(bool ack) const {
	_setSDA(ack ^ 1);
	_setSCL(true, _I2C_BUS_DELAY);
	_setSCL(false, _I2C_BUS_DELAY);
	_setSDA(true, _I2C_BUS_DELAY);
}

uint8_t I2CDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 7; i >= 0; i--) { // MSB first
		_setSCL(true, _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	delayMicroseconds(_I2C_BUS_DELAY);
	return value;
}

void I2CDriver::writeByte(uint8_t value) const {
	for (int i = 7; i >= 0; i--) { // MSB first
		_setSDA((value >> i) & 1);
		_setSCL(true,  _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSDA(true, _I2C_BUS_DELAY);
}

void I2CDriver::readBytes(uint8_t *data, size_t length) const {
	for (; length; length--) {
		*(data++) = readByte();

		if (length > 1)
			sendACK(true);
	}
}

bool I2CDriver::writeBytes(
	const uint8_t *data, size_t length, int lastACKDelay
) const {
	for (; length; length--) {
		writeByte(*(data++));

		if (length == 1)
			delayMicroseconds(lastACKDelay);
		if (!getACK())
			return false;
	}

	return true;
}

uint32_t I2CDriver::resetX76(void) const {
	uint32_t value = 0;

	_setSDA(true);
	_setSCL(false);
	_setCS(false);
	_setReset(false);

	_setReset(true, _I2C_RESET_DELAY);
	_setSCL(true, _I2C_BUS_DELAY);
	_setSCL(false, _I2C_BUS_DELAY);
	_setReset(false, _I2C_RESET_DELAY);

	for (int i = 0; i < 32; i++) { // LSB first
		_setSCL(true, _I2C_BUS_DELAY);
		value |= _getSDA() << i;
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setCS(true, _I2C_BUS_DELAY);
	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

// For whatever reason the ZS01 does not implement the exact same response to
// reset protocol as the X76 chips. The reset pin is also active-low rather
// than active-high, and CS is ignored.
uint32_t I2CDriver::resetZS01(void) const {
	uint32_t value = 0;

	_setSDA(true);
	_setSCL(false);
	_setCS(false);
	_setReset(true);

	_setReset(false, _I2C_RESET_DELAY);
	_setReset(true, _I2C_RESET_DELAY);
	_setSCL(true, _I2C_BUS_DELAY);
	_setSCL(false, _I2C_BUS_DELAY);

	for (int i = 31; i >= 0; i--) { // MSB first
		value |= _getSDA() << i;
		_setSCL(true, _I2C_BUS_DELAY);
		_setSCL(false, _I2C_BUS_DELAY);
	}

	_setSCL(true, _I2C_BUS_DELAY);
	return value;
}

bool CartI2CDriver::_getSDA(void) const {
	return (SYS573_MISC_IN / SYS573_MISC_IN_CART_SDA) & 1;
}

void CartI2CDriver::_setSDA(bool value) const {
	// SDA is open-drain so it is toggled by tristating the pin.
	setCartOutput(CART_OUTPUT_SDA, false);
	setCartSDADirection(value ^ 1);
}

void CartI2CDriver::_setSCL(bool value) const {
	setCartOutput(CART_OUTPUT_SCL, value);
}

void CartI2CDriver::_setCS(bool value) const {
	setCartOutput(CART_OUTPUT_CS, value);
}

void CartI2CDriver::_setReset(bool value) const {
	setCartOutput(CART_OUTPUT_RESET, value);
}

bool DigitalIOI2CDriver::_getSDA(void) const {
	return (SYS573D_FPGA_MP3_I2C / SYS573D_FPGA_MP3_I2C_SDA) & 1;
}

void DigitalIOI2CDriver::_setSDA(bool value) const {
	if (value)
		_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SDA;
	else
		_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SCL;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
}

void DigitalIOI2CDriver::_setSCL(bool value) const {
	if (value)
		_digitalIOI2CReg |= SYS573D_FPGA_MP3_I2C_SDA;
	else
		_digitalIOI2CReg &= ~SYS573D_FPGA_MP3_I2C_SCL;

	SYS573D_FPGA_MP3_I2C = _digitalIOI2CReg;
}

const CartI2CDriver      cartI2C;
const DigitalIOI2CDriver digitalIOI2C;

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

bool OneWireDriver::reset(void) const {
	_set(false, _DS_RESET_LOW_TIME);
	_set(true, _DS_RESET_SAMPLE_DELAY);
	bool present = _get();

	delayMicroseconds(_DS_RESET_DELAY);
	return present ^ 1;
}

uint8_t OneWireDriver::readByte(void) const {
	uint8_t value = 0;

	for (int i = 0; i < 8; i++) { // LSB first
		_set(false, _DS_READ_LOW_TIME);
		_set(true, _DS_READ_SAMPLE_DELAY);
		value |= _get() << i;
		delayMicroseconds(_DS_READ_DELAY);
	}

	return value;
}

void OneWireDriver::writeByte(uint8_t value) const {
	for (int i = 8; i; i--, value >>= 1) { // LSB first
		if (value & 1) {
			_set(false, _DS_ONE_LOW_TIME);
			_set(true, _DS_ONE_HIGH_TIME);
		} else {
			_set(false, _DS_ZERO_LOW_TIME);
			_set(true, _DS_ZERO_HIGH_TIME);
		}
	}
}

bool CartDS2401Driver::_get(void) const {
	return getCartInput(CART_INPUT_DS2401);
}

void CartDS2401Driver::_set(bool value) const {
	setCartOutput(CART_OUTPUT_DS2401, value ^ 1);
}

bool DigitalIODS2401Driver::_get(void) const {
	return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2401) & 1;
}

void DigitalIODS2401Driver::_set(bool value) const {
	if (value)
		_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2401;
	else
		_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2401;

	SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
}

bool DigitalIODS2433Driver::_get(void) const {
	return (SYS573D_FPGA_DS_BUS / SYS573D_FPGA_DS_BUS_DS2433) & 1;
}

void DigitalIODS2433Driver::_set(bool value) const {
	if (value)
		_digitalIODSBusReg &= ~SYS573D_FPGA_DS_BUS_DS2433;
	else
		_digitalIODSBusReg |= SYS573D_FPGA_DS_BUS_DS2433;

	SYS573D_FPGA_DS_BUS = _digitalIODSBusReg;
}

const CartDS2401Driver      cartDS2401;
const DigitalIODS2401Driver digitalIODS2401;
const DigitalIODS2433Driver digitalIODS2433;

}
