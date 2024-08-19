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

/* Flip flops */

module FDCE (input D, input C, input CLR, input CE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b0;
	assign Q    = data;

	always @(posedge C, posedge CLR) begin
		if (CLR)
			data <= 1'b0;
		else if (CE)
			data <= D;
	end
`endif
endmodule

module FDPE (input D, input C, input PRE, input CE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b1;
	assign Q    = data;

	always @(posedge C, posedge PRE) begin
		if (PRE)
			data <= 1'b1;
		else if (CE)
			data <= D;
	end
`endif
endmodule

/* Latches */

module LDCE_1 (input D, input G, input CLR, input GE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b0;
	assign Q    = data;

	always @(*) begin
		if (CLR)
			data <= 1'b0;
		else if (GE)
			data <= D;
	end
`endif
endmodule

module LDPE_1 (input D, input G, input PRE, input GE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b1;
	assign Q    = data;

	always @(*) begin
		if (PRE)
			data <= 1'b1;
		else if (GE)
			data <= D;
	end
`endif
endmodule

/* Distributed LUT RAM */

module RAM16X1S #(parameter INIT = 16'h0000) (
	input A0,
	input A1,
	input A2,
	input A3,
	input D,
	input WCLK,
	input WE,

	output O
);
`ifndef SYNTHESIS
	wire [3:0]  addr = { A3, A2, A1, A0 };
	reg  [15:0] data = INIT;

	assign O = data[addr];

	always @(posedge WCLK) begin
		if (WE)
			data[addr] <= D;
	end
`endif
endmodule

module RAM16X1D #(parameter INIT = 16'h0000) (
	input A0,
	input A1,
	input A2,
	input A3,
	input DPRA0,
	input DPRA1,
	input DPRA2,
	input DPRA3,
	input D,
	input WCLK,
	input WE,

	output SPO,
	output DPO
);
`ifndef SYNTHESIS
	wire [3:0]  addr0 = { A3, A2, A1, A0 };
	wire [3:0]  addr1 = { DPRA3, DPRA2, DPRA1, DPRA0 };
	reg  [15:0] data  = INIT;

	assign SPO = data[addr0];
	assign DPO = data[addr1];

	always @(posedge WCLK) begin
		if (WE)
			data[addr0] <= D;
	end
`endif
endmodule

module RAM32X1S #(parameter INIT = 32'h00000000) (
	input A0,
	input A1,
	input A2,
	input A3,
	input A4,
	input D,
	input WCLK,
	input WE,

	output O
);
`ifndef SYNTHESIS
	wire [4:0]  addr = { A4, A3, A2, A1, A0 };
	reg  [31:0] data = INIT;

	assign O = data[addr];

	always @(posedge WCLK) begin
		if (WE)
			data[addr] <= D;
	end
`endif
endmodule
