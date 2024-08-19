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

/* Serial data feeder */

module MP3DataFeeder #(
	parameter DATA_WIDTH = 8,

	parameter _BIT_COUNT_WIDTH = $clog2(DATA_WIDTH + 1)
) (
	input clk,
	input reset,

	input [DATA_WIDTH - 1:0] dataIn,
	output reg               dataInAck = 1'b0,
	input                    dataInReady,

	output     dataOut,
	output reg dataOutClk = 1'b0,
	input      dataOutReq
);
	reg  [DATA_WIDTH       - 1:0] shiftReg;
	wire [_BIT_COUNT_WIDTH - 1:0] bitCount;

	wire dataInAvailable  = dataInReady &  bitCount[_BIT_COUNT_WIDTH - 1];
	wire dataOutAvailable = dataInReady | ~bitCount[_BIT_COUNT_WIDTH - 1];

	// The counter is incremented on each falling edge of the output clock and
	// cleared when the shift register is reloaded. The reset input sets the
	// counter to its final value rather than clearing it, as the shift register
	// will be empty.
	Counter #(
		.ID   ("MP3DataFeeder.bitCounter"),
		.WIDTH(_BIT_COUNT_WIDTH),
		.INIT (DATA_WIDTH)
	) bitCounter (
		.clk    (clk),
		.reset  (reset),
		.countEn(dataOutClk),

		.loadEn  (dataInAvailable),
		.valueIn ({_BIT_COUNT_WIDTH{1'b0}}),
		.valueOut(bitCount)
	);

	// Data is output MSB first to match the conventional MP3 file bit order.
	// The shift register is updated on each rising edge of the output clock
	// (and sampled by the decoder on the respective falling edge).
	assign dataOut      = shiftReg[DATA_WIDTH - 1];
	wire   nextClkState = ~dataOutClk & (dataOutAvailable & dataOutReq);

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			dataInAck  <= 1'b0;
			dataOutClk <= 1'b0;
		end else begin
			if (dataInAck) begin
				dataInAck <= 1'b0;
			end else if (dataInAvailable) begin
				dataInAck <= 1'b1;
				shiftReg  <= dataIn;
			end else if (nextClkState) begin
				// Note that both dataInAck and nextClkState will be high after
				// a reload, so the shift register will not start shifting until
				// the first bit is clocked out.
				dataInAck <= 1'b0;
				shiftReg  <= { shiftReg[DATA_WIDTH - 2:0], 1'b0 };
			end

			dataOutClk <= nextClkState;
		end
	end
endmodule

/* Descrambling module */

`define DESCRAMBLE_NONE    2'h0
`define DESCRAMBLE_DEFAULT 2'h1
`define DESCRAMBLE_DDRSBM  2'h2

module MP3Descrambler (
	input clk,
	input reset,

	input [1:0]  mode,
	input [15:0] keyValue,
	input        key1Load,
	input        key2Load,
	input        key3Load,

	input  [15:0] dataIn,
	output [15:0] dataOut,
	input         dataOutAck
);
	/* Key registers */

	reg  [15:0] key1;
	reg  [15:0] key2;
	wire [7:0]  key3;

	Counter #(
		.ID   ("MP3Descrambler.key3Counter"),
		.WIDTH(8)
	) key3Counter (
		.clk    (clk),
		.reset  (reset),
		.countEn(dataOutAck),

		.loadEn  (key3Load),
		.valueIn (keyValue[7:0]),
		.valueOut(key3)
	);

	reg [15:0] nextKey1;
	reg [15:0] nextKey2;

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			key1 <= 16'h0000;
			key2 <= 16'h0000;
		end else begin
			if (key1Load)
				key1 <= keyValue;
			else if (dataOutAck)
				key1 <= nextKey1;

			if (key2Load)
				key2 <= keyValue;
			else if (dataOutAck & (key1[14] ^ key1[15]))
				key2 <= nextKey2;
		end
	end

	/* Descrambler core */

	reg [15:0] derivedKey;
	reg [15:0] xorMask;

	function automatic [1:0] _decodePair (input [1:0] pair, input [1:0] key);
		// In both variants of the algorithm, a 16-bit key is derived from the
		// current state and treated as a set of 8 pairs of 2 bits. Bit 1 of
		// each pair controls whether the respective bit pair of the data is
		// swapped, while bit 0 controls whether the respective data bit is
		// inverted after performing the swap.
		_decodePair = {
			(key[1] ? pair[0] : pair[1]),
			(key[1] ? pair[1] : pair[0]) ^ key[0]
		};
	endfunction

	assign dataOut = xorMask ^ {
		_decodePair(dataIn[15:14], derivedKey[15:14]),
		_decodePair(dataIn[13:12], derivedKey[13:12]),
		_decodePair(dataIn[11:10], derivedKey[11:10]),
		_decodePair(dataIn[9:8],   derivedKey[9:8]),
		_decodePair(dataIn[7:6],   derivedKey[7:6]),
		_decodePair(dataIn[5:4],   derivedKey[5:4]),
		_decodePair(dataIn[3:2],   derivedKey[3:2]),
		_decodePair(dataIn[1:0],   derivedKey[1:0])
	};

	/* Key multiplexer */

	wire [15:0] key12 = key1 ^ key2;

	always @(*) begin
		case (mode)
			`DESCRAMBLE_NONE: begin
				derivedKey = 16'h0000;
				xorMask    = 16'h0000;
				nextKey1   = 16'hxxxx;
				nextKey2   = 16'hxxxx;
			end
			`DESCRAMBLE_DEFAULT: begin
				derivedKey = {
					key12[15], key12[13], key12[14], key12[12],
					key12[11], key12[10], key12[9],  key12[7],
					key12[8],  key12[6],  key12[5],  key12[4],
					key12[3],  key12[1],  key12[2],  key12[0]
				};
				xorMask    = {
					key3[7], key3[0], key3[6], key3[1],
					key3[5], key3[2], key3[4], key3[3],
					key3[3], key3[4], key3[2], key3[5],
					key3[1], key3[6], key3[0], key3[7]
				};
				nextKey1   = { key1[15], key1[13:0], key1[14] };
				nextKey2   = { key2[14:0], key2[15] };
			end
			`DESCRAMBLE_DDRSBM: begin
				derivedKey = key1;
				xorMask    = 16'h0000;
				nextKey1   = { key1[14:0], key1[15] };
				nextKey2   = 16'hxxxx;
			end
			default: begin
				derivedKey = 16'hxxxx;
				xorMask    = 16'hxxxx;
				nextKey1   = 16'hxxxx;
				nextKey2   = 16'hxxxx;
			end
		endcase
	end
endmodule

/* MP3 playback state machine */

module MP3StateMachine #(
	parameter FRAME_COUNT_WIDTH  = 16,
	parameter SAMPLE_COUNT_WIDTH = 16,
	parameter SAMPLE_DELTA_WIDTH = 16
) (
	input clk,
	input reset,

	input resetCountOnStart,
	input disableCountOnIdleReset,

	input enabledValue,
	input playingValue,
	input frameCountEnValue,
	input loadFlags,

	input forceStop,
	input sampleCountReset,
	input sampleDeltaReset,

	input mp3FrameSync,
	input mp3Error,
	input mp3SampleClk,

	output                            isBusy,
	output [FRAME_COUNT_WIDTH  - 1:0] frameCount,
	output [SAMPLE_COUNT_WIDTH - 1:0] sampleCount,
	output [SAMPLE_DELTA_WIDTH - 1:0] sampleDelta
);
	/* Control flags */

	reg isEnabled    = 1'b0;
	reg isPlaying    = 1'b0;
	reg frameCountEn = 1'b0;

	assign isBusy = isEnabled & isPlaying;

	wire willBeBusy     = enabledValue & playingValue;
	wire startedPlaying = loadFlags & (~isBusy & willBeBusy);

	always @(posedge clk) begin
		if (loadFlags) begin
			isEnabled    <= enabledValue;
			isPlaying    <= playingValue;
			frameCountEn <= frameCountEnValue;
		end else if (forceStop) begin
			isPlaying <= 1'b0;
		end
	end

	/* Counter control logic */

	// These signals can be disabled through configuration inputs, as the DDR
	// Solo Bass Mix bitstream does not implement them.
	wire forceDisableCount = sampleCountReset & disableCountOnIdleReset;
	wire forceResetCount   = startedPlaying   & resetCountOnStart;

	wire frameTick, sampleTick;

	EdgeDetector mp3FrameSyncDet (
		.clk  (clk),
		.reset(reset),

		.valueIn(mp3FrameSync),
		.rising (frameTick)
	);
	EdgeDetector mp3SampleClkDet (
		.clk  (clk),
		.reset(reset),

		// The I2S LRCK output is low while the left audio sample is being sent
		// and high while the right sample is being sent.
		.valueIn(mp3SampleClk),
		.falling(sampleTick)
	);

	reg sampleCountEn = 1'b0;

	always @(posedge clk) begin
		if (forceDisableCount | mp3Error)
			sampleCountEn <= 1'b0;
		else if (isBusy & mp3FrameSync)
			sampleCountEn <= 1'b1;
	end

	/* Frame and sample counters */

	wire frameCountInc  = isBusy & frameCountEn  & frameTick;
	wire sampleCountInc = isBusy & sampleCountEn & sampleTick;

	Counter #(
		.ID   ("MP3Counter.frameCountReg"),
		.WIDTH(FRAME_COUNT_WIDTH)
	) frameCountReg (
		// Disabling the frame counter also resets it to zero.
		.clk    (clk),
		.reset  (reset),
		.countEn(frameCountInc),
		.loadEn (~frameCountEn),

		.valueIn ({FRAME_COUNT_WIDTH{1'b0}}),
		.valueOut(frameCount)
	);
	Counter #(
		.ID   ("MP3Counter.sampleCountReg"),
		.WIDTH(SAMPLE_COUNT_WIDTH)
	) sampleCountReg (
		.clk    (clk),
		.reset  (reset),
		.countEn(sampleCountInc),
		.loadEn (forceResetCount | sampleCountReset),

		.valueIn ({SAMPLE_COUNT_WIDTH{1'b0}}),
		.valueOut(sampleCount)
	);
	Counter #(
		.ID   ("MP3Counter.sampleDeltaReg"),
		.WIDTH(SAMPLE_DELTA_WIDTH)
	) sampleDeltaReg (
		.clk    (clk),
		.reset  (reset),
		.countEn(sampleCountInc),
		.loadEn (forceResetCount | sampleCountReset | sampleDeltaReset),

		.valueIn ({SAMPLE_DELTA_WIDTH{1'b0}}),
		.valueOut(sampleDelta)
	);
endmodule
