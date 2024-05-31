/*
 * ps1-bare-metal - (C) 2023 spicyjpeg
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "ps1/registers.h"

/* System 573 base hardware */

typedef enum {
	SYS573_MISC_OUT_ADC_MOSI    = 1 << 0,
	SYS573_MISC_OUT_ADC_CS      = 1 << 1,
	SYS573_MISC_OUT_ADC_SCK     = 1 << 2,
	SYS573_MISC_OUT_COIN_COUNT1 = 1 << 3,
	SYS573_MISC_OUT_COIN_COUNT2 = 1 << 4,
	SYS573_MISC_OUT_AMP_ENABLE  = 1 << 5,
	SYS573_MISC_OUT_CDDA_ENABLE = 1 << 6,
	SYS573_MISC_OUT_SPU_ENABLE  = 1 << 7,
	SYS573_MISC_OUT_JVS_RESET   = 1 << 8
} Sys573MiscOutputFlag;

typedef enum {
	SYS573_MISC_IN_ADC_MISO   = 1 <<  0,
	SYS573_MISC_IN_ADC_SARS   = 1 <<  1,
	SYS573_MISC_IN_CART_SDA   = 1 <<  2,
	SYS573_MISC_IN_JVS_SENSE  = 1 <<  3,
	SYS573_MISC_IN_JVS_IRDY   = 1 <<  4,
	SYS573_MISC_IN_JVS_DRDY   = 1 <<  5,
	SYS573_MISC_IN_CART_IRDY  = 1 <<  6,
	SYS573_MISC_IN_CART_DRDY  = 1 <<  7,
	SYS573_MISC_IN_COIN1      = 1 <<  8,
	SYS573_MISC_IN_COIN2      = 1 <<  9,
	SYS573_MISC_IN_PCMCIA_CD1 = 1 << 10,
	SYS573_MISC_IN_PCMCIA_CD2 = 1 << 11,
	SYS573_MISC_IN_SERVICE    = 1 << 12
} Sys573MiscInputFlag;

typedef enum {
	SYS573_BANK_FLASH   =  0,
	SYS573_BANK_PCMCIA1 = 16,
	SYS573_BANK_PCMCIA2 = 32
} Sys573Bank;

#define SYS573_MISC_OUT     _MMIO16(DEV0_BASE | 0x400000)
#define SYS573_DIP_CART     _MMIO16(DEV0_BASE | 0x400004)
#define SYS573_MISC_IN      _MMIO16(DEV0_BASE | 0x400006)
#define SYS573_JAMMA_MAIN   _MMIO16(DEV0_BASE | 0x400008)
#define SYS573_JVS_RX_DATA  _MMIO16(DEV0_BASE | 0x40000a)
#define SYS573_JAMMA_EXT1   _MMIO16(DEV0_BASE | 0x40000c)
#define SYS573_JAMMA_EXT2   _MMIO16(DEV0_BASE | 0x40000e)
#define SYS573_BANK_CTRL    _MMIO16(DEV0_BASE | 0x500000)
#define SYS573_JVS_IRDY_ACK _MMIO16(DEV0_BASE | 0x520000)
#define SYS573_IDE_RESET    _MMIO16(DEV0_BASE | 0x560000)
#define SYS573_WATCHDOG     _MMIO16(DEV0_BASE | 0x5c0000)
#define SYS573_EXT_OUT      _MMIO16(DEV0_BASE | 0x600000)
#define SYS573_JVS_TX_DATA  _MMIO16(DEV0_BASE | 0x680000)
#define SYS573_CART_OUT     _MMIO16(DEV0_BASE | 0x6a0000)

#define SYS573_FLASH_BASE   _ADDR16(DEV0_BASE | 0x000000)
#define SYS573_IDE_CS0_BASE _ADDR16(DEV0_BASE | 0x480000)
#define SYS573_IDE_CS1_BASE _ADDR16(DEV0_BASE | 0x4c0000)
#define SYS573_RTC_BASE     _ADDR16(DEV0_BASE | 0x620000)
#define SYS573_IO_BASE      _ADDR16(DEV0_BASE | 0x640000)

/* System 573 RTC */

typedef enum {
	SYS573_RTC_CTRL_CAL_BITMASK  = 31 << 0,
	SYS573_RTC_CTRL_CAL_POSITIVE =  0 << 5,
	SYS573_RTC_CTRL_CAL_NEGATIVE =  1 << 5,
	SYS573_RTC_CTRL_READ         =  1 << 6,
	SYS573_RTC_CTRL_WRITE        =  1 << 7
} Sys573RTCControlFlag;

