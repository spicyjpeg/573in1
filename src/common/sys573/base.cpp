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
#include "common/sys573/base.hpp"
#include "common/util/misc.hpp"
#include "common/util/templates.hpp"
#include "common/bus.hpp"
#include "ps1/registers.h"
#include "ps1/registers573.h"
#include "ps1/system.h"

namespace sys573 {

uint16_t _bankSwitchReg, _cartOutputReg, _miscOutputReg;

/* System bus APIs */

static constexpr int _IDE_RESET_ASSERT_DELAY = 5000;
static constexpr int _IDE_RESET_CLEAR_DELAY  = 50000;

static constexpr int _DMA_TIMEOUT = 100000;

void init(void) {
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
	DMA_DPCR     |= DMA_DPCR_CH_ENABLE(DMA_PIO);

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

void resetIDEDevices(void) {
	SYS573_IDE_RESET = 0;
	delayMicroseconds(_IDE_RESET_ASSERT_DELAY);

	SYS573_IDE_RESET = 1;
	delayMicroseconds(_IDE_RESET_CLEAR_DELAY);
}

size_t doDMARead(
	volatile void *source,
	void          *data,
	size_t        length,
	bool          wait
) {
	length = (length + 3) / 4;

	util::assertAligned<uint32_t>(data);

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT))
		return 0;

	// The BIU will output the base address set through this register over the
	// address lines during a DMA transfer. This does not affect non-DMA access
	// as the BIU will realign the address by masking off the bottommost N bits
	// (where N is the number of address lines used) and replace them with the
	// respective CPU address bits.
	BIU_DEV0_ADDR = uint32_t(source) & 0x1fffffff;

	DMA_MADR(DMA_PIO) = uint32_t(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_READ
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (wait)
		waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);

	return length * 4;
}

size_t doDMAWrite(
	volatile void *dest,
	const void    *data,
	size_t        length,
	bool          wait
) {
	length = (length + 3) / 4;

	util::assertAligned<uint32_t>(data);

	if (!waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT))
		return 0;

	BIU_DEV0_ADDR = uint32_t(dest) & 0x1fffffff;

	DMA_MADR(DMA_PIO) = uint32_t(data);
	DMA_BCR (DMA_PIO) = length;
	DMA_CHCR(DMA_PIO) = 0
		| DMA_CHCR_WRITE
		| DMA_CHCR_MODE_BURST
		| DMA_CHCR_ENABLE
		| DMA_CHCR_TRIGGER;

	if (wait)
		waitForDMATransfer(DMA_PIO, _DMA_TIMEOUT);

	return length * 4;
}

/* JAMMA, ADC and RTC functions */

enum ADCCommandFlag : uint8_t {
	_ADC_CMD_START_BIT = 1 << 0,
	_ADC_CMD_SGL_DIF   = 1 << 1,
	_ADC_CMD_ODD_SIGN  = 1 << 2,
	_ADC_CMD_SELECT    = 1 << 3
};

static constexpr int _ADC_START_DELAY = 20;
static constexpr int _ADC_BUS_DELAY   = 20;

JAMMAInputMask getJAMMAInputs(void) {
	JAMMAInputMask inputs;

	inputs  =  SYS573_JAMMA_MAIN;
	inputs |= (SYS573_JAMMA_EXT1 & 0x0f00) <<  8;
	inputs |= (SYS573_JAMMA_EXT2 & 0x0f00) << 12;
	inputs |= (SYS573_MISC_IN2   & 0x1f00) << 16;

	return inputs ^ 0x1fffffff;
}

