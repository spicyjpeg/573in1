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

#include <stdint.h>

#define _ADDR8(addr)  ((volatile uint8_t *) (addr))
#define _ADDR16(addr) ((volatile uint16_t *) (addr))
#define _ADDR32(addr) ((volatile uint32_t *) (addr))
#define _MMIO8(addr)  (*_ADDR8(addr))
#define _MMIO16(addr) (*_ADDR16(addr))
#define _MMIO32(addr) (*_ADDR32(addr))

/* Constants */

#define F_CPU      33868800
#define F_GPU_NTSC 53693175
#define F_GPU_PAL  53203425

typedef enum {
	DEV0_BASE  = 0xbf000000,
	EXP1_BASE  = 0xbf000000,
	CACHE_BASE = 0xbf800000,
	IO_BASE    = 0xbf801000,
	EXP2_BASE  = 0xbf802000,
	EXP3_BASE  = 0xbfa00000,
	DEV2_BASE  = 0xbfc00000
} BaseAddress;

/* Bus interface */

typedef enum {
	BIU_CTRL_WRITE_DELAY_BITMASK = 15 <<  0,
	BIU_CTRL_READ_DELAY_BITMASK  = 15 <<  4,
	BIU_CTRL_RECOVERY            =  1 <<  8,
	BIU_CTRL_HOLD                =  1 <<  9,
	BIU_CTRL_FLOAT               =  1 << 10,
	BIU_CTRL_PRESTROBE           =  1 << 11,
	BIU_CTRL_WIDTH_8             =  0 << 12,
	BIU_CTRL_WIDTH_16            =  1 << 12,
	BIU_CTRL_AUTO_INCR           =  1 << 13,
	BIU_CTRL_SIZE_BITMASK        = 31 << 16,
	BIU_CTRL_DMA_DELAY_BITMASK   = 15 << 24,
	BIU_CTRL_ADDR_ERROR          =  1 << 28,
	BIU_CTRL_DMA_DELAY           =  1 << 29,
	BIU_CTRL_DMA32               =  1 << 30,
	BIU_CTRL_WAIT                =  1 << 31
} BIUControlFlag;

#define BIU_DEV0_ADDR _MMIO32(IO_BASE | 0x000) // PIO/573
#define BIU_EXP2_ADDR _MMIO32(IO_BASE | 0x004) // PIO/debug
#define BIU_DEV0_CTRL _MMIO32(IO_BASE | 0x008) // PIO/573
#define BIU_EXP3_CTRL _MMIO32(IO_BASE | 0x00c) // PIO/debug
#define BIU_DEV2_CTRL _MMIO32(IO_BASE | 0x010) // BIOS ROM
#define BIU_DEV4_CTRL _MMIO32(IO_BASE | 0x014) // SPU
#define BIU_DEV5_CTRL _MMIO32(IO_BASE | 0x018) // CD-ROM
#define BIU_EXP2_CTRL _MMIO32(IO_BASE | 0x01c) // PIO/debug
#define BIU_COM_DELAY _MMIO32(IO_BASE | 0x020)

/* Serial interfaces */

typedef enum {
	SIO_STAT_TX_NOT_FULL   = 1 << 0,
	SIO_STAT_RX_NOT_EMPTY  = 1 << 1,
	SIO_STAT_TX_EMPTY      = 1 << 2,
	SIO_STAT_RX_PARITY_ERR = 1 << 3,
	SIO_STAT_RX_OVERRUN    = 1 << 4, // SIO1 only
	SIO_STAT_RX_STOP_ERR   = 1 << 5, // SIO1 only
	SIO_STAT_RX_INVERT     = 1 << 6, // SIO1 only
	SIO_STAT_DSR           = 1 << 7, // DSR is /ACK on SIO0
	SIO_STAT_CTS           = 1 << 8, // SIO1 only
	SIO_STAT_IRQ           = 1 << 9
} SIOStatusFlag;

