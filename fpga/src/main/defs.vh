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

`ifndef _INC_DEFS_
`define _INC_DEFS_

/* Constants */

`define BITSTREAM_MAGIC   16'h573f
`define BITSTREAM_VERSION 8'h02

/* Register definitions */

`define SYS573D_FPGA_MAGIC     8'h80
`define SYS573D_FPGA_CONFIG    8'h82 // Custom register
`define SYS573D_FPGA_UART_DATA 8'h84 // Custom register
`define SYS573D_FPGA_UART_CTRL 8'h86 // Custom register

`define SYS573D_FPGA_NET_ID 8'h90

`define SYS573D_FPGA_MP3_PTR_H     8'ha0
`define SYS573D_FPGA_MP3_PTR_L     8'ha2
`define SYS573D_FPGA_MP3_END_PTR_H 8'ha4
`define SYS573D_FPGA_MP3_END_PTR_L 8'ha6
`define SYS573D_FPGA_MP3_COUNTER   8'ha8
`define SYS573D_FPGA_MP3_KEY1      8'ha8
`define SYS573D_FPGA_MP3_CHIP_STAT 8'haa
`define SYS573D_FPGA_MP3_CHIP_CTRL 8'haa
`define SYS573D_FPGA_MP3_I2C       8'hac
`define SYS573D_FPGA_MP3_FEED_STAT 8'hae
`define SYS573D_FPGA_MP3_FEED_CTRL 8'hae

`define SYS573D_FPGA_DRAM_WR_PTR_H 8'hb0
`define SYS573D_FPGA_DRAM_WR_PTR_L 8'hb2
`define SYS573D_FPGA_DRAM_DATA     8'hb4
`define SYS573D_FPGA_DRAM_RD_PTR_H 8'hb6
`define SYS573D_FPGA_DRAM_RD_PTR_L 8'hb8
`define SYS573D_FPGA_DRAM_CTRL     8'hba
`define SYS573D_FPGA_DRAM_PEEK     8'hbc

`define SYS573D_FPGA_NET_DATA      8'hc0
`define SYS573D_FPGA_NET_TX_LENGTH 8'hc2
`define SYS573D_FPGA_NET_RX_LENGTH 8'hc4
`define SYS573D_FPGA_NET_RESET     8'hc8
`define SYS573D_FPGA_DAC_COUNTER_H 8'hca
`define SYS573D_FPGA_DAC_COUNTER_L 8'hcc
`define SYS573D_FPGA_DAC_COUNTER_D 8'hce

`define SYS573D_FPGA_LIGHTS_AH 8'he0
`define SYS573D_FPGA_LIGHTS_AL 8'he2
`define SYS573D_FPGA_LIGHTS_BH 8'he4
`define SYS573D_FPGA_LIGHTS_D  8'he6
`define SYS573D_FPGA_RESET     8'he8
`define SYS573D_FPGA_MP3_KEY2  8'hea
`define SYS573D_FPGA_MP3_KEY3  8'hec
`define SYS573D_FPGA_DS_BUS    8'hee

`endif
