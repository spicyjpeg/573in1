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

`include "defs.vh"

module FPGA (
	input clkInMain,
	input clkInUART,

	input        nHostCS,
	input        nHostRead,
	input        nHostWrite,
	input [6:0]  hostAddr,
	inout [15:0] hostData,

	output        nSRAMCS,
	output        nSRAMRead,
	output        nSRAMWrite,
	output [16:0] sramAddr,
	inout  [7:0]  sramData,

	output        nDRAMRead,
	output        nDRAMWrite,
	output [2:0]  nDRAMRAS,
	output [2:0]  nDRAMLCAS,
	output [2:0]  nDRAMUCAS,
	output [12:0] dramAddr,
	inout  [15:0] dramData,

	output reg mp3ClkIn = 1'b0,
	input      mp3ClkOut,

	output reg mp3Reset = 1'b0,
	input      mp3Ready,
	inout      mp3SDA,
	inout      mp3SCL,

	output reg mp3StatusCS = 1'b1,
	input      mp3StatusError,
	input      mp3StatusFrameSync,
	input      mp3StatusDataReq,

	output mp3InSDIN,
	output mp3InBCLK,
	output mp3InLRCK,
	input  mp3OutSDOUT,
	input  mp3OutBCLK,
	input  mp3OutLRCK,

	output reg dacSDIN = 1'b0,
	output reg dacBCLK = 1'b0,
	output reg dacLRCK = 1'b0,
	output     dacMCLK,

	output networkTXE,
	output networkTX,
	input  networkRX,

	output serialTX,
	input  serialRX,
	output serialRTS,
	input  serialCTS,
	output serialDTR,
	input  serialDSR,

	output [3:0] lightBankAH,
	output [3:0] lightBankAL,
	output [3:0] lightBankBH,
	output [3:0] lightBankD,
	input  [3:0] inputBank,

	inout ds2433,
	inout ds2401
);
	genvar i, j;

	/* System clocks */

	(* keep *) wire clkMain, clkUART;

	BUFGLS clkMainBuf (.I(clkInMain), .O(clkMain));
	BUFGLS clkUARTBuf (.I(clkInUART), .O(clkUART));

	/* Host interface */

	wire        hostAPBEnable, hostAPBWrite;
	wire [6:0]  hostAPBAddr;
	wire [15:0] hostAPBRData;
	wire [15:0] hostAPBWData;

	APBHostBridge #(
		.ADDR_WIDTH(7),
		.DATA_WIDTH(16),

		// The FPGA shall only respond to addresses in 0x80-0xef range, as
		// 0xf0-0xff is used by the CPLD and 0x00-0x7f seems to be reserved for
		// debugging hardware.
		.VALID_ADDR_MASK   (7'b1000000),
		.VALID_ADDR_VALUE  (7'b1000000),
		.INVALID_ADDR_MASK (7'b0111000),
		.INVALID_ADDR_VALUE(7'b0111000)
	) hostBridge (
		.clk(clkMain),

		.nHostCS   (nHostCS),
		.nHostRead (nHostRead),
		.nHostWrite(nHostWrite),
		.hostAddr  (hostAddr),
		.hostData  (hostData),

		// The ready input is tied high as all accesses are processed in one
		// cycle. This also ensures hostAPBEnable is not strobed for longer than
		// a single cycle.
		.apbEnable(hostAPBEnable),
		.apbWrite (hostAPBWrite),
		.apbReady (1'b1),
		.apbAddr  (hostAPBAddr),
		.apbRData (hostAPBRData),
		.apbWData (hostAPBWData)
	);

	/* Host address decoder */

	wire [255:0] hostRegRead;
	wire [255:0] hostRegWrite;

	generate
		// Bit 0 of the 573's address bus is not wired to the FPGA as all
		// registers are 16 bits wide and aligned. Bit 7 of the bus (hostAPBAddr
		// bit 6) is not checked as it is always set for addresses 0x80-0xef.
		for (i = 0; i < 14; i = i + 1) begin
			wire       msbValid;
			wire [3:0] lsb = { msbValid, hostAPBWrite, hostAPBAddr[1:0] };

			CompareConst #(
				.WIDTH(5),
				.VALUE({ 1'b1, i[3:0] })
			) readDec (
				.value ({ hostAPBEnable, hostAPBAddr[5:2] }),
				.result(msbValid)
			);

			for (j = 0; j < 4; j = j + 1) begin
				localparam addr = { 1'b1, i[3:0], j[1:0], 1'b0 };

				CompareConst #(
					.WIDTH(4),
					.VALUE({ 2'b10, j[1:0] })
				) readDec (
					.value (lsb),
					.result(hostRegRead[addr])
				);
				CompareConst #(
					.WIDTH(4),
					.VALUE({ 2'b11, j[1:0] })
				) writeDec (
					.value (lsb),
					.result(hostRegWrite[addr])
				);
			end
		end
	endgenerate

	/* Configuration and reset registers */

	// TODO: find out which bits reset which blocks in Konami's bitstream
	reg [3:0] mp3StateMachineCfg = 4'b1110;
	reg [3:0] mp3DescramblerCfg  = 4'b0100;
	reg [3:0] nFPGAReset         = 4'b1111;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MAGIC]
		? `BITSTREAM_MAGIC : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_CONFIG]
		? {
			mp3DescramblerCfg, mp3StateMachineCfg, `BITSTREAM_VERSION
		} : 16'hzzzz;

	always @(posedge clkMain) begin
		if (hostRegWrite[`SYS573D_FPGA_CONFIG]) begin
			mp3StateMachineCfg <= hostAPBWData[11:8];
			mp3DescramblerCfg  <= hostAPBWData[15:12];
		end

		if (hostRegWrite[`SYS573D_FPGA_RESET])
			nFPGAReset <= hostAPBWData[15:12];
	end

	/* SRAM interface (currently unused) */

	assign nSRAMCS    = 1'b1;
	assign nSRAMRead  = 1'b1;
	assign nSRAMWrite = 1'b1;
	assign sramAddr   = 17'h1ffff;

	/* DRAM interface */

	wire        dramAPBEnable, dramAPBWrite, dramAPBReady;
	wire [23:0] dramAPBAddr;
	wire [15:0] dramAPBRData;
	wire [15:0] dramAPBWData;

	assign nDRAMUCAS    = nDRAMLCAS;
	assign dramAddr[12] = 1'b0;

	DRAMController #(
		.ROW_WIDTH (12),
		.COL_WIDTH (10),
		.DATA_WIDTH(16),
		.NUM_CHIPS (3),

		.REFRESH_PERIOD(250)
	) dramController (
		.clk  (clkMain),
		.reset(1'b0),

		.apbEnable(dramAPBEnable),
		.apbWrite (dramAPBWrite),
		.apbReady (dramAPBReady),
		.apbAddr  (dramAPBAddr),
		.apbRData (dramAPBRData),
		.apbWData (dramAPBWData),

		.nDRAMRead (nDRAMRead),
		.nDRAMWrite(nDRAMWrite),
		.nDRAMRAS  (nDRAMRAS),
		.nDRAMCAS  (nDRAMLCAS),
		.dramAddr  (dramAddr[11:0]),
		.dramData  (dramData)
	);

	/* DRAM access arbiter */

	// Each DRAM address is mapped to two different registers, one for bits
	// 0-14 (the LSB is ignored as all accesses are word aligned) and the
	// other for bits 15-23.
	wire [23:0] hostAPBWDRAMAddr = { hostAPBWData[8:0], hostAPBWData[15:1] };
	wire [15:0] dramMainRData;

	wire        dramAuxRAddrWrite, dramAuxRDataAck, dramAuxRReady;
	wire [23:0] dramAuxRAddr;
	wire [23:0] dramAuxRAddrWData;
	wire [15:0] dramAuxRData;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_PTR_H]
		? { 7'h00, dramAuxRAddr[23:15] } : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_PTR_L]
		? { dramAuxRAddr[14:0], 1'h0 }   : 16'hzzzz;

	assign hostAPBRData = (
		hostRegRead[`SYS573D_FPGA_DRAM_DATA] |
		hostRegRead[`SYS573D_FPGA_DRAM_PEEK]
	) ? dramMainRData : 16'hzzzz;

	DRAMArbiter #(
		.ADDR_H_WIDTH(9),
		.ADDR_L_WIDTH(15),
		.DATA_WIDTH  (16)
	) dramArbiter (
		.clk  (clkMain),
		.reset(1'b0),

		.mainWAddrHWrite(hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_H]),
		.mainWAddrLWrite(hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_L]),
		.mainWrite      (hostRegWrite[`SYS573D_FPGA_DRAM_DATA]),
		.mainWAddrWData (hostAPBWDRAMAddr),
		.mainWData      (hostAPBWData),

		.mainRAddrHWrite(hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_H]),
		.mainRAddrLWrite(hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_L]),
		.mainRDataAck   (hostRegRead [`SYS573D_FPGA_DRAM_DATA]),
		.mainRAddrWData (hostAPBWDRAMAddr),
		.mainRData      (dramMainRData),

		.auxRAddrHWrite(dramAuxRAddrWrite),
		.auxRAddrLWrite(dramAuxRAddrWrite),
		.auxRDataAck   (dramAuxRDataAck),
		.auxRReady     (dramAuxRReady),
		.auxRAddr      (dramAuxRAddr),
		.auxRAddrWData (dramAuxRAddrWData),
		.auxRData      (dramAuxRData),

		.apbEnable(dramAPBEnable),
		.apbWrite (dramAPBWrite),
		.apbReady (dramAPBReady),
		.apbAddr  (dramAPBAddr),
		.apbRData (dramAPBRData),
		.apbWData (dramAPBWData)
	);

	/* MP3 clock generator and DAC wiring */

	reg dacMCLKOut = 1'b0;

	always @(posedge clkMain)
		mp3ClkIn   <= ~mp3ClkIn;
	always @(posedge mp3ClkOut)
		dacMCLKOut <= ~dacMCLKOut;

	BUFGLS dacMCLKBuf (.I(dacMCLKOut), .O(dacMCLK));

	always @(posedge dacMCLK) begin
		dacSDIN <= mp3OutSDOUT;
		dacBCLK <= mp3OutBCLK;
		dacLRCK <= mp3OutLRCK;
	end

	/* MP3 decoder control interface */

	reg mp3ReadyIn           = 1'b0;
	reg mp3StatusErrorIn     = 1'b0;
	reg mp3StatusFrameSyncIn = 1'b0;
	reg mp3StatusDataReqIn   = 1'b0;

	reg mp3SDAIn  = 1'b1;
	reg mp3SCLIn  = 1'b1;
	reg mp3SDAOut = 1'b1;
	reg mp3SCLOut = 1'b1;

	assign mp3SDA = mp3SDAOut ? 1'bz : 1'b0;
	assign mp3SCL = mp3SCLOut ? 1'bz : 1'b0;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_CHIP_STAT]
		? {
			mp3ReadyIn,
			mp3StatusFrameSyncIn,
			mp3StatusErrorIn,
			mp3StatusDataReqIn,
			12'h000
		} : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_I2C]
		? { 2'h0, mp3SCLIn, mp3SDAIn, 12'h000 } : 16'hzzzz;

	always @(posedge clkMain) begin
		mp3ReadyIn           <= mp3Ready;
		mp3StatusErrorIn     <= mp3StatusError;
		mp3StatusFrameSyncIn <= mp3StatusFrameSync;
		mp3StatusDataReqIn   <= mp3StatusDataReq;

		mp3SDAIn <= mp3SDA;
		mp3SCLIn <= mp3SCL;

		if (hostRegWrite[`SYS573D_FPGA_MP3_CHIP_CTRL]) begin
			mp3Reset    <= hostAPBWData[12];
			mp3StatusCS <= hostAPBWData[13];
		end
		if (hostRegWrite[`SYS573D_FPGA_MP3_I2C]) begin
			mp3SDAOut <= hostAPBWData[12];
			mp3SCLOut <= hostAPBWData[13];
		end
	end

	/* MP3 playback state machine */

	wire        mp3Enabled, mp3Playing;
	wire [15:0] mp3FrameCount;
	wire [31:0] mp3SampleCount;
	wire [15:0] mp3SampleDelta;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_FEED_STAT]
		? { 3'h0, mp3Playing, 12'h000 } : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_COUNTER]
		? mp3FrameCount                 : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_H]
		? mp3SampleCount[31:16]         : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_L]
		? mp3SampleCount[15:0]          : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_D]
		? mp3SampleDelta                : 16'hzzzz;

	MP3StateMachine #(
		.ADDR_H_WIDTH        (9),
		.ADDR_L_WIDTH        (15),
		.FRAME_COUNT_WIDTH   (16),
		.SAMPLE_COUNT_H_WIDTH(16),
		.SAMPLE_COUNT_L_WIDTH(16),
		.SAMPLE_DELTA_WIDTH  (16)
	) mp3StateMachine (
		.clk  (clkMain),
		.reset(1'b0),
		.cfg  (mp3StateMachineCfg),

		.enabled  (mp3Enabled),
		.playing  (mp3Playing),
		.ctrlWrite(hostRegWrite[`SYS573D_FPGA_MP3_FEED_CTRL]),
		.ctrlWData(hostAPBWData[15:13]),

		.startAddrHWrite(hostRegWrite[`SYS573D_FPGA_MP3_PTR_H]),
		.startAddrLWrite(hostRegWrite[`SYS573D_FPGA_MP3_PTR_L]),
		.endAddrHWrite  (hostRegWrite[`SYS573D_FPGA_MP3_END_PTR_H]),
		.endAddrLWrite  (hostRegWrite[`SYS573D_FPGA_MP3_END_PTR_L]),
		.addrWData      (hostAPBWDRAMAddr),

		.feederAddrWrite(dramAuxRAddrWrite),
		.feederReady    (dramAuxRReady),
		.feederAddr     (dramAuxRAddr),
		.feederAddrWData(dramAuxRAddrWData),

		.sampleCountReset(hostRegWrite[`SYS573D_FPGA_DAC_COUNTER_L]),
		.sampleDeltaReset(
			hostRegRead[`SYS573D_FPGA_DAC_COUNTER_H] |
			hostRegRead[`SYS573D_FPGA_DAC_COUNTER_L]
		),
		.sampleDeltaRead (hostRegRead [`SYS573D_FPGA_DAC_COUNTER_D]),
		.frameCount      (mp3FrameCount),
		.sampleCount     (mp3SampleCount),
		.sampleDelta     (mp3SampleDelta),

		.mp3FrameSync(mp3StatusFrameSyncIn),
		.mp3Error    (mp3StatusErrorIn),
		.mp3SampleClk(mp3OutLRCK)
	);

	/* MP3 decoder data feeder */

	wire        mp3DataReady = mp3Enabled & mp3Playing & dramAuxRReady;
	wire [15:0] mp3Data;

	assign mp3InLRCK = 1'b0;

	MP3Descrambler mp3Descrambler (
		.clk  (clkMain),
		.reset(1'b0),
		.cfg  (mp3DescramblerCfg),

		.key1Write(hostRegWrite[`SYS573D_FPGA_MP3_KEY1]),
		.key2Write(hostRegWrite[`SYS573D_FPGA_MP3_KEY2]),
		.key3Write(hostRegWrite[`SYS573D_FPGA_MP3_KEY3]),
		.keyWData (hostAPBWData),

		.dataIn (dramAuxRData),
		.dataOut(mp3Data),
		.dataAck(dramAuxRDataAck)
	);
	MP3DataFeeder #(
		// The minimum I2S input bit clock period is ~28 cycles (rounded to 16
		// cycles per bit clock state) as per the MAS3507D datasheet.
		.DATA_BITS     (16),
		.BIT_CLK_PERIOD(32)
	) mp3DataFeeder (
		.clk   (clkMain),
		.reset (1'b0),

		.dataIn   (mp3Data),
		.dataReady(mp3DataReady),
		.dataAck  (dramAuxRDataAck),

		.dataOut   (mp3InSDIN),
		.dataOutClk(mp3InBCLK),
		.dataOutReq(mp3StatusDataReqIn)
	);

	/* Network interface (currently unused) */

	assign networkTXE = 1'b1;
	assign networkTX  = 1'b1;

	/* Serial interface */

	wire [7:0]  uartFIFORData;
	wire [15:0] uartCtrlRData;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_UART_DATA]
		? { 8'h00, uartFIFORData } : 16'hzzzz;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_UART_CTRL]
		? uartCtrlRData            : 16'hzzzz;

	UART uart (
		.clk    (clkMain),
		.baudClk(clkUART),
		.reset  (1'b0),

		.fifoRead (hostRegRead [`SYS573D_FPGA_UART_DATA]),
		.fifoRData(uartFIFORData),
		.fifoWrite(hostRegWrite[`SYS573D_FPGA_UART_DATA]),
		.fifoWData(hostAPBWData[7:0]),

		.ctrlRead (hostRegRead [`SYS573D_FPGA_UART_CTRL]),
		.ctrlRData(uartCtrlRData),
		.ctrlWrite(hostRegWrite[`SYS573D_FPGA_UART_CTRL]),
		.ctrlWData(hostAPBWData),

		.tx (serialTX),
		.rx (serialRX),
		.rts(serialRTS),
		.cts(serialCTS),
		.dtr(serialDTR),
		.dsr(serialDSR)
	);

	/* Light outputs */

	reg [3:0] lightBankAHOut = 4'b1111;
	reg [3:0] lightBankALOut = 4'b1111;
	reg [3:0] lightBankBHOut = 4'b1111;
	reg [3:0] lightBankDOut  = 4'b1111;

	generate
		for (i = 0; i < 4; i = i + 1) begin
			assign lightBankAH[i] = lightBankAHOut[i] ? 1'bz : 1'b0;
			assign lightBankAL[i] = lightBankALOut[i] ? 1'bz : 1'b0;
			assign lightBankBH[i] = lightBankBHOut[i] ? 1'bz : 1'b0;
			assign lightBankD [i] = lightBankDOut [i] ? 1'bz : 1'b0;
		end
	endgenerate

	always @(posedge clkMain) begin
		if (hostRegWrite[`SYS573D_FPGA_LIGHTS_AH])
			lightBankAHOut <= hostAPBWData[15:12];
		if (hostRegWrite[`SYS573D_FPGA_LIGHTS_AL])
			lightBankALOut <= hostAPBWData[15:12];
		if (hostRegWrite[`SYS573D_FPGA_LIGHTS_BH])
			lightBankBHOut <= hostAPBWData[15:12];
		if (hostRegWrite[`SYS573D_FPGA_LIGHTS_D])
			lightBankDOut  <= hostAPBWData[15:12];
	end

	/* 1-wire bus */

	reg ds2433In  = 1'b1;
	reg ds2401In  = 1'b1;
	reg ds2433Out = 1'b0;
	reg ds2401Out = 1'b0;

	// The 1-wire pins are pulled low by writing 1 (rather than 0) to the
	// respective register bits, but not inverted when read.
	assign ds2433 = ds2433Out ? 1'b0 : 1'bz;
	assign ds2401 = ds2401Out ? 1'b0 : 1'bz;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DS_BUS]
		? { 3'h0, ds2401In, 3'h0, ds2433In, 8'h00 } : 16'hzzzz;

	always @(posedge clkMain) begin
		ds2433In <= ds2433;
		ds2401In <= ds2401;

		if (hostRegWrite[`SYS573D_FPGA_DS_BUS]) begin
			ds2433Out <= hostAPBWData[8];
			ds2401Out <= hostAPBWData[12];
		end
	end
endmodule