typedef enum {
	SIO_MODE_BAUD_BITMASK   = 3 << 0,
	SIO_MODE_BAUD_DIV1      = 1 << 0,
	SIO_MODE_BAUD_DIV16     = 2 << 0,
	SIO_MODE_BAUD_DIV64     = 3 << 0,
	SIO_MODE_DATA_BITMASK   = 3 << 2,
	SIO_MODE_DATA_5         = 0 << 2,
	SIO_MODE_DATA_6         = 1 << 2,
	SIO_MODE_DATA_7         = 2 << 2,
	SIO_MODE_DATA_8         = 3 << 2,
	SIO_MODE_PARITY_BITMASK = 3 << 4,
	SIO_MODE_PARITY_NONE    = 0 << 4,
	SIO_MODE_PARITY_EVEN    = 1 << 4,
	SIO_MODE_PARITY_ODD     = 3 << 4,
	SIO_MODE_STOP_BITMASK   = 3 << 6, // SIO1 only
	SIO_MODE_STOP_1         = 1 << 6, // SIO1 only
	SIO_MODE_STOP_1_5       = 2 << 6, // SIO1 only
	SIO_MODE_STOP_2         = 3 << 6, // SIO1 only
	SIO_MODE_SCK_INVERT     = 1 << 8  // SIO0 only
} SIOModeFlag;

typedef enum {
	SIO_CTRL_TX_ENABLE      = 1 <<  0,
	SIO_CTRL_DTR            = 1 <<  1, // DTR is /CS on SIO0
	SIO_CTRL_RX_ENABLE      = 1 <<  2,
	SIO_CTRL_TX_INVERT      = 1 <<  3, // SIO1 only
	SIO_CTRL_ACKNOWLEDGE    = 1 <<  4,
	SIO_CTRL_RTS            = 1 <<  5, // SIO1 only
	SIO_CTRL_RESET          = 1 <<  6,
	SIO_CTRL_TX_IRQ_ENABLE  = 1 << 10,
	SIO_CTRL_RX_IRQ_ENABLE  = 1 << 11,
	SIO_CTRL_DSR_IRQ_ENABLE = 1 << 12, // DSR is /ACK on SIO0
	SIO_CTRL_CS_PORT_1      = 0 << 13, // SIO0 only
	SIO_CTRL_CS_PORT_2      = 1 << 13  // SIO0 only
} SIOControlFlag;

// SIO_DATA is a 32-bit register, but some emulators do not implement it
// correctly and break if it's read more than 8 bits at a time.
#define SIO_DATA(N) _MMIO8 ((IO_BASE | 0x040) + (16 * (N)))
#define SIO_STAT(N) _MMIO16((IO_BASE | 0x044) + (16 * (N)))
#define SIO_MODE(N) _MMIO16((IO_BASE | 0x048) + (16 * (N)))
#define SIO_CTRL(N) _MMIO16((IO_BASE | 0x04a) + (16 * (N)))
#define SIO_BAUD(N) _MMIO16((IO_BASE | 0x04e) + (16 * (N)))

/* DRAM controller */

typedef enum {
	DRAM_CTRL_UNKNOWN     = 1 <<  3,
	DRAM_CTRL_FETCH_DELAY = 1 <<  7,
	DRAM_CTRL_SIZE_MUL1   = 0 <<  9,
	DRAM_CTRL_SIZE_MUL4   = 1 <<  9,
	DRAM_CTRL_COUNT_1     = 0 << 10, // 1 DRAM bank (single RAS)
	DRAM_CTRL_COUNT_2     = 1 << 10, // 2 DRAM banks (dual RAS)
	DRAM_CTRL_SIZE_1MB    = 0 << 11, // 1MB chips (4MB with MUL4)
	DRAM_CTRL_SIZE_2MB    = 1 << 11  // 2MB chips (8MB with MUL4)
} DRAMControlFlag;

#define DRAM_CTRL _MMIO32(IO_BASE | 0x060)

/* IRQ controller */

typedef enum {
	IRQ_VSYNC  =  0,
	IRQ_GPU    =  1,
	IRQ_CDROM  =  2,
	IRQ_DMA    =  3,
	IRQ_TIMER0 =  4,
	IRQ_TIMER1 =  5,
	IRQ_TIMER2 =  6,
	IRQ_SIO0   =  7,
	IRQ_SIO1   =  8,
	IRQ_SPU    =  9,
	IRQ_GUN    = 10,
	IRQ_PIO    = 10
} IRQChannel;

#define IRQ_STAT _MMIO16(IO_BASE | 0x070)
#define IRQ_MASK _MMIO16(IO_BASE | 0x074)

/* DMA */

typedef enum {
	DMA_MDEC_IN  = 0,
	DMA_MDEC_OUT = 1,
	DMA_GPU      = 2,
	DMA_CDROM    = 3,
	DMA_SPU      = 4,
	DMA_PIO      = 5,
	DMA_OTC      = 6
} DMAChannel;

