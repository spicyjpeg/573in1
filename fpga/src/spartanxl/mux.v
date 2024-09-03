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

module MultiplexerBit #(
	parameter ID       = "",
	parameter LUT_RLOC = ""
) (
	input  sel,
	input  invert,
	input  valueA,
	input  valueB,
	output valueOut
);
	//assign valueOut = invert ^ (sel ? valueB : valueA);
	wire mux0Out, mux1Out, muxOut;

	AND2B1 mux0 (.I0(sel),     .I1(valueA),  .O(mux0Out));
	AND2   mux1 (.I0(sel),     .I1(valueB),  .O(mux1Out));
	OR2    mux  (.I0(mux0Out), .I1(mux1Out), .O(muxOut));
	XOR2   inv  (.I0(muxOut),  .I1(invert),  .O(valueOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (.I1(sel), .I2(invert), .I3(valueA), .I4(valueB), .O(valueOut));
endmodule

module XORMultiplexerBit #(
	parameter ID       = "",
	parameter LUT_RLOC = ""
) (
	input  sel,
	input  valueA,
	input  valueB0,
	input  valueB1,
	output valueOut
);
	//assign valueOut = sel ? (valueB0 ^ valueB1) : valueA;
	wire valueB, mux0Out, mux1Out;

	XOR2   inv  (.I0(valueB0), .I1(valueB1), .O(valueB));
	AND2B1 mux0 (.I0(sel),     .I1(valueA),  .O(mux0Out));
	AND2   mux1 (.I0(sel),     .I1(valueB),  .O(mux1Out));
	OR2    mux  (.I0(mux0Out), .I1(mux1Out), .O(valueOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (.I1(sel), .I2(valueA), .I3(valueB0), .I4(valueB1), .O(valueOut));
endmodule

module Multiplexer #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1
) (
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

			// iverilog does not support inlining $sformatf() like this, so the
			// RLOC constraints must be removed when targeting the testbench.
			MultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.F", cy, cx))
`endif
			) bit0 (
				.sel     (sel),
				.invert  (1'b0),
				.valueA  (valueA  [i0]),
				.valueB  (valueB  [i0]),
				.valueOut(valueOut[i0])
			);

			if (i1 < WIDTH)
				MultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.G", cy, cx))
`endif
				) bit1 (
					.sel     (sel),
					.invert  (1'b0),
					.valueA  (valueA  [i1]),
					.valueB  (valueB  [i1]),
					.valueOut(valueOut[i1])
				);
		end
	endgenerate
endmodule

module BitSwapper #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1,

	parameter _SWAP_WIDTH = (WIDTH + 1) / 2
) (
	input  [_SWAP_WIDTH - 1:0] swap,
	input  [WIDTH       - 1:0] invert,

	input  [WIDTH - 1:0] valueIn,
	output [WIDTH - 1:0] valueOut
);
	localparam NUM_CLBS = _SWAP_WIDTH;
	localparam X_OFFSET = X_ORIGIN + ((X_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	localparam Y_OFFSET = Y_ORIGIN + ((Y_STRIDE < 0) ? (NUM_CLBS - 1) : 0);
	genvar     i;

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam cx = X_OFFSET + X_STRIDE * i;
			localparam cy = Y_OFFSET + Y_STRIDE * i;

			MultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.F", cy, cx))
`endif
			) bit0 (
				.sel     (swap    [i]),
				.invert  (invert  [i0]),
				.valueA  (valueIn [i0]),
				.valueB  (valueIn [i1]),
				.valueOut(valueOut[i0])
			);

			if (i1 < WIDTH)
				MultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.G", cy, cx))
`endif
				) bit1 (
					.sel     (swap    [i]),
					.invert  (invert  [i1]),
					.valueA  (valueIn [i1]),
					.valueB  (valueIn [i0]),
					.valueOut(valueOut[i1])
				);
		end
	endgenerate
endmodule

module XORMultiplexer #(
	parameter ID       = "",
	parameter WIDTH    =  8,
	parameter X_ORIGIN =  0,
	parameter Y_ORIGIN =  0,
	parameter X_STRIDE =  0,
	parameter Y_STRIDE = -1
) (
	input sel,

	input  [WIDTH - 1:0] valueA,
	input  [WIDTH - 1:0] valueB0,
	input  [WIDTH - 1:0] valueB1,
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

			XORMultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.F", cy, cx))
`endif
			) bit0 (
				.sel     (sel),
				.valueA  (valueA  [i0]),
				.valueB0 (valueB0 [i0]),
				.valueB1 (valueB1 [i0]),
				.valueOut(valueOut[i0])
			);

			if (i1 < WIDTH)
				XORMultiplexerBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC%0d.G", cy, cx))
`endif
				) bit1 (
					.sel     (sel),
					.valueA  (valueA  [i1]),
					.valueB0 (valueB0 [i1]),
					.valueB1 (valueB1 [i1]),
					.valueOut(valueOut[i1])
				);
		end
	endgenerate
endmodule
