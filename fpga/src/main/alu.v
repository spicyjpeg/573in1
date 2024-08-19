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

/* Spartan-XL optimized adder/subtractor */

module ALUCarryLogic #(
	parameter ID          = "",
	parameter LUT_RLOC    = "",
	parameter IS_OVERFLOW = 0
) (
	input addMode,

	input  carryIn,
	input  msbA,
	input  msbB,
	output carryOut
);
	//wire msbBInv = msbB ^ ~addMode;
	wire msbBInv;

	XNOR2 msbBMux (.I0(msbB), .I1(addMode), .O(msbBInv));

	//wire msbCarry = (carryIn & msbA) | (carryIn & msbBInv) | (msbA & msbBInv);
	wire carry0, carry1, carry2, msbCarry;

	AND2 carryAnd0 (.I0(carryIn), .I1(msbA),    .O(carry0));
	AND2 carryAnd1 (.I0(carryIn), .I1(msbBInv), .O(carry1));
	AND2 carryAnd2 (.I0(msbA),    .I1(msbBInv), .O(carry2));
	OR3  carryOr   (.I0(carry0),  .I1(carry1),  .I2(carry2), .O(msbCarry));

	//assign carryOut = IS_OVERFLOW ? (msbCarry ^ carryIn) : msbCarry;
	wire overflowCarry = IS_OVERFLOW ? carryIn : 1'b0;

	XOR2 carryOutMux (.I0(msbCarry), .I1(overflowCarry), .O(carryOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (
		.I1(addMode),
		.I2(carryIn),
		.I3(msbA),
		.I4(msbB),
		.O (carryOut)
	);
endmodule

module ALUBit #(
	parameter ID       = "",
	parameter LUT_RLOC = ""
) (
	input addMode,

	input  carryIn,
	input  operandA,
	input  operandB,
	output result
);
	//assign result = (operandA ^ carryIn) ^ (operandB ^ ~addMode);
	XNOR4 adder (
		.I0(addMode),
		.I1(carryIn),
		.I2(operandA),
		.I3(operandB),
		.O (result)
	);

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (
		.I1(addMode),
		.I2(carryIn),
		.I3(operandA),
		.I4(operandB),
		.O (result)
	);
endmodule

module ALU #(
	parameter ID    = "",
	parameter WIDTH = 8
) (
	input addMode,

	input  [WIDTH - 1:0] operandA,
	input  [WIDTH - 1:0] operandB,
	output [WIDTH - 1:0] result,
	output               carryOut,
	output               overflowOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;

	genvar i;

	wire [NUM_CLBS:0] carryChain;

	/* Carry output LUTs */

	// iverilog does not support passing strings as parameters, so the RLOC
	// constraints must be removed when targeting the testbench.
	ALUCarryLogic #(
`ifdef SYNTHESIS
		.ID         (ID),
		.LUT_RLOC   ("R0C0.F"),
`endif
		.IS_OVERFLOW(0)
	) carryLogic (
		.addMode(addMode),

		.carryIn (carryChain[NUM_CLBS]),
		.msbA    (operandA  [WIDTH - 1]),
		.msbB    (operandB  [WIDTH - 1]),
		.carryOut(carryOut)
	);
	ALUCarryLogic #(