typedef enum {
	DMA_CHCR_READ             = 0 <<  0,
	DMA_CHCR_WRITE            = 1 <<  0,
	DMA_CHCR_REVERSE          = 1 <<  1,
	DMA_CHCR_CHOPPING         = 1 <<  8,
	DMA_CHCR_MODE_BITMASK     = 3 <<  9,
	DMA_CHCR_MODE_BURST       = 0 <<  9,
	DMA_CHCR_MODE_SLICE       = 1 <<  9,
	DMA_CHCR_MODE_LIST        = 2 <<  9,
	DMA_CHCR_DMA_TIME_BITMASK = 7 << 16,
	DMA_CHCR_CPU_TIME_BITMASK = 7 << 20,
	DMA_CHCR_ENABLE           = 1 << 24,
	DMA_CHCR_TRIGGER          = 1 << 28,
	DMA_CHCR_PAUSE            = 1 << 29  // Burst mode only
} DMACHCRFlag;

typedef enum {
	DMA_DPCR_PRIORITY_BITMASK = 7 << 0,
	DMA_DPCR_PRIORITY_MIN     = 7 << 0,
	DMA_DPCR_PRIORITY_MAX     = 0 << 0,
	DMA_DPCR_ENABLE           = 1 << 3
} DMADPCRFlag;

typedef enum {
	DMA_DICR_CH_MODE_BITMASK   = 0x7f <<  0,
	DMA_DICR_BUS_ERROR         =    1 << 15,
	DMA_DICR_CH_ENABLE_BITMASK = 0x7f << 16,
	DMA_DICR_IRQ_ENABLE        =    1 << 23,
	DMA_DICR_CH_STAT_BITMASK   = 0x7f << 24,
	DMA_DICR_IRQ               =    1 << 31
} DMADICRFlag;

#define DMA_DICR_CH_MODE(dma)   (1 << ((dma) +  0))
#define DMA_DICR_CH_ENABLE(dma) (1 << ((dma) + 16))
#define DMA_DICR_CH_STAT(dma)   (1 << ((dma) + 24))

#define DMA_MADR(N) _MMIO32((IO_BASE | 0x080) + (16 * (N)))
#define DMA_BCR(N)  _MMIO32((IO_BASE | 0x084) + (16 * (N)))
#define DMA_CHCR(N) _MMIO32((IO_BASE | 0x088) + (16 * (N)))

#define DMA_DPCR _MMIO32(IO_BASE | 0x0f0)
#define DMA_DICR _MMIO32(IO_BASE | 0x0f4)

/* Timers */

typedef enum {
	TIMER_CTRL_ENABLE_SYNC     = 1 <<  0,
	TIMER_CTRL_SYNC_BITMASK    = 3 <<  1,
	TIMER_CTRL_SYNC_PAUSE      = 0 <<  1,
	TIMER_CTRL_SYNC_RESET1     = 1 <<  1,
	TIMER_CTRL_SYNC_RESET2     = 2 <<  1,
	TIMER_CTRL_SYNC_PAUSE_ONCE = 3 <<  1,
	TIMER_CTRL_RELOAD          = 1 <<  3,
	TIMER_CTRL_IRQ_ON_RELOAD   = 1 <<  4,
	TIMER_CTRL_IRQ_ON_OVERFLOW = 1 <<  5,
	TIMER_CTRL_IRQ_REPEAT      = 1 <<  6,
	TIMER_CTRL_IRQ_LATCH       = 1 <<  7,
	TIMER_CTRL_EXT_CLOCK       = 1 <<  8,
	TIMER_CTRL_PRESCALE        = 1 <<  9,
	TIMER_CTRL_IRQ             = 1 << 10,
	TIMER_CTRL_RELOADED        = 1 << 11,
	TIMER_CTRL_OVERFLOWED      = 1 << 12
} TimerControlFlag;

#define TIMER_VALUE(N)  _MMIO32((IO_BASE | 0x100) + (16 * (N)))
#define TIMER_CTRL(N)   _MMIO32((IO_BASE | 0x104) + (16 * (N)))
#define TIMER_RELOAD(N) _MMIO32((IO_BASE | 0x108) + (16 * (N)))

/* CD-ROM drive */

