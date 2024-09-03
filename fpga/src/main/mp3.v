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

module MP3DataFeeder #(
	parameter DATA_BITS      = 8,
	parameter BIT_CLK_PERIOD = 2,

	parameter _BAUD_GEN_WIDTH = $clog2(BIT_CLK_PERIOD) - 1,
	parameter _COUNTER_WIDTH  = $clog2(DATA_BITS) + 2,
	parameter _COUNTER_RELOAD = {_COUNTER_WIDTH{1'b1}} - (DATA_BITS * 2)
) (
	input clk,
	input reset,

	input [DATA_BITS - 1:0] dataIn,
	input                   dataReady,
	output                  outIdle,
	output reg              dataAck = 1'b0,

	output reg dataOut    = 1'b0,
	output reg dataOutClk = 1'b0,
	input      dataOutReq
);
	/* Baud rate generator */

	wire update;

	Counter #(
		.ID       ("MP3DataFeeder.baudRateGen"),
		.WIDTH    (_BAUD_GEN_WIDTH),
		.CARRY_OUT(1)
	) baudRateGen (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.load    ({_BAUD_GEN_WIDTH{1'b0}}),
		.valueIn ({_BAUD_GEN_WIDTH{1'b0}}),
		.carryOut(update)
	);

	/* Cycle counter */

	wire [_COUNTER_WIDTH - 1:0] counterValue;

	// The LSB of the counter is used to generate the bit clock.
	wire bitClk = ~outIdle & counterValue[0];

	wire startNewFrame = outIdle & (dataOutReq    & dataReady);
	wire counterInc    = bitClk  | (dataOutReq    & ~outIdle);
	wire counterEn     = update  & (startNewFrame | counterInc);

	Counter #(
		.ID       ("MP3DataFeeder.counter"),
		.WIDTH    (_COUNTER_WIDTH),
		.INIT     ({_COUNTER_WIDTH{1'b1}}),
		.CARRY_OUT(1)
	) counter (
		.clk  (clk),
		.reset(reset),
		.clkEn(counterEn),

		// Reloading the shift register will also clear the counter and set the
		// data validity flag.
		.load    ({_COUNTER_WIDTH{startNewFrame}}),
		.valueIn (_COUNTER_RELOAD[_COUNTER_WIDTH - 1:0]),
		.valueOut(counterValue),
		.carryOut(outIdle)
	);

	always @(posedge clk, posedge reset) begin
		if (reset)
			dataAck <= 1'b0;
		else
			dataAck <= update & startNewFrame;
	end

	/* Shift register */

	wire [DATA_BITS - 1:0] shiftRegValue;

	wire shiftRegUpdate = dataOutReq & ~bitClk;
	wire shiftRegEn     = update     & (startNewFrame | shiftRegUpdate);

	// Data is output MSB first to match the conventional MP3 file bit order.
	// The shift register is updated on each rising edge of the bit clock (and
	// sampled by the decoder on the respective falling edge).
	MultiplexerReg #(
		.ID   ("MP3DataFeeder.shiftReg"),
		.WIDTH(DATA_BITS)
	) shiftReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(shiftRegEn),
		.sel  (startNewFrame),

		.valueA  ({ shiftRegValue[DATA_BITS - 2:0], 1'b0 }),
		.valueB  (dataIn),
		.valueOut(shiftRegValue)
	);

	// The serial output (particularly the bit clock) must be behind flip flops
	// in order to prevent glitches. I've learned this the hard way.
	always @(posedge clk) begin
		dataOut    <= shiftRegValue[DATA_BITS - 1];
		dataOutClk <= bitClk;
	end
endmodule

module MP3Descrambler (
	input       clk,
	input       reset,
	input [3:0] cfg,

	input        key1Write,
	input        key2Write,
	input        key3Write,
	input [15:0] keyWData,

	input  [15:0] dataIn,
	output [15:0] dataOut,
	input         dataAck
);
	wire useKey2        = cfg[0];
	wire useKey3        = cfg[1];
	wire byteSwapOutput = cfg[2];
	wire swapKey1MSBs   = cfg[3];

	/* Key registers */

	wire [15:0] key1;
	wire [15:0] key2;
	wire [7:0]  key3;

	wire key1Update = dataAck;
	wire key2Update = dataAck & (key1[14] ^ key1[15]);
	wire key3Update = dataAck & useKey3;

	wire key1Bit14 = swapKey1MSBs ? key1[15] : key1[14];
	wire key1Bit15 = swapKey1MSBs ? key1[14] : key1[15];

	MultiplexerReg #(
		.ID   ("MP3Descrambler.key1Reg"),
		.WIDTH(16)
	) key1Reg (
		.clk  (clk),
		.reset(reset),
		.clkEn(key1Write | key1Update),
		.sel  (key1Write),

		.valueA  ({ key1Bit14, key1[13:0], key1Bit15 }),
		.valueB  (keyWData),
		.valueOut(key1)
	);
	MultiplexerReg #(
		.ID   ("MP3Descrambler.key2Reg"),
		.WIDTH(16)
	) key2Reg (
		.clk  (clk),
		.reset(reset),
		.clkEn(key2Write | key2Update),
		.sel  (key2Write),

		.valueA  ({ key2[14:0], key2[15] }),
		.valueB  (keyWData),
		.valueOut(key2)
	);
	Counter #(
		.ID   ("MP3Descrambler.key3Reg"),
		.WIDTH(8)
	) key3Reg (
		.clk  (clk),
		.reset(reset),
		.clkEn(key3Write | key3Update),

		.load    ({8{key3Write}}),
		.valueIn (keyWData[7:0]),
		.valueOut(key3)
	);

	/* Key multiplexer */

	wire [15:0] derivedKey;

	XORMultiplexer #(
		.ID   ("MP3Descrambler.derivedKeyMux"),
		.WIDTH(16)
	) derivedKeyMux (
		.sel(useKey2),

		.valueA  (key1),
		.valueB0 ({
			key1[15], key1[13], key1[14], key1[12],
			key1[11], key1[10], key1[9],  key1[7],
			key1[8],  key1[6],  key1[5],  key1[4],
			key1[3],  key1[1],  key1[2],  key1[0]
		}),
		.valueB1 ({
			key2[15], key2[13], key2[14], key2[12],
			key2[11], key2[10], key2[9],  key2[7],
			key2[8],  key2[6],  key2[5],  key2[4],
			key2[3],  key2[1],  key2[2],  key2[0]
		}),
		.valueOut(derivedKey)
	);

	/* Descrambler core */

	// The key calculated above is treated as a set of 8 pairs of 2 bits. Bit 1
	// of each pair controls whether the respective bit pair of the data is
	// swapped, while bit 0 controls whether the respective data bit is inverted
	// after performing the swap.
	wire [15:0] swappedData;

	BitSwapper #(
		.ID   ("MP3Descrambler.swapper"),
		.WIDTH(16)
	) swapper (
		.swap  ({
			derivedKey[15], derivedKey[13], derivedKey[11], derivedKey[9],
			derivedKey[7],  derivedKey[5],  derivedKey[3],  derivedKey[1]
		}),
		.invert(derivedKey & 16'h5555),

		.valueIn (dataIn),
		.valueOut(swappedData)
	);

	// In the default variant, the value is additionally XOR's with a scrambled
	// copy of the key counter.
	wire [15:0] unscrambledData;

	XORMultiplexer #(
		.ID   ("MP3Descrambler.xorMux"),
		.WIDTH(16)
	) xorMux (
		.sel(useKey3),

		.valueA  (swappedData),
		.valueB0 (swappedData),
		.valueB1 ({
			key3[7], key3[0], key3[6], key3[1],
			key3[5], key3[2], key3[4], key3[3],
			key3[3], key3[4], key3[2], key3[5],
			key3[1], key3[6], key3[0], key3[7]
		}),
		.valueOut(unscrambledData)
	);

	// Byte swapping might be required in order to play unscrambled MP3 files,
	// as the decoder expects MP3 data to be shifted in MSB first but the 573
	// may write each 16-bit word in little endian format.
	Multiplexer #(
		.ID   ("MP3Descrambler.byteSwapMux"),
		.WIDTH(16)
	) byteSwapMux (
		.sel(byteSwapOutput),

		.valueA  (unscrambledData),
		.valueB  ({ unscrambledData[7:0], unscrambledData[15:8] }),
		.valueOut(dataOut)
	);
endmodule

module MP3StateMachine #(
	parameter ADDR_H_WIDTH         = 16,
	parameter ADDR_L_WIDTH         = 16,
	parameter FRAME_COUNT_WIDTH    = 16,
	parameter SAMPLE_COUNT_H_WIDTH = 16,
	parameter SAMPLE_COUNT_L_WIDTH = 16,
	parameter SAMPLE_DELTA_WIDTH   = 16,

	parameter _ADDR_WIDTH         = ADDR_H_WIDTH + ADDR_L_WIDTH,
	parameter _SAMPLE_COUNT_WIDTH = SAMPLE_COUNT_H_WIDTH + SAMPLE_COUNT_L_WIDTH
) (
	input       clk,
	input       reset,
	input [3:0] cfg,

	output reg  enabled = 1'b0,
	output reg  playing = 1'b0,
	input       ctrlWrite,
	input [2:0] ctrlWData,

	input                     startAddrHWrite,
	input                     startAddrLWrite,
	input                     endAddrHWrite,
	input                     endAddrLWrite,
	input [_ADDR_WIDTH - 1:0] addrWData,

	output                     feederAddrWrite,
	input                      feederReady,
	input  [_ADDR_WIDTH - 1:0] feederAddr,
	output [_ADDR_WIDTH - 1:0] feederAddrWData,

	input                              sampleCountReset,
	input                              sampleDeltaReset,
	input                              sampleDeltaRead,
	output [FRAME_COUNT_WIDTH   - 1:0] frameCount,
	output [_SAMPLE_COUNT_WIDTH - 1:0] sampleCount,
	output [SAMPLE_DELTA_WIDTH  - 1:0] sampleDelta,

	input mp3FrameSync,
	input mp3Error,
	input mp3SampleClk
);
	wire loopOnEnd               = cfg[0];
	wire resetCountOnStart       = cfg[1];
	wire resetCountLOnDeltaRead  = cfg[2];
	wire disableCountOnIdleReset = cfg[3];

	/* Start and end address registers */

	wire [_ADDR_WIDTH - 1:0] reachedEnd;

	wire startLoop    = (&reachedEnd) &  loopOnEnd;
	wire stopPlayback = (&reachedEnd) & ~loopOnEnd;

	Register #(
		.ID   ("MP3StateMachine.startAddrHReg"),
		.WIDTH(ADDR_H_WIDTH)
	) startAddrHReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(startAddrHWrite),

		.valueIn (addrWData      [_ADDR_WIDTH - 1:ADDR_L_WIDTH]),
		.valueOut(feederAddrWData[_ADDR_WIDTH - 1:ADDR_L_WIDTH])
	);
	Register #(
		.ID   ("MP3StateMachine.startAddrLReg"),
		.WIDTH(ADDR_L_WIDTH)
	) startAddrLReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(startAddrLWrite),

		.valueIn (addrWData      [ADDR_L_WIDTH - 1:0]),
		.valueOut(feederAddrWData[ADDR_L_WIDTH - 1:0])
	);
	CompareReg #(
		.ID   ("MP3StateMachine.endAddrHReg"),
		.WIDTH(ADDR_H_WIDTH)
	) endAddrHReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(endAddrHWrite),

		.regValueIn(addrWData [_ADDR_WIDTH - 1:ADDR_L_WIDTH]),
		.valueIn   (feederAddr[_ADDR_WIDTH - 1:ADDR_L_WIDTH]),
		.valueOut  (reachedEnd[_ADDR_WIDTH - 1:ADDR_L_WIDTH])
	);
	CompareReg #(
		.ID   ("MP3StateMachine.endAddrLReg"),
		.WIDTH(ADDR_L_WIDTH)
	) endAddrLReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(endAddrLWrite),

		.regValueIn(addrWData [ADDR_L_WIDTH - 1:0]),
		.valueIn   (feederAddr[ADDR_L_WIDTH - 1:0]),
		.valueOut  (reachedEnd[ADDR_L_WIDTH - 1:0])
	);

	/* State machine */

	reg frameCountEn     = 1'b0;
	reg sampleCountEn    = 1'b0;
	reg addrWritePending = 1'b0;

	wire seekToStartAddr    = startAddrHWrite | startAddrLWrite | startLoop;
	wire disableSampleCount =
		~playing & sampleCountReset & disableCountOnIdleReset;

	assign feederAddrWrite = addrWritePending & feederReady;

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			enabled <= 1'b0;
			playing <= 1'b0;

			frameCountEn     <= 1'b0;
			sampleCountEn    <= 1'b0;
			addrWritePending <= 1'b0;
		end else begin
			if (ctrlWrite) begin
				enabled      <= ctrlWData[0];
				playing      <= ctrlWData[1];
				frameCountEn <= ctrlWData[2];
			end else if (stopPlayback) begin
				playing      <= 1'b0;
			end

			if (mp3Error | disableSampleCount)
				sampleCountEn <= 1'b0;
			else if (playing & mp3FrameSync)
				sampleCountEn <= 1'b1;

			// The feeder is instructed to seek to the start address after a new
			// address is set or when looping.
			if (seekToStartAddr)
				addrWritePending <= 1'b1;
			else if (feederReady)
				addrWritePending <= 1'b0;
		end
	end

	/* Frame and sample detectors */

	wire frameTick, sampleTick;

	EdgeDetector mp3FrameSyncDet (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.valueIn(mp3FrameSync),
		.rising (frameTick)
	);
	EdgeDetector mp3SampleClkDet (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		// The I2S LRCK output is low while the left audio sample is being sent
		// and high while the right sample is being sent.
		.valueIn(mp3SampleClk),
		.falling(sampleTick)
	);

	/* Frame and sample counters */

	wire sampleCountCarry;
	wire sampleCountHClear =
		sampleCountReset  | (seekToStartAddr & resetCountOnStart);
	wire sampleCountLClear =
		sampleCountHClear | (sampleDeltaRead & resetCountLOnDeltaRead);
	wire sampleDeltaClear  = sampleCountHClear | sampleDeltaReset;

	Counter #(
		.ID      ("MP3StateMachine.frameCountReg"),
		.WIDTH   (FRAME_COUNT_WIDTH),
		.CARRY_IN(1)
	) frameCountReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		// Disabling the frame counter also resets it to zero.
		.carryIn (frameTick),
		.load    ({FRAME_COUNT_WIDTH{~frameCountEn}}),
		.valueIn ({FRAME_COUNT_WIDTH{1'b0}}),
		.valueOut(frameCount)
	);
	Counter #(
		.ID      ("MP3StateMachine.sampleCountHReg"),
		.WIDTH   (SAMPLE_COUNT_H_WIDTH),
		.CARRY_IN(1)
	) sampleCountHReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(sampleCountEn | sampleCountHClear),

		.carryIn (sampleCountCarry),
		.load    ({SAMPLE_COUNT_H_WIDTH{sampleCountHClear}}),
		.valueIn ({SAMPLE_COUNT_H_WIDTH{1'b0}}),
		.valueOut(sampleCount[_SAMPLE_COUNT_WIDTH - 1:SAMPLE_COUNT_L_WIDTH])
	);
	Counter #(
		.ID       ("MP3StateMachine.sampleCountLReg"),
		.WIDTH    (SAMPLE_COUNT_L_WIDTH),
		.CARRY_IN (1),
		.CARRY_OUT(1)
	) sampleCountLReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(sampleCountEn | sampleCountLClear),

		.carryIn (sampleTick),
		.load    ({SAMPLE_COUNT_L_WIDTH{sampleCountLClear}}),
		.valueIn ({SAMPLE_COUNT_L_WIDTH{1'b0}}),
		.valueOut(sampleCount[SAMPLE_COUNT_L_WIDTH - 1:0]),
		.carryOut(sampleCountCarry)
	);
	Counter #(
		.ID      ("MP3StateMachine.sampleDeltaReg"),
		.WIDTH   (SAMPLE_DELTA_WIDTH),
		.CARRY_IN(1)
	) sampleDeltaReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(sampleCountEn | sampleDeltaClear),

		.carryIn (sampleTick),
		.load    ({SAMPLE_DELTA_WIDTH{sampleDeltaClear}}),
		.valueIn ({SAMPLE_DELTA_WIDTH{1'b0}}),
		.valueOut(sampleDelta)
	);
endmodule
