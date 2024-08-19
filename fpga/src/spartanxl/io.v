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

/* I/O pads */

(* keep *) module IPAD (output IPAD);
endmodule

(* keep *) module OPAD (input OPAD);
endmodule

(* keep *) module IOPAD (inout IOPAD);
endmodule

(* keep *) module UPAD (inout UPAD);
endmodule

(* keep *) module TDI (inout I);
endmodule

(* keep *) module TDO (input O);
endmodule

(* keep *) module TCK (inout I);
endmodule

(* keep *) module TMS (inout I);
endmodule

/* I/O buffers */

module BUFGLS (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module IBUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module OBUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module OBUFT (input I, input T, output O);
`ifndef SYNTHESIS
	assign O = T ? 1'bz : I;
`endif
endmodule

/* I/O flip flops */

module IFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b0;
	assign Q    = data;

	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module IFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data = 1'b1;
	assign Q    = data;

	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module OFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDX ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDXI ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDTX (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire   Q;
	assign O = T ? 1'bz : Q;

	IFDX ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDTXI (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire   Q;
	assign O = T ? 1'bz : Q;

	IFDXI ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

/* Fixed-function blocks */

(* keep *) module STARTUP (
	input GSR,
	input GTS,
	input CLK,

	output DONEIN,
	output Q1Q4,
	output Q2,
	output Q3
);
`ifndef SYNTHESIS
	// TODO: propagate GSR and GTS to all other cells
	assign DONEIN = 1'b1;
	assign Q1Q4   = 1'b1;
	assign Q2     = 1'b1;
	assign Q3     = 1'b1;
`endif
endmodule

(* keep *) module OSC4 (
	output F8M,
	output F500K,
	output F16K,
	output F490,
	output F15
);
`ifndef SYNTHESIS
	// TODO: actually simulate the clocks
	assign F8M   = 1'b0;
	assign F500K = 1'b0;
	assign F16K  = 1'b0;
	assign F490  = 1'b0;
	assign F15   = 1'b0;
`endif
endmodule

(* keep *) module BSCAN(
	input TDI,
	input TDO1,
	input TDO2,
	input TCK,
	input TMS,

	output TDO,
	output DRCK,
	output IDLE,
	output SEL1,
	output SEL2
);
endmodule

(* keep *) module RDBK (input TRIG, output DATA, output RIP);
endmodule

(* keep *) module RDCLK (input I);
endmodule
