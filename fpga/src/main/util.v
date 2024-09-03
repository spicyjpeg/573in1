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

module EdgeDetector #(parameter STAGES = 2) (
	input clk,
	input reset,
	input clkEn,

	input  valueIn,
	output valueOut,
	output stableLow,
	output stableHigh,
	output rising,
	output falling
);
	reg [STAGES - 1:0] state = {STAGES{1'b0}};

	assign valueOut   = state[STAGES - 1];
	assign stableLow  = (state[STAGES - 1:STAGES - 2] == 2'b00);
	assign stableHigh = (state[STAGES - 1:STAGES - 2] == 2'b11);
	assign rising     = (state[STAGES - 1:STAGES - 2] == 2'b01);
	assign falling    = (state[STAGES - 1:STAGES - 2] == 2'b10);

	always @(posedge clk, posedge reset) begin
		if (reset)
			state <= {STAGES{1'b0}};
		else if (clkEn)
			state <= { state[STAGES - 2:0], valueIn };
	end
endmodule

module APBHostBridge #(
	parameter ADDR_WIDTH = 32,
	parameter DATA_WIDTH = 32,

	parameter VALID_ADDR_MASK    = 0,
	parameter VALID_ADDR_VALUE   = 0,
	parameter INVALID_ADDR_MASK  = 0,
	parameter INVALID_ADDR_VALUE = 1
) (
	input clk,

	input                    nHostCS,
	input                    nHostRead,
	input                    nHostWrite,
	input [ADDR_WIDTH - 1:0] hostAddr,
	inout [DATA_WIDTH - 1:0] hostData,

	output                        apbEnable,
	output                        apbWrite,
	input                         apbReady,
	output     [ADDR_WIDTH - 1:0] apbAddr,
	input      [DATA_WIDTH - 1:0] apbRData,
	output reg [DATA_WIDTH - 1:0] apbWData
);
	wire addrValid   = 1'b1
		& ~nHostCS
		& ((hostAddr & VALID_ADDR_MASK)   == VALID_ADDR_VALUE)
		& ((hostAddr & INVALID_ADDR_MASK) != INVALID_ADDR_VALUE);
	wire readStrobe  = addrValid & ~nHostRead & nHostWrite;
	wire writeStrobe = addrValid &  nHostRead & ~nHostWrite;

	/* Read/write strobe edge detectors */

	wire readTrigger, writeTrigger;

	wire updateState = ~apbEnable | apbReady;
	wire dataReady   =  apbEnable & apbReady;

	assign apbEnable = readTrigger | writeTrigger;
	assign apbWrite  = writeTrigger;

	EdgeDetector hostReadDet (
		.clk  (clk),
		.reset(1'b0),
		.clkEn(updateState),

		.valueIn(readStrobe),
		.rising (readTrigger)
	);
	EdgeDetector hostWriteDet (
		.clk  (clk),
		.reset(1'b0),
		.clkEn(updateState),

		.valueIn(writeStrobe),
		.falling(writeTrigger)
	);

	/* Data buffers */

	reg [DATA_WIDTH - 1:0] hostRData;
	reg                    hostDataDir = 1'b0;

	assign apbAddr  = hostAddr;
	assign hostData = hostDataDir ? hostRData : {DATA_WIDTH{1'bz}};

	// Using separate clocks for apbRData and apbWData allows ISE to pack both
	// into IOB flip flops (as the input and output flip flops of each IOB must
	// share the same clock enable, but can have different clocks).
	always @(negedge writeStrobe)
		apbWData <= hostData;

	always @(posedge clk) begin
		hostDataDir <= readStrobe & (hostDataDir | dataReady);

		if (dataReady)
			hostRData <= apbRData;
	end
endmodule
