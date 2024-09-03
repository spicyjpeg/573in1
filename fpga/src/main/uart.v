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

module BaudRateGenerator #(
	parameter FIXED_STAGES = 0,
	parameter SEL_STAGES   = 8,

	parameter _NUM_STAGES = FIXED_STAGES + SEL_STAGES,
	parameter _SEL_WIDTH  = $clog2(SEL_STAGES)
) (
	input clk,
	input baudClk,
	input reset,

	input                    enable,
	input [_SEL_WIDTH - 1:0] baudRateSel,
	output                   update
);
	genvar i;

	/* Clock divider */

	wire                     dividerEn;
	wire [_NUM_STAGES - 1:0] dividerOut;

	// The divider and mux run in the baud rate clock domain, so a synchronizer
	// is needed to bridge the enable signal over.
	EdgeDetector enableSync (
		.clk  (baudClk),
		.reset(reset),
		.clkEn(1'b1),

		.valueIn (enable),
		.valueOut(dividerEn)
	);
	Counter #(
		.ID   ("BaudRateGenerator.divider"),
		.WIDTH(_NUM_STAGES)
	) divider (
		.clk  (baudClk),
		.reset(reset),
		.clkEn(1'b1),

		.load    ({_NUM_STAGES{~dividerEn}}),
		.valueIn ({_NUM_STAGES{1'b0}}),
		.valueOut(dividerOut)
	);

	/* Clock multiplexer */

	wor muxOut;

	generate
		for (i = 0; i < SEL_STAGES; i = i + 1) begin
			localparam regValue = (SEL_STAGES - 1) - i;

			CompareConst #(
				.WIDTH(_SEL_WIDTH + 1),
				.VALUE({ 1'b1, regValue[_SEL_WIDTH - 1:0] })
			) baudRateDec (
				.value ({ dividerOut[FIXED_STAGES + i], baudRateSel }),
				.result(muxOut)
			);
		end
	endgenerate

	// The first two flip flop stages serve as a synchronizer, while the third
	// one performs edge detection.
	EdgeDetector #(.STAGES(3)) updatePulseGen (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.valueIn(muxOut),
		.rising (update)
	);
endmodule

module UARTTransmitter #(
	parameter DATA_BITS = 8,
	parameter STOP_BITS = 1,

	parameter _SHIFT_REG_WIDTH = 1 + DATA_BITS,
	parameter _FRAME_WIDTH     = 1 + DATA_BITS + STOP_BITS,
	parameter _COUNTER_WIDTH   = $clog2(_FRAME_WIDTH + 1),
	parameter _COUNTER_RELOAD  = {_COUNTER_WIDTH{1'b1}} - _FRAME_WIDTH
) (
	input clk,
	input reset,
	input update,

	input                   fifoWrite,
	input [DATA_BITS - 1:0] fifoWData,
	output                  txIdle,
	output reg              txFull = 1'b0,

	output tx,
	input  txReq
);
	/* Cycle counter */

	wire startNewFrame =  txIdle & (txReq & txFull);
	wire counterInc    = ~txIdle;
	wire counterEn     = update  & (startNewFrame | counterInc);

	Counter #(
		.ID       ("UARTTransmitter.counter"),
		.WIDTH    (_COUNTER_WIDTH),
		.INIT     ({_COUNTER_WIDTH{1'b1}}),
		.CARRY_OUT(1)
	) counter (
		.clk  (clk),
		.reset(reset),
		.clkEn(counterEn),

		.load    ({_COUNTER_WIDTH{startNewFrame}}),
		.valueIn (_COUNTER_RELOAD[_COUNTER_WIDTH - 1:0]),
		.carryOut(txIdle)
	);

	/* Data input FIFO */

	wire [DATA_BITS - 1:0] fifoData;

	Register #(
		.ID   ("UARTTransmitter.fifo"),
		.WIDTH(DATA_BITS)
	) fifo (
		.clk  (clk),
		.reset(reset),
		.clkEn(fifoWrite & ~txFull),

		.valueIn (fifoWData),
		.valueOut(fifoData)
	);

	always @(posedge clk, posedge reset) begin
		if (reset)
			txFull <= 1'b0;
		else if (update & startNewFrame)
			txFull <= 1'b0;
		else if (fifoWrite)
			txFull <= 1'b1;
	end

	/* Shift register */

	wire [_SHIFT_REG_WIDTH - 1:0] shiftRegValue;

	assign tx = shiftRegValue[0];

	// Data is shifted out LSB first, preceded by a start bit (always cleared)
	// and followed by stop bits (always set).
	MultiplexerReg #(
		.ID   ("UARTTransmitter.shiftReg"),
		.WIDTH(_SHIFT_REG_WIDTH),
		.INIT ({_SHIFT_REG_WIDTH{1'b1}})
	) shiftReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(update),
		.sel  (startNewFrame),

		.valueA  ({ 1'b1, shiftRegValue[_SHIFT_REG_WIDTH - 1:1] }),
		.valueB  ({ fifoData, 1'b0 }),
		.valueOut(shiftRegValue)
	);
endmodule

module UARTReceiver #(
	parameter DATA_BITS = 8,
	parameter STOP_BITS = 1,

	parameter _SHIFT_REG_WIDTH = DATA_BITS + STOP_BITS,
	parameter _FRAME_WIDTH     = 1 + DATA_BITS + STOP_BITS,
	parameter _COUNTER_WIDTH   = $clog2(_FRAME_WIDTH) + 1,
	parameter _COUNTER_RELOAD  = {_COUNTER_WIDTH{1'b1}} + 1 - _FRAME_WIDTH
) (
	input clk,
	input reset,
	input update,

	input                    fifoRead,
	output [DATA_BITS - 1:0] fifoRData,
	output                   rxIdle,
	output reg               rxFull = 1'b0,

	input      clearErrors,
	output reg frameError = 1'b0,

	input rx
);
	/* Cycle counter */

	wire [_COUNTER_WIDTH - 1:0] counterValue;
	wire                        isLastCycle;

	// The counter is triggered by pulling RX low (i.e. by the start bit).
	wire startNewFrame =  rxIdle & ~rx;
	wire counterInc    = ~rxIdle;
	wire counterEn     = update & (startNewFrame | counterInc);

	assign rxIdle = ~counterValue[_COUNTER_WIDTH - 1];

	Counter #(
		.ID       ("UARTReceiver.counter"),
		.WIDTH    (_COUNTER_WIDTH),
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
		.carryOut(isLastCycle)
	);

	/* Shift register */

	wire [_SHIFT_REG_WIDTH - 1:0] shiftRegValue;

	// All stop bits must be set in order for the frame to be considered valid.
	wire stopBitsValid = &shiftRegValue[_SHIFT_REG_WIDTH - 1:DATA_BITS];

	MultiplexerReg #(
		.ID   ("UARTReceiver.shiftReg"),
		.WIDTH(_SHIFT_REG_WIDTH)
	) shiftReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(update),
		.sel  (isLastCycle),

		.valueA  ({ rx, shiftRegValue[_SHIFT_REG_WIDTH - 1:1] }),
		.valueB  (shiftRegValue),
		.valueOut(shiftRegValue)
	);

	/* Data output FIFO */

	wire dataValid   = isLastCycle &  stopBitsValid;
	wire dataInvalid = isLastCycle & ~stopBitsValid;

	Register #(
		.ID   ("UARTReceiver.fifo"),
		.WIDTH(DATA_BITS)
	) fifo (
		.clk  (clk),
		.reset(reset),
		.clkEn(dataValid),

		.valueIn (shiftRegValue[DATA_BITS - 1:0]),
		.valueOut(fifoRData)
	);

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			rxFull     <= 1'b0;
			frameError <= 1'b0;
		end else begin
			if (dataValid)
				rxFull <= 1'b1;
			else if (fifoRead)
				rxFull <= 1'b0;

			if (dataValid)
				frameError <= 1'b1;
			else if (clearErrors)
				frameError <= 1'b0;
		end
	end
endmodule

module UART #(
	parameter DATA_BITS = 8,
	parameter STOP_BITS = 1
) (
	input clk,
	input baudClk,
	input reset,

	input                    fifoRead,
	output [DATA_BITS - 1:0] fifoRData,
	input                    fifoWrite,
	input  [DATA_BITS - 1:0] fifoWData,

	input         ctrlRead,
	output [15:0] ctrlRData,
	input         ctrlWrite,
	input  [15:0] ctrlWData,

	output reg tx  = 1'b1,
	input      rx,
	output reg rts = 1'b1,
	input      cts,
	output reg dtr = 1'b1,
	input      dsr
);
	reg       enable      = 1'b0;
	reg [2:0] baudRateSel = 3'h0;
	reg       txFlowCtrl  = 1'b0;
	reg       rxFlowCtrl  = 1'b0;
	reg       rtsOut      = 1'b0;
	reg       dtrOut      = 1'b0;

	reg rxIn  = 1'b1;
	reg ctsIn = 1'b0;
	reg dsrIn = 1'b0;

	wire update, txOut;
	wire txIdle, rxIdle, txFull, rxFull, frameError;

	assign ctrlRData = {
		1'h0,
		frameError,
		rxFull,
		txFull,
		rxIdle,
		txIdle,
		dsrIn,
		dtrOut,
		ctsIn,
		rtsOut,
		rxFlowCtrl,
		txFlowCtrl,
		baudRateSel,
		enable
	};

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			enable      <= 1'b0;
			baudRateSel <= 3'h0;
			txFlowCtrl  <= 1'b0;
			rxFlowCtrl  <= 1'b0;
			rtsOut      <= 1'b0;
			dtrOut      <= 1'b0;

			tx    <= 1'b1;
			rxIn  <= 1'b1;
			rts   <= 1'b1;
			ctsIn <= 1'b0;
			dtr   <= 1'b1;
			dsrIn <= 1'b0;
		end else begin
			if (ctrlWrite) begin
				enable      <= ctrlWData[0];
				txFlowCtrl  <= ctrlWData[4];
				rxFlowCtrl  <= ctrlWData[5];
				rtsOut      <= ctrlWData[6];
				dtrOut      <= ctrlWData[8];

				// In order to prevent glitches, baud rate changes are ignored
				// when the baud rate generator is enabled.
				if (~enable)
					baudRateSel <= ctrlWData[3:1];
			end

			tx    <= txOut;
			rxIn  <= rx;
			rts   <= rxFlowCtrl ? rxFull : ~rtsOut;
			ctsIn <= ~cts;
			dtr   <= ~dtrOut;
			dsrIn <= ~dsr;
		end
	end

	BaudRateGenerator #(
		.FIXED_STAGES(4), // Maximum baud rate: 19660800 >> (4 + 1) = 614400
		.SEL_STAGES  (8)  // Minimum baud rate: 19660800 >> (4 + 8) =   4800
	) baudRateGen (
		.clk    (clk),
		.baudClk(baudClk),
		.reset  (reset),

		.enable     (enable),
		.baudRateSel(baudRateSel),
		.update     (update)
	);
	UARTTransmitter #(
		.DATA_BITS(DATA_BITS),
		.STOP_BITS(STOP_BITS)
	) txUnit (
		.clk   (clk),
		.reset (reset),
		.update(update),

		.fifoWrite(fifoWrite),
		.fifoWData(fifoWData),
		.txIdle   (txIdle),
		.txFull   (txFull),

		.tx   (txOut),
		.txReq(uartCTSIn | ~txFlowCtrl)
	);
	UARTReceiver #(
		.DATA_BITS(DATA_BITS),
		.STOP_BITS(STOP_BITS)
	) rxUnit (
		.clk   (clk),
		.reset (reset),
		.update(update),

		.fifoRead (fifoRead),
		.fifoRData(fifoRData),
		.rxIdle   (rxIdle),
		.rxFull   (rxFull),

		.clearErrors(ctrlRead),
		.frameError (frameError),

		.rx(rxIn)
	);
endmodule