`ifdef SYNTHESIS
		.ID         (ID),
		.LUT_RLOC   ("R0C0.G"),
`endif
		.IS_OVERFLOW(1)
	) overflowLogic (
		.addMode(addMode),

		.carryIn (carryChain[NUM_CLBS]),
		.msbA    (operandA  [WIDTH - 1]),
		.msbB    (operandB  [WIDTH - 1]),
		.carryOut(overflowOut)
	);

	/* Carry input and output units */

	wire [7:0] inMode;
	wire [7:0] outMode;

	// This carry unit inverts addMode, always generating a null carry input (0
	// for addition and 1 for subtraction) for the adder's LSB.
	(* HU_SET = ID, RLOC = "R0C0" *)
	CY4 carryInUnit (
		.COUT(carryChain[0]),

		.ADD(addMode),

		.C0(inMode[0]), .C1(inMode[1]), .C2(inMode[2]), .C3(inMode[3]),
		.C4(inMode[4]), .C5(inMode[5]), .C6(inMode[6]), .C7(inMode[7])
	);
	CY4_41 carryInMode ( // FORCE-F3-
		.C0(inMode[0]), .C1(inMode[1]), .C2(inMode[2]), .C3(inMode[3]),
		.C4(inMode[4]), .C5(inMode[5]), .C6(inMode[6]), .C7(inMode[7])
	);

	// This carry unit does not perform any computation but passes CIN from the
	// CLB below it to COUT0 and COUT, allowing the F/G LUTs (and thus any
	// external logic) to access the carry output.
	(* HU_SET = ID, RLOC = "R0C0" *)
	CY4 carryOutUnit (
		.CIN(carryChain[NUM_CLBS]),

		.C0(outMode[0]), .C1(outMode[1]), .C2(outMode[2]), .C3(outMode[3]),
		.C4(outMode[4]), .C5(outMode[5]), .C6(outMode[6]), .C7(outMode[7])
	);
	CY4_42 carryOutMode ( // EXAMINE-CI
		.C0(outMode[0]), .C1(outMode[1]), .C2(outMode[2]), .C3(outMode[3]),
		.C4(outMode[4]), .C5(outMode[5]), .C6(outMode[6]), .C7(outMode[7])
	);

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;

			wire cout0;

			/* LUTs */

			ALUBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC0.F", NUM_CLBS - i))
`endif
			) bit0 (
				.addMode(addMode),

				.carryIn (carryChain[i]),
				.operandA(operandA  [i0]),
				.operandB(operandB  [i0]),
				.result  (result    [i0])
			);

			if (i1 < WIDTH)
				ALUBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC0.G", NUM_CLBS - i))
`endif
				) bit1 (
					.addMode(addMode),

					.carryIn (cout0),
					.operandA(operandA[i1]),
					.operandB(operandB[i1]),
					.result  (result  [i1])
				);

			/* Carry unit */

			wire [7:0] mode;