typedef enum {
	CDROM_STAT_BANK_BITMASK = 3 << 0,
	CDROM_STAT_BANK_0       = 0 << 0,
	CDROM_STAT_BANK_1       = 1 << 0,
	CDROM_STAT_BANK_2       = 2 << 0,
	CDROM_STAT_BANK_3       = 3 << 0,
	CDROM_STAT_ADPCM_BUSY   = 1 << 2,
	CDROM_STAT_PARAM_EMPTY  = 1 << 3,
	CDROM_STAT_PARAM_FULL   = 1 << 4,
	CDROM_STAT_RESP_EMPTY   = 1 << 5,
	CDROM_STAT_DATA_EMPTY   = 1 << 6,
	CDROM_STAT_BUSY         = 1 << 7
} CDROMStatusFlag;

typedef enum {
	CDROM_REQ_START_IRQ_ENABLE = 1 << 5,
	CDROM_REQ_BUFFER_WRITE     = 1 << 6,
	CDROM_REQ_BUFFER_READ      = 1 << 7
} CDROMRequestFlag;

typedef enum {
	CDROM_IRQ_NONE        = 0,
	CDROM_IRQ_DATA        = 1,
	CDROM_IRQ_COMPLETE    = 2,
	CDROM_IRQ_ACKNOWLEDGE = 3,
	CDROM_IRQ_DATA_END    = 4,
	CDROM_IRQ_ERROR       = 5
} CDROMIRQType;

typedef enum {
	CDROM_CMDSTAT_ERROR      = 1 << 0,
	CDROM_CMDSTAT_SPINDLE_ON = 1 << 1,
	CDROM_CMDSTAT_SEEK_ERROR = 1 << 2,
	CDROM_CMDSTAT_ID_ERROR   = 1 << 3,
	CDROM_CMDSTAT_LID_OPEN   = 1 << 4,
	CDROM_CMDSTAT_READING    = 1 << 5,
	CDROM_CMDSTAT_SEEKING    = 1 << 6,
	CDROM_CMDSTAT_PLAYING    = 1 << 7
} CDROMCommandStatusFlag;

typedef enum {
	CDROM_MODE_CDDA        = 1 << 0,
	CDROM_MODE_AUTO_PAUSE  = 1 << 1,
	CDROM_MODE_CDDA_REPORT = 1 << 2,
	CDROM_MODE_XA_FILTER   = 1 << 3,
	CDROM_MODE_IGNORE_LOC  = 1 << 4,
	CDROM_MODE_SIZE_2048   = 0 << 5,
	CDROM_MODE_SIZE_2340   = 1 << 5,
	CDROM_MODE_XA_ADPCM    = 1 << 6,
	CDROM_MODE_SPEED_1X    = 0 << 7,
	CDROM_MODE_SPEED_2X    = 1 << 7
} CDROMModeFlag;

#define CDROM_STAT _MMIO8(IO_BASE | 0x800)
#define CDROM_CMD  _MMIO8(IO_BASE | 0x801)
#define CDROM_DATA _MMIO8(IO_BASE | 0x802)
#define CDROM_IRQ  _MMIO8(IO_BASE | 0x803)

#define CDROM_REG(N) _MMIO8((IO_BASE | 0x800) + (N))

/* GPU */

typedef enum {
	GP1_STAT_MODE_BITMASK = 1 << 20,
	GP1_STAT_MODE_NTSC    = 0 << 20,
	GP1_STAT_MODE_PAL     = 1 << 20,
	GP1_STAT_DISP_BLANK   = 1 << 23,
	GP1_STAT_IRQ          = 1 << 24,
	GP1_STAT_DREQ         = 1 << 25,
	GP1_STAT_CMD_READY    = 1 << 26,
	GP1_STAT_READ_READY   = 1 << 27,
	GP1_STAT_WRITE_READY  = 1 << 28,
	GP1_STAT_FIELD_ODD    = 1 << 31
} GP1StatusFlag;

#define GPU_GP0 _MMIO32(IO_BASE | 0x810)
#define GPU_GP1 _MMIO32(IO_BASE | 0x814)

/* MDEC */

typedef enum {
	MDEC_STAT_BLOCK_BITMASK = 7 << 16,
	MDEC_STAT_BLOCK_Y0      = 0 << 16,
	MDEC_STAT_BLOCK_Y1      = 1 << 16,
	MDEC_STAT_BLOCK_Y2      = 2 << 16,
	MDEC_STAT_BLOCK_Y3      = 3 << 16,
	MDEC_STAT_BLOCK_CR      = 4 << 16,
	MDEC_STAT_BLOCK_CB      = 5 << 16,
	MDEC_STAT_DREQ_OUT      = 1 << 27,
	MDEC_STAT_DREQ_IN       = 1 << 28,
	MDEC_STAT_BUSY          = 1 << 29,
	MDEC_STAT_DATA_FULL     = 1 << 30,
	MDEC_STAT_DATA_EMPTY    = 1 << 31
} MDECStatusFlag;