typedef enum {
	SYS573_RTC_SECOND_UNITS_BITMASK = 15 << 0,
	SYS573_RTC_SECOND_TENS_BITMASK  =  7 << 4,
	SYS573_RTC_SECOND_STOP          =  1 << 7
} Sys573RTCSecondFlag;

typedef enum {
	SYS573_RTC_WEEKDAY_UNITS_BITMASK  = 7 << 0,
	SYS573_RTC_WEEKDAY_CENTURY        = 1 << 4,
	SYS573_RTC_WEEKDAY_CENTURY_ENABLE = 1 << 5,
	SYS573_RTC_WEEKDAY_FREQUENCY_TEST = 1 << 6
} Sys573RTCWeekdayFlag;

typedef enum {
	SYS573_RTC_DAY_UNITS_BITMASK   = 15 << 0,
	SYS573_RTC_DAY_TENS_BITMASK    =  3 << 4,
	SYS573_RTC_DAY_LOW_BATTERY     =  1 << 6,
	SYS573_RTC_DAY_BATTERY_MONITOR =  1 << 7
} Sys573RTCDayFlag;

#define SYS573_RTC_CTRL    _MMIO16(DEV0_BASE | 0x623ff0)
#define SYS573_RTC_SECOND  _MMIO16(DEV0_BASE | 0x623ff2)
#define SYS573_RTC_MINUTE  _MMIO16(DEV0_BASE | 0x623ff4)
#define SYS573_RTC_HOUR    _MMIO16(DEV0_BASE | 0x623ff6)
#define SYS573_RTC_WEEKDAY _MMIO16(DEV0_BASE | 0x623ff8)
#define SYS573_RTC_DAY     _MMIO16(DEV0_BASE | 0x623ffa)
#define SYS573_RTC_MONTH   _MMIO16(DEV0_BASE | 0x623ffc)
#define SYS573_RTC_YEAR    _MMIO16(DEV0_BASE | 0x623ffe)

/* System 573 analog I/O board */

#define SYS573A_LIGHTS_A _MMIO16(DEV0_BASE | 0x640080)
#define SYS573A_LIGHTS_B _MMIO16(DEV0_BASE | 0x640088)
#define SYS573A_LIGHTS_C _MMIO16(DEV0_BASE | 0x640090)
#define SYS573A_LIGHTS_D _MMIO16(DEV0_BASE | 0x640098)

/* System 573 digital I/O board */

typedef enum {
	SYS573D_CPLD_STAT_INIT = 1 << 12,
	SYS573D_CPLD_STAT_DONE = 1 << 13,
	SYS573D_CPLD_STAT_ID1  = 1 << 14,
	SYS573D_CPLD_STAT_ID2  = 1 << 15
} Sys573DCPLDStatusFlag;

typedef enum {
	SYS573D_CPLD_CTRL_INIT    = 1 << 12,
	SYS573D_CPLD_CTRL_DONE    = 1 << 13,
	SYS573D_CPLD_CTRL_PROGRAM = 1 << 14,
	SYS573D_CPLD_CTRL_UNKNOWN = 1 << 15
} Sys573DCPLDControlFlag;

#define SYS573D_FPGA_MAGIC     _MMIO16(DEV0_BASE | 0x640080)
#define SYS573D_FPGA_LIGHTS_A1 _MMIO16(DEV0_BASE | 0x6400e0)
#define SYS573D_FPGA_LIGHTS_A0 _MMIO16(DEV0_BASE | 0x6400e2)
#define SYS573D_FPGA_LIGHTS_B1 _MMIO16(DEV0_BASE | 0x6400e4)
#define SYS573D_FPGA_LIGHTS_D0 _MMIO16(DEV0_BASE | 0x6400e6)
#define SYS573D_FPGA_INIT      _MMIO16(DEV0_BASE | 0x6400e8)
#define SYS573D_FPGA_DS2401    _MMIO16(DEV0_BASE | 0x6400ee)

#define SYS573D_CPLD_UNK_RESET _MMIO16(DEV0_BASE | 0x6400f4)
#define SYS573D_CPLD_STAT      _MMIO16(DEV0_BASE | 0x6400f6)
#define SYS573D_CPLD_CTRL      _MMIO16(DEV0_BASE | 0x6400f6)
#define SYS573D_CPLD_BITSTREAM _MMIO16(DEV0_BASE | 0x6400f8)
#define SYS573D_CPLD_LIGHTS_C0 _MMIO16(DEV0_BASE | 0x6400fa)
#define SYS573D_CPLD_LIGHTS_C1 _MMIO16(DEV0_BASE | 0x6400fc)
#define SYS573D_CPLD_LIGHTS_B0 _MMIO16(DEV0_BASE | 0x6400fe)
