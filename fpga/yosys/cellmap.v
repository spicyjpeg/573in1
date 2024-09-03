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

/* Flip flops and latches */

// The dfflibmap command does not currently support mapping memory cells with an
// enable input, so these have to be declared here rather than in spartanxl.lib.
module \$_DFFE_PP0P_ (input D, input C, input R, input E, output Q);
	FDCE _TECHMAP_REPLACE_ (.D(D), .C(C), .CLR(R), .CE(E), .Q(Q));
endmodule

module \$_DFFE_PP1P_ (input D, input C, input R, input E, output Q);
	FDPE _TECHMAP_REPLACE_ (.D(D), .C(C), .PRE(R), .CE(E), .Q(Q));
endmodule

module \$_DLATCH_PP0_ (input E, input R, input D, output Q);
	LDCE_1 _TECHMAP_REPLACE_ (.D(D), .G(E), .CLR(R), .GE(1'b1), .Q(Q));
endmodule

module \$_DLATCH_PP1_ (input E, input R, input D, output Q);
	LDPE_1 _TECHMAP_REPLACE_ (.D(D), .G(E), .PRE(R), .GE(1'b1), .Q(Q));
endmodule

/* Distributed LUT RAM */

module _RAM16X1S #(parameter INIT = 16'h0000) (
	input [3:0] PORT_0_ADDR,
	input       PORT_0_WR_DATA,
	input       PORT_0_WR_EN,
	input       PORT_0_CLK,

	output PORT_0_RD_DATA
);
	RAM16X1S #(.INIT(INIT)) _TECHMAP_REPLACE_ (
		.A0  (PORT_0_ADDR[0]),
		.A1  (PORT_0_ADDR[1]),
		.A2  (PORT_0_ADDR[2]),
		.A3  (PORT_0_ADDR[3]),
		.D   (PORT_0_WR_DATA),
		.WCLK(PORT_0_CLK),
		.WE  (PORT_0_WR_EN),

		.O(PORT_0_RD_DATA)
	);
endmodule

module _RAM16X1D #(parameter INIT = 16'h0000) (
	input [3:0] PORT_0_ADDR,
	input [3:0] PORT_1_ADDR,
	input       PORT_0_WR_DATA,
	input       PORT_0_WR_EN,
	input       PORT_0_CLK,

	output PORT_0_RD_DATA,
	output PORT_1_RD_DATA
);
	RAM16X1D #(.INIT(INIT)) _TECHMAP_REPLACE_ (
		.A0   (PORT_0_ADDR[0]),
		.A1   (PORT_0_ADDR[1]),
		.A2   (PORT_0_ADDR[2]),
		.A3   (PORT_0_ADDR[3]),
		.DPRA0(PORT_1_ADDR[0]),
		.DPRA1(PORT_1_ADDR[1]),
		.DPRA2(PORT_1_ADDR[2]),
		.DPRA3(PORT_1_ADDR[3]),
		.D    (PORT_0_WR_DATA),
		.WCLK (PORT_0_CLK),
		.WE   (PORT_0_WR_EN),

		.SPO(PORT_0_RD_DATA),
		.DPO(PORT_1_RD_DATA)
	);
endmodule

module _RAM32X1S #(parameter INIT = 32'h00000000) (
	input [4:0] PORT_0_ADDR,
	input       PORT_0_WR_DATA,
	input       PORT_0_WR_EN,
	input       PORT_0_CLK,

	output PORT_0_RD_DATA
);
	RAM32X1S #(.INIT(INIT)) _TECHMAP_REPLACE_ (
		.A0  (PORT_0_ADDR[0]),
		.A1  (PORT_0_ADDR[1]),
		.A2  (PORT_0_ADDR[2]),
		.A3  (PORT_0_ADDR[3]),
		.A4  (PORT_0_ADDR[4]),
		.D   (PORT_0_WR_DATA),
		.WCLK(PORT_0_CLK),
		.WE  (PORT_0_WR_EN),

		.O(PORT_0_RD_DATA)
	);
endmodule

/* I/O cells */

module \$_TBUF_ (input A, input E, output Y);
	wire T;

	INV  _TECHMAP_REPLACE_.inv  (.I(E), .O(T));
	BUFT _TECHMAP_REPLACE_.tbuf (.I(A), .T(T), .O(Y));
endmodule

module _OBUFE (input I, input E, output O);
	wire T;

	INV   _TECHMAP_REPLACE_.inv  (.I(E), .O(T));
	OBUFT _TECHMAP_REPLACE_.obuf (.I(I), .T(T), .O(O));
endmodule

module _IOBUFE (input I, input E, output O, inout IO);
	wire T;

	INV   _TECHMAP_REPLACE_.inv  (.I(E), .O(T));
	OBUFT _TECHMAP_REPLACE_.obuf (.I(I), .T(T), .O(IO));
	IBUF  _TECHMAP_REPLACE_.ibuf (.I(IO), .O(O));
endmodule