typedef enum {
	MDEC_CTRL_DMA_OUT = 1 << 29,
	MDEC_CTRL_DMA_IN  = 1 << 30,
	MDEC_CTRL_RESET   = 1 << 31
} MDECControlFlag;

#define MDEC0 _MMIO32(IO_BASE | 0x820)
#define MDEC1 _MMIO32(IO_BASE | 0x824)

/* SPU */

typedef enum {
	SPU_STAT_CDDA           = 1 <<  0,
	SPU_STAT_EXT            = 1 <<  1,
	SPU_STAT_CDDA_REVERB    = 1 <<  2,
	SPU_STAT_EXT_REVERB     = 1 <<  3,
	SPU_STAT_XFER_BITMASK   = 3 <<  4,
	SPU_STAT_XFER_NONE      = 0 <<  4,
	SPU_STAT_XFER_WRITE     = 1 <<  4,
	SPU_STAT_XFER_DMA_WRITE = 2 <<  4,
	SPU_STAT_XFER_DMA_READ  = 3 <<  4,
	SPU_STAT_IRQ            = 1 <<  6,
	SPU_STAT_DREQ           = 1 <<  7,
	SPU_STAT_WRITE_REQ      = 1 <<  8,
	SPU_STAT_READ_REQ       = 1 <<  9,
	SPU_STAT_BUSY           = 1 << 10,
	SPU_STAT_CAPTURE_BUF    = 1 << 11
} SPUStatusFlag;

typedef enum {
	SPU_CTRL_CDDA           = 1 <<  0,
	SPU_CTRL_EXT            = 1 <<  1,
	SPU_CTRL_CDDA_REVERB    = 1 <<  2,
	SPU_CTRL_EXT_REVERB     = 1 <<  3,
	SPU_CTRL_XFER_BITMASK   = 3 <<  4,
	SPU_CTRL_XFER_NONE      = 0 <<  4,
	SPU_CTRL_XFER_WRITE     = 1 <<  4,
	SPU_CTRL_XFER_DMA_WRITE = 2 <<  4,
	SPU_CTRL_XFER_DMA_READ  = 3 <<  4,
	SPU_CTRL_IRQ_ENABLE     = 1 <<  6,
	SPU_CTRL_REVERB         = 1 <<  7,
	SPU_CTRL_UNMUTE         = 1 << 14,
	SPU_CTRL_ENABLE         = 1 << 15
} SPUControlFlag;

#define SPU_CH_VOL_L(N) _MMIO16((IO_BASE | 0xc00) + (16 * (N)))
#define SPU_CH_VOL_R(N) _MMIO16((IO_BASE | 0xc02) + (16 * (N)))
#define SPU_CH_FREQ(N)  _MMIO16((IO_BASE | 0xc04) + (16 * (N)))
#define SPU_CH_ADDR(N)  _MMIO16((IO_BASE | 0xc06) + (16 * (N)))
#define SPU_CH_ADSR1(N) _MMIO16((IO_BASE | 0xc08) + (16 * (N)))
#define SPU_CH_ADSR2(N) _MMIO16((IO_BASE | 0xc0a) + (16 * (N)))
#define SPU_CH_LOOP(N)  _MMIO16((IO_BASE | 0xc0e) + (16 * (N)))

#define SPU_MASTER_VOL_L _MMIO16(IO_BASE | 0xd80)
#define SPU_MASTER_VOL_R _MMIO16(IO_BASE | 0xd82)
#define SPU_REVERB_VOL_L _MMIO16(IO_BASE | 0xd84)
#define SPU_REVERB_VOL_R _MMIO16(IO_BASE | 0xd86)
#define SPU_FLAG_ON1     _MMIO16(IO_BASE | 0xd88)
#define SPU_FLAG_ON2     _MMIO16(IO_BASE | 0xd8a)
#define SPU_FLAG_OFF1    _MMIO16(IO_BASE | 0xd8c)
#define SPU_FLAG_OFF2    _MMIO16(IO_BASE | 0xd8e)
#define SPU_FLAG_FM1     _MMIO16(IO_BASE | 0xd90)
#define SPU_FLAG_FM2     _MMIO16(IO_BASE | 0xd92)
#define SPU_FLAG_NOISE1  _MMIO16(IO_BASE | 0xd94)
#define SPU_FLAG_NOISE2  _MMIO16(IO_BASE | 0xd96)
#define SPU_FLAG_REVERB1 _MMIO16(IO_BASE | 0xd98)
#define SPU_FLAG_REVERB2 _MMIO16(IO_BASE | 0xd9a)
#define SPU_FLAG_STATUS1 _MMIO16(IO_BASE | 0xd9c)
#define SPU_FLAG_STATUS2 _MMIO16(IO_BASE | 0xd9e)

