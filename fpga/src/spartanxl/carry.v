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

/* Carry logic primitive */

(* keep *) module CY4 (
	input  CIN,
	output COUT0,
	output COUT,

	input A0,
	input A1,
	input B0,
	input B1,
	input ADD,

	input C0,
	input C1,
	input C2,
	input C3,
	input C4,
	input C5,
	input C6,
	input C7
);
`ifndef SYNTHESIS
	// Unconnected inputs shall be pulled down.
	wire cin = CIN ? 1'b1 : 1'b0;
	wire a0  = A0  ? 1'b1 : 1'b0;
	wire a1  = A1  ? 1'b1 : 1'b0;
	wire b0  = B0  ? 1'b1 : 1'b0;
	wire b1  = B1  ? 1'b1 : 1'b0;
	wire add = ADD ? 1'b1 : 1'b0;

	// The following code is based on the CY4 simulation model provided by
	// Xilinx, reverse engineered to make it (slightly) more readable. A
	// schematic of the carry unit is also present in the Spartan-XL datasheet.
	wire inv = C0 ? ADD : C1;
	wire p0  = a0 ^ (inv ^ (~C7 | ~b0));
	wire p1  = a1 ^ (inv ^ (~C7 | ~b1));

	wire mux0 = C3 ? p0 : C2;
	wire mux1 = C6 ? p1 : 1'b1;
	wire out0 =
		(~C5 & ~C4) ? C7   :
		( C5 & ~C4) ? ~add :
		( C5 &  C4) ? a0   :
		1'b0;

	assign COUT0 = mux0 ? CIN   : out0;
	assign COUT  = mux1 ? COUT0 : a1;
`endif
endmodule

/* Carry logic mode primitives */

module CY4_01 ( // ADD-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b10111010;
`endif
endmodule

module CY4_02 ( // ADD-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11111010;
`endif
endmodule

module CY4_03 ( // ADD-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11110010;
`endif
endmodule

module CY4_04 ( // ADD-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11000110;
`endif
endmodule

module CY4_05 ( // ADD-G-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11100010;
`endif
endmodule

module CY4_06 ( // SUB-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b10111000;
`endif
endmodule

module CY4_07 ( // SUB-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11111000;
`endif
endmodule

module CY4_08 ( // SUB-G-1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11000000;
`endif
endmodule

module CY4_09 ( // SUB-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11000100;
`endif
endmodule

module CY4_10 ( // SUB-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11110000;
`endif
endmodule

module CY4_11 ( // SUB-G-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11100000;
`endif
endmodule

module CY4_12 ( // ADDSUB-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b10111001;
`endif
endmodule

module CY4_13 ( // ADDSUB-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11111001;
`endif
endmodule

module CY4_14 ( // ADDSUB-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11110001;
`endif
endmodule

module CY4_15 ( // ADDSUB-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11000101;
`endif
endmodule

module CY4_16 ( // ADDSUB-G-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b11100001;
`endif
endmodule

module CY4_17 ( // INC-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00111010;
`endif
endmodule

module CY4_18 ( // INC-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01111010;
`endif
endmodule

module CY4_19 ( // INC-FG-1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110010;
`endif
endmodule

module CY4_20 ( // INC-G-1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000000;
`endif
endmodule

module CY4_21 ( // INC-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110010;
`endif
endmodule

module CY4_22 ( // INC-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000110;
`endif
endmodule

module CY4_23 ( // INC-G-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01100010;
`endif
endmodule

module CY4_24 ( // DEC-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00111000;
`endif
endmodule

module CY4_25 ( // DEC-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01111000;
`endif
endmodule

module CY4_26 ( // DEC-FG-0
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110000;
`endif
endmodule

module CY4_27 ( // DEC-G-0
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000000;
`endif
endmodule

module CY4_28 ( // DEC-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110000;
`endif
endmodule

module CY4_29 ( // DEC-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000100;
`endif
endmodule

module CY4_30 ( // DEC-G-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01100000;
`endif
endmodule

module CY4_31 ( // INCDEC-F-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00111001;
`endif
endmodule

module CY4_32 ( // INCDEC-FG-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01111001;
`endif
endmodule

module CY4_33 ( // INCDEC-FG-1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110001;
`endif
endmodule

module CY4_34 ( // INCDEC-G-0
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000000;
`endif
endmodule

module CY4_35 ( // INCDEC-G-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01110001;
`endif
endmodule

module CY4_36 ( // INCDEC-G-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000101;
`endif
endmodule

module CY4_37 ( // FORCE-0
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00000000;
`endif
endmodule

module CY4_38 ( // FORCE-1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b10000000;
`endif
endmodule

module CY4_39 ( // FORCE-F1
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00110000;
`endif
endmodule

module CY4_40 ( // FORCE-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00000100;
`endif
endmodule

module CY4_41 ( // FORCE-F3-
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00100000;
`endif
endmodule

module CY4_42 ( // EXAMINE-CI
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b00000100;
`endif
endmodule

module CY4_43 ( // FORCE-G4
	output C0, output C1, output C2, output C3,
	output C4, output C5, output C6, output C7
);
`ifndef SYNTHESIS
	assign { C7, C6, C5, C4, C3, C2, C1, C0 } = 8'b01000000;
`endif
endmodule
