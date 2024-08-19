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

/* Edge detectors and synchronizers */

module EdgeDetector (
	input clk,
	input reset,

	input  valueIn,
	output delayedOut,
	output stableLow,
	output stableHigh,
	output rising,
	output falling
);
	reg [1:0] state = 2'b00;

	assign delayedOut = state[1];
	assign stableLow  = (state == 2'b00);
	assign stableHigh = (state == 2'b11);
	assign rising     = (state == 2'b01);
	assign falling    = (state == 2'b10);

	always @(posedge clk, posedge reset) begin
		if (reset)
			state <= 2'b00;
		else
			state <= { state[0], valueIn };
	end
endmodule

module Synchronizer #(parameter STAGES = 2) (
	input clk,
	input reset,

	input  valueIn,
	output valueOut
);
	reg [STAGES - 1:0] state = {STAGES{1'b0}};

	assign valueOut = state[STAGES - 1];

	always @(posedge clk, posedge reset) begin
		if (reset)
			state <= {STAGES{1'b0}};
		else
			state <= { state[STAGES - 2:0], valueIn };
	end
endmodule

/* 573 to APB bridge */

module APBHostBridge #(
	parameter ADDR_WIDTH = 32,
	parameter DATA_WIDTH = 32,

	parameter VALID_ADDR_MASK    = 0,
	parameter VALID_ADDR_VALUE   = 0,
	parameter INVALID_ADDR_MASK  = 0,
	parameter INVALID_ADDR_VALUE = 1
) (
	input clk,
	input reset,

	input                    nHostCS,
	input                    nHostRead,
	input                    nHostWrite,
	input [ADDR_WIDTH - 1:0] hostAddr,
	inout [DATA_WIDTH - 1:0] hostData,

	output reg                    apbEnable = 1'b0,
	output reg                    apbWrite  = 1'b0,
	input                         apbReady,
	output reg [ADDR_WIDTH - 1:0] apbAddr,
	input      [DATA_WIDTH - 1:0] apbRData,
	output reg [DATA_WIDTH - 1:0] apbWData
);
	wire hostAddrValid   = 1'b1
		& ~nHostCS
		& ((hostAddr & VALID_ADDR_MASK)   == VALID_ADDR_VALUE)
		& ((hostAddr & INVALID_ADDR_MASK) != INVALID_ADDR_VALUE);
	wire hostReadStrobe  = hostAddrValid & ~nHostRead & nHostWrite;
	wire hostWriteStrobe = hostAddrValid &  nHostRead & ~nHostWrite;

	/* Read/write strobe edge detectors */

	wire hostReadAsserted, hostWriteDelayed, hostWriteReleased;

	EdgeDetector hostReadDet (
		.clk  (clk),
		.reset(reset),

		.valueIn(hostReadStrobe),
		.rising (hostReadAsserted)
	);
	EdgeDetector hostWriteDet (
		.clk  (clk),
		.reset(reset),

		.valueIn   (hostWriteStrobe),
		.delayedOut(hostWriteDelayed),
		.falling   (hostWriteReleased)
	);

	/* Address and data buffers */

	reg [DATA_WIDTH - 1:0] hostRData;
	reg                    hostDataDir = 1'b0;

	assign hostData = hostDataDir ? hostRData : {DATA_WIDTH{1'bz}};

	// Data written by the 573 must be latched as soon as possible once either
	// the write strobe or chip select is deasserted.
	always @(negedge hostWriteStrobe)
		apbWData <= hostData;

	/* State machine */

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			apbEnable   <= 1'b0;
			apbWrite    <= 1'b0;
			hostDataDir <= 1'b0;
		end else if (~apbEnable) begin
			// The address and write bit are not updated while a transaction is
			// in progress.
			apbEnable   <= hostReadAsserted | hostWriteReleased;
			apbWrite    <= hostWriteDelayed;
			apbAddr     <= hostAddr;
			hostDataDir <= hostReadStrobe & hostDataDir;
		end else if (apbReady) begin
			apbEnable   <= 1'b0;
			hostRData   <= apbRData;
			hostDataDir <= hostReadStrobe;
		end else begin
			hostDataDir <= 1'b0;
		end
	end
endmodule
