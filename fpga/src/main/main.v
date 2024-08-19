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

	output reg mp3ClkIn,
	input      mp3ClkOut,

	output reg mp3Reset,
	input      mp3Ready,
	inout      mp3SDA,
	inout      mp3SCL,

	output reg mp3StatusCS,
	input      mp3StatusError,
	input      mp3StatusFrameSync,
	input      mp3StatusDataReq,

	output mp3InSDIN,
	output mp3InBCLK,
	output mp3InLRCK,
	input  mp3OutSDOUT,
	input  mp3OutBCLK,
	input  mp3OutLRCK,

	output reg dacSDIN,
	output reg dacBCLK,
	output reg dacLRCK,
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

	inout ds2433,
	inout ds2401
);
	genvar i;

	/* System clocks */

	(* keep *) wire clkMain, clkUART;

	BUFGLS clkBufMain (.I(clkInMain), .O(clkMain));
	BUFGLS clkBufUART (.I(clkInUART), .O(clkUART));

	/* Host interface */

	wire        hostAPBEnable, hostAPBWrite;
	reg         hostAPBReady = 1'b0;
	wire [7:0]  hostAPBAddr;
	wor  [15:0] hostAPBRData;
	wire [15:0] hostAPBWData;

	APBHostBridge #(
		.ADDR_WIDTH(8),
		.DATA_WIDTH(16),

		// The FPGA shall only respond to addresses in 0x80-0xef range, as
		// 0xf0-0xff is used by the CPLD and 0x00-0x7f seems to be reserved for
		// debugging hardware.
		.VALID_ADDR_MASK   (8'b10000000),
		.VALID_ADDR_VALUE  (8'b10000000),
		.INVALID_ADDR_MASK (8'b01110000),
		.INVALID_ADDR_VALUE(8'b01110000)
	) hostBridge (
		.clk  (clkMain),
		.reset(1'b0),

		// Bit 0 of the 573's address bus is not wired to the FPGA as all
		// registers are 16 bits wide and aligned.
		.nHostCS   (nHostCS),
		.nHostRead (nHostRead),
		.nHostWrite(nHostWrite),
		.hostAddr  ({ hostAddr, 1'b0 }),
		.hostData  (hostData),

		.apbEnable(hostAPBEnable),
		.apbWrite (hostAPBWrite),
		.apbReady (hostAPBReady),
		.apbAddr  (hostAPBAddr),
		.apbRData (hostAPBRData),
		.apbWData (hostAPBWData)
	);

	/* Address decoding */

	// All accesses are processed in two cycles, one to decode the address and
	// one to actually dispatch the transaction.
	reg [255:0] hostRegRead  = 256'h0;
	reg [255:0] hostRegWrite = 256'h0;

	generate
		for (i = 128; i < 256; i = i + 2) begin
			wire cs = hostAPBEnable & (hostAPBAddr == i);

			always @(posedge clkMain) begin
				hostRegRead [i] <= cs & ~hostAPBWrite;
				hostRegWrite[i] <= cs &  hostAPBWrite;
			end
		end
	endgenerate

	always @(posedge clkMain)
		hostAPBReady <= hostAPBEnable;

	/* Configuration and reset registers */

	// TODO: find out which bits reset which blocks in Konami's bitstream
	reg nFPGAReset0 = 1'b1;
	reg nFPGAReset1 = 1'b1;
	reg nFPGAReset2 = 1'b1;
	reg nFPGAReset3 = 1'b1;

	reg [1:0] mp3DescramblerMode      = 2'h0;
	reg       resetCountOnStart       = 1'b1;
	reg       disableCountOnIdleReset = 1'b1;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MAGIC]
		? `BITSTREAM_MAGIC : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_CONFIG]
		? {
			mp3DescramblerMode,
			disableCountOnIdleReset,
			resetCountOnStart,
			4'h0,
			`BITSTREAM_VERSION
		} : 16'h0000;

	always @(posedge clkMain) begin
		if (hostRegWrite[`SYS573D_FPGA_CONFIG]) begin
			resetCountOnStart       <= hostAPBWData[12];
			disableCountOnIdleReset <= hostAPBWData[13];
			mp3DescramblerMode      <= hostAPBWData[15:14];
		end
		if (hostRegWrite[`SYS573D_FPGA_RESET]) begin
			nFPGAReset0 <= hostAPBWData[12];
			nFPGAReset1 <= hostAPBWData[13];
			nFPGAReset2 <= hostAPBWData[14];
			nFPGAReset3 <= hostAPBWData[15];
		end
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

	wire [23:0] dramMainWAddr;
	wire [23:0] dramMainRAddr;
	wire [15:0] dramMainRData;

	wire [23:0] dramAuxRAddr;
	wire [15:0] dramAuxRData;
	wire        dramAuxRAck, dramAuxRReady;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DRAM_DATA]
		? dramMainRData                  : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DRAM_PEEK]
		? dramMainRData                  : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_PTR_H]
		? { 7'h00, dramAuxRAddr[23:15] } : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_PTR_L]
		? { dramAuxRAddr[14:0], 1'h0 }   : 16'h0000;

	// Each DRAM address is mapped to two different registers, one for bits 0-14
	// (the LSB is ignored as the 573 writes byte offsets) and the other for
	// bits 15-23.
	wire [8:0]  hostDRAMAddrH = hostAPBWData[8:0];
	wire [14:0] hostDRAMAddrL = hostAPBWData[15:1];
	wor  [23:0] dramAddrValue;

	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_H]
		? { hostDRAMAddrH, dramMainWAddr[14:0] }  : 24'h000000;
	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_L]
		? { dramMainWAddr[23:15], hostDRAMAddrL } : 24'h000000;
	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_H]
		? { hostDRAMAddrH, dramMainRAddr[14:0] }  : 24'h000000;
	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_L]
		? { dramMainRAddr[23:15], hostDRAMAddrL } : 24'h000000;
	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_MP3_PTR_H]
		? { hostDRAMAddrH, dramAuxRAddr[14:0] }   : 24'h000000;
	assign dramAddrValue = hostRegWrite[`SYS573D_FPGA_MP3_PTR_L]
		? { dramAuxRAddr[23:15], hostDRAMAddrL }  : 24'h000000;

	wire dramMainWAddrLoad = 1'b0
		| hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_H]
		| hostRegWrite[`SYS573D_FPGA_DRAM_WR_PTR_L];
	wire dramMainRAddrLoad = 1'b0
		| hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_H]
		| hostRegWrite[`SYS573D_FPGA_DRAM_RD_PTR_L];
	wire dramAuxRAddrLoad  = 1'b0
		| hostRegWrite[`SYS573D_FPGA_MP3_PTR_H]
		| hostRegWrite[`SYS573D_FPGA_MP3_PTR_L];

	DRAMArbiter #(
		// FIXME: enabling the round robin scheduler pushes the latency of some
		// nets way too close to the 29.45 MHz limit...
		.ADDR_WIDTH (24),
		.DATA_WIDTH (16),
		.ROUND_ROBIN(0)
	) dramArbiter (
		.clk  (clkMain),
		.reset(1'b0),

		.addrValue    (dramAddrValue),
		.mainWAddrLoad(dramMainWAddrLoad),
		.mainRAddrLoad(dramMainRAddrLoad),
		.auxRAddrLoad (dramAuxRAddrLoad),

		.mainWAddr(dramMainWAddr),
		.mainWData(hostAPBWData),
		.mainWReq (hostRegWrite[`SYS573D_FPGA_DRAM_DATA]),

		.mainRAddr(dramMainRAddr),
		.mainRData(dramMainRData),
		.mainRAck (hostRegRead[`SYS573D_FPGA_DRAM_DATA]),

		.auxRAddr (dramAuxRAddr),
		.auxRData (dramAuxRData),
		.auxRAck  (dramAuxRAck),
		.auxRReady(dramAuxRReady),

		.apbEnable(dramAPBEnable),
		.apbWrite (dramAPBWrite),
		.apbReady (dramAPBReady),
		.apbAddr  (dramAPBAddr),
		.apbRData (dramAPBRData),
		.apbWData (dramAPBWData)
	);

	/* MP3 clock generator and DAC wiring */

	reg dacMCLKOut;

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

	/* MP3 decoder reset and I2C interface */

	reg mp3ReadyIn, mp3StatusErrorIn, mp3StatusFrameSyncIn, mp3StatusDataReqIn;
	reg mp3SDAIn, mp3SDAOut, mp3SCLIn, mp3SCLOut;

	assign mp3SDA = mp3SDAOut ? 1'bz : 1'b0;
	assign mp3SCL = mp3SCLOut ? 1'bz : 1'b0;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_CHIP_STAT]
		? {
			mp3ReadyIn,
			mp3StatusFrameSyncIn,
			mp3StatusErrorIn,
			mp3StatusDataReqIn,
			12'h000
		} : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_I2C]
		? { 2'h0, mp3SCLIn, mp3SDAIn, 12'h000 } : 16'h0000;

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

	wire        mp3IsBusy;
	wire [15:0] mp3FrameCount;
	wire [27:0] mp3SampleCount;
	wire [15:0] mp3SampleDelta;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_COUNTER]
		? mp3FrameCount                   : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_MP3_FEED_STAT]
		? { 3'h0, mp3IsBusy, 12'h000 }    : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_H]
		? { 4'h0, mp3SampleCount[27:16] } : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_L]
		? mp3SampleCount[15:0]            : 16'h0000;
	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DAC_COUNTER_D]
		? mp3SampleDelta                  : 16'h0000;

	wire sampleDeltaReset = 1'b0
		| hostRegRead[`SYS573D_FPGA_DAC_COUNTER_H]
		| hostRegRead[`SYS573D_FPGA_DAC_COUNTER_L]
		| hostRegRead[`SYS573D_FPGA_DAC_COUNTER_D];

	// Playback is automatically stopped once the current read pointer reaches
	// the end address.
	reg [23:0] mp3EndAddr;

	always @(posedge clkMain) begin
		if (hostRegWrite[`SYS573D_FPGA_MP3_END_PTR_H])
			mp3EndAddr[23:15] <= hostDRAMAddrH;
		if (hostRegWrite[`SYS573D_FPGA_MP3_END_PTR_L])
			mp3EndAddr[14:0]  <= hostDRAMAddrL;
	end

	MP3StateMachine #(
		// Note that 28 bits is the maximum size for a counter due to the
		// requirement for its CLBs to be vertically adjacent (the XCS40XL has a
		// 28x28 CLB grid).
		.FRAME_COUNT_WIDTH (16),
		.SAMPLE_COUNT_WIDTH(28),
		.SAMPLE_DELTA_WIDTH(16)
	) mp3StateMachine (
		.clk  (clkMain),
		.reset(1'b0),

		.resetCountOnStart      (resetCountOnStart),
		.disableCountOnIdleReset(disableCountOnIdleReset),

		.enabledValue     (hostAPBWData[13]),
		.playingValue     (hostAPBWData[14]),
		.frameCountEnValue(hostAPBWData[15]),
		.loadFlags        (hostRegWrite[`SYS573D_FPGA_MP3_FEED_CTRL]),

		.forceStop       (dramAuxRAddr == mp3EndAddr),
		.sampleCountReset(hostRegWrite[`SYS573D_FPGA_DAC_COUNTER_L]),
		.sampleDeltaReset(sampleDeltaReset),

		.mp3FrameSync(mp3StatusFrameSync),
		.mp3Error    (mp3StatusError),
		.mp3SampleClk(mp3OutLRCK),

		.isBusy     (mp3IsBusy),
		.frameCount (mp3FrameCount),
		.sampleCount(mp3SampleCount),
		.sampleDelta(mp3SampleDelta)
	);

	/* MP3 decoder data feeder */

	wire [15:0] decodedData;

	MP3Descrambler mp3Descrambler (
		.clk  (clkMain),
		.reset(1'b0),

		.mode    (mp3DescramblerMode),
		.keyValue(hostAPBWData),
		.key1Load(hostRegWrite[`SYS573D_FPGA_MP3_KEY1]),
		.key2Load(hostRegWrite[`SYS573D_FPGA_MP3_KEY2]),
		.key3Load(hostRegWrite[`SYS573D_FPGA_MP3_KEY3]),

		.dataIn    (dramAuxRData),
		.dataOut   (decodedData),
		.dataOutAck(dramAuxRAck)
	);
	MP3DataFeeder #(.DATA_WIDTH(16)) mp3DataFeeder (
		.clk  (clkMain),
		.reset(1'b0),

		.dataIn     (decodedData),
		.dataInAck  (dramAuxRAck),
		.dataInReady(mp3IsBusy & dramAuxRReady),

		.dataOut   (mp3InSDIN),
		.dataOutClk(mp3InBCLK),
		.dataOutReq(mp3StatusDataReq)
	);

	assign mp3InLRCK = 1'b0;

	/* Serial interfaces (currently unused) */

	assign networkTXE = 1'b1;
	assign networkTX  = 1'b1;

	assign serialTX  = 1'b1;
	assign serialRTS = 1'b1;
	assign serialDTR = 1'b1;

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

	reg ds2433In, ds2433Out, ds2401In, ds2401Out;

	// The 1-wire pins are pulled low by writing 1 (rather than 0) to the
	// respective register bits, but not inverted when read.
	assign ds2433 = ds2433Out ? 1'b0 : 1'bz;
	assign ds2401 = ds2401Out ? 1'b0 : 1'bz;

	assign hostAPBRData = hostRegRead[`SYS573D_FPGA_DS_BUS]
		? { 3'h0, ds2401In, 3'h0, ds2433In, 8'h00 } : 16'h0000;

	always @(posedge clkMain) begin
		ds2433In <= ds2433;
		ds2401In <= ds2401;

		if (hostRegWrite[`SYS573D_FPGA_DS_BUS]) begin
			ds2433Out <= hostAPBWData[8];
			ds2401Out <= hostAPBWData[12];
		end
	end
endmodule