#define SPU_REVERB_ADDR _MMIO16(IO_BASE | 0xda2)
#define SPU_IRQ_ADDR    _MMIO16(IO_BASE | 0xda4)
#define SPU_ADDR        _MMIO16(IO_BASE | 0xda6)
#define SPU_DATA        _MMIO16(IO_BASE | 0xda8)
#define SPU_CTRL        _MMIO16(IO_BASE | 0xdaa)
#define SPU_DMA_CTRL    _MMIO16(IO_BASE | 0xdac)
#define SPU_STAT        _MMIO16(IO_BASE | 0xdae)

#define SPU_CDDA_VOL_L _MMIO16(IO_BASE | 0xdb0)
#define SPU_CDDA_VOL_R _MMIO16(IO_BASE | 0xdb2)
#define SPU_EXT_VOL_L  _MMIO16(IO_BASE | 0xdb4)
#define SPU_EXT_VOL_R  _MMIO16(IO_BASE | 0xdb6)
#define SPU_VOL_STAT_L _MMIO16(IO_BASE | 0xdb8)
#define SPU_VOL_STAT_R _MMIO16(IO_BASE | 0xdba)

#define SPU_REVERB_BASE _ADDR16(IO_BASE | 0xdc0)

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
	SYS573_MISC_OUT_JVS_STAT    = 1 << 8
} Sys573MiscOutputFlag;

typedef enum {
	SYS573_MISC_IN_ADC_MISO   = 1 <<  0,
	SYS573_MISC_IN_ADC_SARS   = 1 <<  1,
	SYS573_MISC_IN_CART_SDA   = 1 <<  2,
	SYS573_MISC_IN_JVS_SENSE  = 1 <<  3,
	SYS573_MISC_IN_JVS_AVAIL  = 1 <<  4,
	SYS573_MISC_IN_JVS_UNK    = 1 <<  5,
	SYS573_MISC_IN_CART_ISIG  = 1 <<  6,
	SYS573_MISC_IN_CART_DSIG  = 1 <<  7,
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

#define SYS573_MISC_OUT    _MMIO16(DEV0_BASE | 0x400000)
#define SYS573_DIP_CART    _MMIO16(DEV0_BASE | 0x400004)
#define SYS573_MISC_IN     _MMIO16(DEV0_BASE | 0x400006)
#define SYS573_JAMMA_MAIN  _MMIO16(DEV0_BASE | 0x400008)
#define SYS573_JVS_RX_DATA _MMIO16(DEV0_BASE | 0x40000a)
#define SYS573_JAMMA_EXT1  _MMIO16(DEV0_BASE | 0x40000c)
#define SYS573_JAMMA_EXT2  _MMIO16(DEV0_BASE | 0x40000e)
#define SYS573_BANK_CTRL   _MMIO16(DEV0_BASE | 0x500000)
#define SYS573_JVS_RESET   _MMIO16(DEV0_BASE | 0x520000)
#define SYS573_IDE_RESET   _MMIO16(DEV0_BASE | 0x560000)
#define SYS573_WATCHDOG    _MMIO16(DEV0_BASE | 0x5c0000)
#define SYS573_EXT_OUT     _MMIO16(DEV0_BASE | 0x600000)
#define SYS573_JVS_TX_DATA _MMIO16(DEV0_BASE | 0x680000)
#define SYS573_CART_OUT    _MMIO16(DEV0_BASE | 0x6a0000)

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
	SYS573D_CPLD_STAT_LDC  = 1 << 14,
	SYS573D_CPLD_STAT_HDC  = 1 << 15
} Sys573DCPLDStatusFlag;

typedef enum {
	SYS573D_CPLD_CTRL_UNK1 = 1 << 12,
	SYS573D_CPLD_CTRL_UNK2 = 1 << 13,
	SYS573D_CPLD_CTRL_UNK3 = 1 << 14,
	SYS573D_CPLD_CTRL_UNK4 = 1 << 15
} Sys573DCPLDControlFlag;

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
