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

module CounterBit #(
	parameter ID       = "",
	parameter LUT_RLOC = "",
	parameter FF_RLOC  = "",
	parameter INIT     = 0
) (
	input clk,
	input reset,
	input clkEn,

	input  carryIn,
	input  load,
	input  valueIn,
	output valueOut
);
	//wire lutOut = load ? valueIn : (valueOut ^ carryIn);
	wire adderOut, mux0Out, mux1Out, lutOut;

	XOR2   adder (.I0(carryIn), .I1(valueOut), .O(adderOut));
	AND2B1 mux0  (.I0(load),    .I1(adderOut), .O(mux0Out));
	AND2   mux1  (.I0(load),    .I1(valueIn),  .O(mux1Out));
	OR2    mux   (.I0(mux0Out), .I1(mux1Out),  .O(lutOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (.I1(load), .I2(carryIn), .I3(valueIn), .I4(valueOut), .O(lutOut));

	generate
		if (INIT)
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDPE ff (.D(lutOut), .C(clk), .PRE(reset), .CE(clkEn), .Q(valueOut));
		else
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDCE ff (.D(lutOut), .C(clk), .CLR(reset), .CE(clkEn), .Q(valueOut));
	endgenerate
endmodule

module Counter #(
	parameter ID        = "",
	parameter WIDTH     = 8,
	parameter INIT      = 0,
	parameter CARRY_IN  = 0,
	parameter CARRY_OUT = 0
) (
	input clk,
	input reset,
	input clkEn,

	input                carryIn,
	input  [WIDTH - 1:0] load,
	input  [WIDTH - 1:0] valueIn,
	output [WIDTH - 1:0] valueOut,
	output               carryOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;
	localparam Y_OFFSET = (WIDTH + CARRY_OUT + 1) / 2 - 1;
	genvar     i;

	wire [NUM_CLBS * 2:0] carryChain;

	generate
		/* Carry input unit */

		if (CARRY_IN) begin
			wire [7:0] mode0;

			// iverilog does not support inlining $sformatf() like this, so the
			// RLOC constraints must be removed when targeting the testbench.
`ifdef SYNTHESIS
			(* HU_SET = ID, RLOC = $sformatf("R%0dC0", Y_OFFSET + 1) *)
`endif
			CY4 carryInUnit (
				.COUT(carryChain[0]),

				.A0(carryIn),

				.C0(mode0[0]), .C1(mode0[1]), .C2(mode0[2]), .C3(mode0[3]),
				.C4(mode0[4]), .C5(mode0[5]), .C6(mode0[6]), .C7(mode0[7])
			);
			CY4_39 carryInMode ( // FORCE-F1
				.C0(mode0[0]), .C1(mode0[1]), .C2(mode0[2]), .C3(mode0[3]),
				.C4(mode0[4]), .C5(mode0[5]), .C6(mode0[6]), .C7(mode0[7])
			);
		end else begin
			assign carryChain[0] = 1'b1;
		end

		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam i2 = i * 2 + 2;

			/* LUTs */

			CounterBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC0.F",   Y_OFFSET - i)),
				.FF_RLOC ($sformatf("R%0dC0.FFX", Y_OFFSET - i)),
`endif
				.INIT    (INIT[i0])
			) bit0 (
				.clk   (clk),
				.reset (reset),
				.clkEn (clkEn),

				.carryIn (carryChain[i0]),
				.load    (load      [i0]),
				.valueIn (valueIn   [i0]),
				.valueOut(valueOut  [i0])
			);

			if (i1 < WIDTH)
				CounterBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC0.G",   Y_OFFSET - i)),
					.FF_RLOC ($sformatf("R%0dC0.FFY", Y_OFFSET - i)),
`endif
					.INIT    (INIT[i1])
				) bit1 (
					.clk   (clk),
					.reset (reset),
					.clkEn (clkEn),

					.carryIn (carryChain[i1]),
					.load    (load      [i1]),
					.valueIn (valueIn   [i1]),
					.valueOut(valueOut  [i1])
				);

			/* Carry unit */

			wire [7:0] mode;

`ifdef SYNTHESIS
			(* HU_SET = ID, RLOC = $sformatf("R%0dC0", Y_OFFSET - i) *)
`endif
			CY4 carryUnit (
				// ISE will complain if any carry pins are connected but not
				// actually used, so they must be set to 1'bz in order to force
				// Yosys to leave them unconnected.
				.CIN  ((i || CARRY_IN) ? carryChain[i0] : 1'bz),
				.COUT0(carryChain[i1]),
				.COUT (carryChain[i2]),

				.A0(valueOut[i0]),
				.A1((i1 < WIDTH) ? valueOut[i1] : 1'bz),

				.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
				.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
			);

			if (!i && !CARRY_IN)
				CY4_19 carryMode ( // INC-FG-1
					.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
					.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
				);
			else if (i1 < WIDTH)
				CY4_18 carryMode ( // INC-FG-CI
					.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
					.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
				);
			else
				CY4_17 carryMode ( // INC-F-CI
					.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
					.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
				);
		end

		if (CARRY_OUT) begin
			/* Carry output unit */

			assign carryOut = carryChain[WIDTH];

			if (!(WIDTH % 2)) begin
				wire [7:0] mode1;

				// This carry unit does not perform any computation but passes
				// CIN from the CLB below it to COUT0 and COUT, allowing the F/G
				// LUTs to access the carry output. It is not required if the
				// last counter CLB's G LUT is free, as that also has access to
				// COUT through the CLB's own carry unit.
				(* HU_SET = ID, RLOC = "R0C0" *)
				CY4 carryOutUnit (
					.CIN(carryOut),

					.C0(mode1[0]), .C1(mode1[1]), .C2(mode1[2]), .C3(mode1[3]),
					.C4(mode1[4]), .C5(mode1[5]), .C6(mode1[6]), .C7(mode1[7])
				);
				CY4_42 carryOutMode ( // EXAMINE-CI
					.C0(mode1[0]), .C1(mode1[1]), .C2(mode1[2]), .C3(mode1[3]),
					.C4(mode1[4]), .C5(mode1[5]), .C6(mode1[6]), .C7(mode1[7])
				);
			end
		end
	endgenerate
endmodule