uint8_t getAnalogInput(AnalogInput pin) {
	setMiscOutput(MISC_OUT_ADC_CLK, false);
	setMiscOutput(MISC_OUT_ADC_CS,  true);

	setMiscOutput(MISC_OUT_ADC_CS, false);
	delayMicroseconds(_ADC_START_DELAY);

	uint8_t cmd = 0
		| _ADC_CMD_START_BIT
		| ((pin & 3) << 2);

	if (pin < ANALOG_IN_CH0P_CH1N)
		cmd |= _ADC_CMD_SGL_DIF;

	for (int i = 5; i; i--) { // LSB first
		setMiscOutput(MISC_OUT_ADC_DI, cmd & 1);
		cmd >>= 1;

		delayMicroseconds(_ADC_BUS_DELAY);
		setMiscOutput(MISC_OUT_ADC_CLK, true);
		delayMicroseconds(_ADC_BUS_DELAY);
		setMiscOutput(MISC_OUT_ADC_CLK, false);
	}

	uint8_t value = 0;

	for (int i = 8; i > 0; i--) { // MSB first
		delayMicroseconds(_ADC_BUS_DELAY);
		setMiscOutput(MISC_OUT_ADC_CLK, true);

		value <<= 1;
		value  |= (SYS573_MISC_IN2 / SYS573_MISC_IN2_ADC_DO) & 1;

		delayMicroseconds(_ADC_BUS_DELAY);
		setMiscOutput(MISC_OUT_ADC_CLK, false);
	}

	setMiscOutput(MISC_OUT_ADC_CS, false);
	delayMicroseconds(_ADC_BUS_DELAY);

	return value;
}

void getRTCTime(util::Date &output) {
	uint16_t ctrlReg = SYS573_RTC_CTRL & ~SYS573_RTC_CTRL_READ;
	SYS573_RTC_CTRL  = ctrlReg | SYS573_RTC_CTRL_READ;

	auto second = SYS573_RTC_SECOND, minute = SYS573_RTC_MINUTE;
	auto hour   = SYS573_RTC_HOUR,   day    = SYS573_RTC_DAY;
	auto month  = SYS573_RTC_MONTH,  year   = SYS573_RTC_YEAR;

	SYS573_RTC_CTRL = ctrlReg;

	output.year   = util::decodeBCD(year);   // 0-99
	output.month  = util::decodeBCD(month);  // 1-12
	output.day    = util::decodeBCD(day);    // 1-31
	output.hour   = util::decodeBCD(hour);   // 0-23
	output.minute = util::decodeBCD(minute); // 0-59
	output.second = util::decodeBCD(second); // 0-59

	output.year += (output.year < 70) ? 2000 : 1900;
}

void setRTCTime(const util::Date &value, bool stop) {
	assert((value.year >= 1970) && (value.year <= 2069));

	int weekday = value.getDayOfWeek() + 1;
	int year    = util::encodeBCD(value.year % 100);
	int month   = util::encodeBCD(value.month);
	int day     = util::encodeBCD(value.day);
	int hour    = util::encodeBCD(value.hour);
	int minute  = util::encodeBCD(value.minute);
	int second  = util::encodeBCD(value.second);

	uint16_t ctrlReg = SYS573_RTC_CTRL & ~SYS573_RTC_CTRL_WRITE;
	SYS573_RTC_CTRL  = ctrlReg | SYS573_RTC_CTRL_WRITE;

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

	SYS573_RTC_CTRL = ctrlReg;
}

bool isRTCBatteryLow(void) {
	SYS573_RTC_DAY |= SYS573_RTC_DAY_BATTERY_MONITOR;

	return (SYS573_RTC_DAY / SYS573_RTC_DAY_LOW_BATTERY) & 1;
}

/* Security cartridge bus drivers */

bool CartI2CDriver::_getSDA(void) const {
	return (SYS573_MISC_IN2 / SYS573_MISC_IN2_CART_SDA) & 1;
}

void CartI2CDriver::_setSDA(bool value) const {
	// SDA is open-drain so it is toggled by tristating the pin.
	setCartOutput(CART_OUT_SDA, false);
	setCartSDADirection(value ^ 1);
}

void CartI2CDriver::_setSCL(bool value) const {
	setCartOutput(CART_OUT_SCL, value);
}

void CartI2CDriver::_setCS(bool value) const {
	setCartOutput(CART_OUT_CS, value);
}

void CartI2CDriver::_setReset(bool value) const {
	setCartOutput(CART_OUT_RESET, value);
}

bool CartDS2401Driver::_get(void) const {
	return getCartInput(CART_IN_DS2401);
}

void CartDS2401Driver::_set(bool value) const {
	setCartOutput(CART_OUT_DS2401, value ^ 1);
}

const bus::SIO1Driver  cartSerial;
const CartI2CDriver    cartI2C;
const CartDS2401Driver cartDS2401;

}