`ifdef SYNTHESIS
			(* HU_SET = ID, RLOC = $sformatf("R%0dC0", NUM_CLBS - i) *)
`endif
			CY4 carryUnit (
				.CIN  (carryChain[i]),
				.COUT0(cout0),
				.COUT (carryChain[i + 1]),

				// ISE will complain if any carry pins are connected but not
				// actually used, so they must be set to 1'bz in order to force
				// Yosys to leave them unconnected.
				.A0 (operandA[i0]),
				.A1 ((i1 < WIDTH) ? operandA[i1] : 1'bz),
				.B0 (operandB[i0]),
				.B1 ((i1 < WIDTH) ? operandB[i1] : 1'bz),
				.ADD(addMode),

				.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
				.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
			);

			if (i1 < WIDTH)
				CY4_13 carryMode ( // ADDSUB-FG-CI
					.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
					.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
				);
			else
				CY4_12 carryMode ( // ADDSUB-F-CI
					.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
					.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
				);
		end
	endgenerate
endmodule

/* Spartan-XL optimized binary counter */

module CounterBit #(
	parameter ID       = "",
	parameter LUT_RLOC = "",
	parameter FF_RLOC  = "",
	parameter INIT     = 0
) (
	input clk,
	input reset,
	input clkEn,
	input loadEn,

	input  carryIn,
	input  valueIn,
	output valueOut
);
	//wire lutOut = loadEn ? valueIn : (valueOut ^ carryIn);
	wire adderOut, mux0Out, mux1Out, lutOut;

	XOR2   adder (.I0(carryIn), .I1(valueOut), .O(adderOut));
	AND2B1 mux0  (.I0(loadEn),  .I1(adderOut), .O(mux0Out));
	AND2   mux1  (.I0(loadEn),  .I1(valueIn),  .O(mux1Out));
	OR2    mux   (.I0(mux0Out), .I1(mux1Out),  .O(lutOut));

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (
		.I1(loadEn),
		.I2(carryIn),
		.I3(valueIn),
		.I4(valueOut),
		.O (lutOut)
	);

	generate
		if (INIT)
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDPE ff (
				.D  (lutOut),
				.C  (clk),
				.PRE(reset),
				.CE (clkEn),
				.Q  (valueOut)
			);
		else
			(* HU_SET = ID, RLOC = FF_RLOC *)
			FDCE ff (
				.D  (lutOut),
				.C  (clk),
				.CLR(reset),
				.CE (clkEn),
				.Q  (valueOut)
			);
	endgenerate
endmodule

module Counter #(
	parameter ID    = "",
	parameter WIDTH = 8,
	parameter INIT  = 0
) (
	input clk,
	input reset,
	input countEn,
	input loadEn,

	input  [WIDTH - 1:0] valueIn,
	output [WIDTH - 1:0] valueOut,
	output               carryOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;

	genvar i;

	wire              clkEn = countEn | loadEn;
	wire [NUM_CLBS:0] carryChain;

	assign carryChain[0] = 1'b1;
	assign carryOut      = carryChain[NUM_CLBS];

	/* Carry output unit */

	wire [7:0] outMode;

	(* HU_SET = ID, RLOC = "R0C0" *)
	CY4 carryOutUnit (
		.CIN(carryOut),

		.C0(outMode[0]), .C1(outMode[1]), .C2(outMode[2]), .C3(outMode[3]),
		.C4(outMode[4]), .C5(outMode[5]), .C6(outMode[6]), .C7(outMode[7])
	);
	CY4_42 carryOutMode ( // EXAMINE-CI
		.C0(outMode[0]), .C1(outMode[1]), .C2(outMode[2]), .C3(outMode[3]),
		.C4(outMode[4]), .C5(outMode[5]), .C6(outMode[6]), .C7(outMode[7])
	);

	generate
		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;

			wire cout0;

			/* LUTs */

			CounterBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC0.F",   NUM_CLBS - i)),
				.FF_RLOC ($sformatf("R%0dC0.FFX", NUM_CLBS - i)),
`endif
				.INIT    ((INIT >> i0) & 1)
			) bit0 (
				.clk   (clk),
				.reset (reset),
				.clkEn (clkEn),
				.loadEn(loadEn),

				.carryIn (carryChain[i]),
				.valueIn (valueIn   [i0]),
				.valueOut(valueOut  [i0])
			);

			if (i1 < WIDTH)
				CounterBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC0.G",   NUM_CLBS - i)),
					.FF_RLOC ($sformatf("R%0dC0.FFY", NUM_CLBS - i)),
`endif
					.INIT    ((INIT >> i1) & 1)
				) bit1 (
					.clk   (clk),
					.reset (reset),
					.clkEn (clkEn),
					.loadEn(loadEn),

					.carryIn (cout0),
					.valueIn (valueIn [i1]),
					.valueOut(valueOut[i1])
				);

			/* Carry unit */

			wire [7:0] mode;

`ifdef SYNTHESIS
			(* HU_SET = ID, RLOC = $sformatf("R%0dC0", NUM_CLBS - i) *)
`endif
			CY4 carryUnit (
				.CIN  (i ? carryChain[i] : 1'bz),
				.COUT0(cout0),
				.COUT (carryChain[i + 1]),

				.A0(valueOut[i0]),
				.A1((i1 < WIDTH) ? valueOut[i1] : 1'bz),

				.C0(mode[0]), .C1(mode[1]), .C2(mode[2]), .C3(mode[3]),
				.C4(mode[4]), .C5(mode[5]), .C6(mode[6]), .C7(mode[7])
			);

			if (!i)
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
	endgenerate
endmodule
