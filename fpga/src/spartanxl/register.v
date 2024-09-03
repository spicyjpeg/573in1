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

module RegisterBit #(
	parameter ID      = "",
	parameter FF_RLOC = "",
	parameter INIT    = 0
) (
	input clk,
	input reset,
	input clkEn,

	input  valueIn,
	output valueOut
);
	generate
		if (INIT)
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDPE ff (.D(valueIn), .C(clk), .PRE(reset), .CE(clkEn), .Q(valueOut));
		else
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDCE ff (.D(valueIn), .C(clk), .CLR(reset), .CE(clkEn), .Q(valueOut));
	endgenerate
endmodule

module CompareRegBit #(
	parameter ID       = "",
	parameter LUT_RLOC = "",
	parameter FF_RLOC  = "",
	parameter INIT     = 0
) (
	input clk,
	input reset,
	input clkEn,

	input  invert,
	input  regValueIn,
	input  valueIn,
	output valueOut
);
	wire regValue;

	//assign valueOut = invert ^ ~(regValue ^ valueIn);
	XNOR3 comp (.I0(invert), .I1(regValue), .I2(valueIn), .O(valueOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (.I1(invert), .I2(regValue), .I3(valueIn), .O(valueOut));

	generate
		if (INIT)
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDPE ff (
				.D(regValueIn), .C(clk), .PRE(reset), .CE(clkEn), .Q(regValue)
			);
		else
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDCE ff (
				.D(regValueIn), .C(clk), .CLR(reset), .CE(clkEn), .Q(regValue)
			);
	endgenerate
endmodule

module MultiplexerRegBit #(
	parameter ID       = "",
	parameter LUT_RLOC = "",
	parameter FF_RLOC  = "",
	parameter INIT     = 0
) (
	input clk,
	input reset,
	input clkEn,

	input  sel,
	input  invert,
	input  valueA,
	input  valueB,
	output valueOut
);
	//assign lutOut = invert ^ (sel ? valueB : valueA);
	wire mux0Out, mux1Out, muxOut, lutOut;

	AND2B1 mux0 (.I0(sel),     .I1(valueA),  .O(mux0Out));
	AND2   mux1 (.I0(sel),     .I1(valueB),  .O(mux1Out));
	OR2    mux  (.I0(mux0Out), .I1(mux1Out), .O(muxOut));
	XOR2   inv  (.I0(muxOut),  .I1(invert),  .O(lutOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (.I1(sel), .I2(invert), .I3(valueA), .I4(valueB), .O(lutOut));

	generate
		if (INIT)
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDPE ff (.D(lutOut), .C(clk), .PRE(reset), .CE(clkEn), .Q(valueOut));
		else
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDCE ff (.D(lutOut), .C(clk), .CLR(reset), .CE(clkEn), .Q(valueOut));
	endgenerate
endmodule

module Register #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter INIT     =  0,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1
) (
	input clk,
	input reset,
	input clkEn,

	input  [WIDTH - 1:0] valueIn,
	output [WIDTH - 1:0] valueOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;
	localparam X_OFFSET = X_ORIGIN + ((X_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	localparam Y_OFFSET = Y_ORIGIN + ((Y_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	genvar     i;

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam cx = X_OFFSET + X_STRIDE * i;
			localparam cy = Y_OFFSET + Y_STRIDE * i;

			// iverilog does not support inlining $sformatf() like this, so the
			// RLOC constraints must be removed when targeting the testbench.
			RegisterBit #(
`ifdef SYNTHESIS
				.ID     (ID),
				.FF_RLOC($sformatf("R%0dC%0d.FFX", cy, cx)),
`endif
				.INIT   (INIT[i0])
			) bit0 (
				.clk  (clk),
				.reset(reset),
				.clkEn(clkEn),

				.valueIn (valueIn [i0]),
				.valueOut(valueOut[i0])
			);

			if (i1 < WIDTH)
				RegisterBit #(
`ifdef SYNTHESIS
					.ID     (ID),
					.FF_RLOC($sformatf("R%0dC%0d.FFY", cy, cx)),
`endif
					.INIT   (INIT[i1])
				) bit1 (
					.clk  (clk),
					.reset(reset),
					.clkEn(clkEn),

					.valueIn (valueIn [i1]),
					.valueOut(valueOut[i1])
				);
		end
	endgenerate
endmodule

module CompareReg #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter INIT     =  0,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1
) (
	input clk,
	input reset,
	input clkEn,

	input  [WIDTH - 1:0] regValueIn,
	input  [WIDTH - 1:0] valueIn,
	output [WIDTH - 1:0] valueOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;
	localparam X_OFFSET = X_ORIGIN + ((X_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	localparam Y_OFFSET = Y_ORIGIN + ((Y_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	genvar     i;

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam cx = X_OFFSET + X_STRIDE * i;
			localparam cy = Y_OFFSET + Y_STRIDE * i;

			CompareRegBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.F",   cy, cx)),
				.FF_RLOC ($sformatf("R%0dC%0d.FFX", cy, cx)),
`endif
				.INIT    (INIT[i0])
			) bit0 (
				.clk  (clk),
				.reset(reset),
				.clkEn(clkEn),

				.invert    (1'b0),
				.regValueIn(regValueIn[i0]),
				.valueIn   (valueIn   [i0]),
				.valueOut  (valueOut  [i0])
			);

			if (i1 < WIDTH)
				CompareRegBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC%0d.G",   cy, cx)),
					.FF_RLOC ($sformatf("R%0dC%0d.FFY", cy, cx)),
`endif
					.INIT    (INIT[i1])
				) bit1 (
					.clk  (clk),
					.reset(reset),
					.clkEn(clkEn),

					.invert    (1'b0),
					.regValueIn(regValueIn[i1]),
					.valueIn   (valueIn   [i1]),
					.valueOut  (valueOut  [i1])
				);
		end
	endgenerate
endmodule

module MultiplexerReg #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter INIT     =  0,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1
) (
	input clk,
	input reset,
	input clkEn,
	input sel,

	input  [WIDTH - 1:0] valueA,
	input  [WIDTH - 1:0] valueB,
	output [WIDTH - 1:0] valueOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;
	localparam X_OFFSET = X_ORIGIN + ((X_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	localparam Y_OFFSET = Y_ORIGIN + ((Y_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	genvar     i;

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam cx = X_OFFSET + X_STRIDE * i;
			localparam cy = Y_OFFSET + Y_STRIDE * i;

			MultiplexerRegBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.F",   cy, cx)),
				.FF_RLOC ($sformatf("R%0dC%0d.FFX", cy, cx)),
`endif
				.INIT    (INIT[i0])
			) bit0 (
				.clk  (clk),
				.reset(reset),
				.clkEn(clkEn),

				.sel     (sel),
				.invert  (1'b0),
				.valueA  (valueA  [i0]),
				.valueB  (valueB  [i0]),
				.valueOut(valueOut[i0])
			);

			if (i1 < WIDTH)
				MultiplexerRegBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC%0d.G",   cy, cx)),
					.FF_RLOC ($sformatf("R%0dC%0d.FFY", cy, cx)),
`endif
					.INIT    (INIT[i1])
				) bit1 (
					.clk  (clk),
					.reset(reset),
					.clkEn(clkEn),

					.sel     (sel),
					.invert  (1'b0),
					.valueA  (valueA  [i1]),
					.valueB  (valueB  [i1]),
					.valueOut(valueOut[i1])
				);
		end
	endgenerate
endmodule
