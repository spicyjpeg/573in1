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
	FMAP lut (.I1(addMode), .I2(carryIn), .I3(msbA), .I4(msbB), .O(carryOut));
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
		.I0(addMode), .I1(carryIn), .I2(operandA), .I3(operandB), .O(result)
	);

	(* HU_SET = ID, RLOC = LUT_RLOC *)
	FMAP lut (
		.I1(addMode), .I2(carryIn), .I3(operandA), .I4(operandB), .O(result)
	);
endmodule

module ALU #(
	parameter ID        = "",
	parameter WIDTH     = 8,
	parameter CARRY_IN  = 0,
	parameter CARRY_OUT = 0
) (
	input addMode,

	input                carryIn,
	input  [WIDTH - 1:0] operandA,
	input  [WIDTH - 1:0] operandB,
	output [WIDTH - 1:0] result,
	output               carryOut,
	output               overflowOut
);
	localparam NUM_CLBS = (WIDTH + 1) / 2;
	localparam Y_OFFSET = NUM_CLBS + CARRY_OUT - 1;
	genvar     i;

	wire [NUM_CLBS * 2:0] carryChain;

	generate
		/* Carry input unit */

		wire [7:0] mode0;

		// iverilog does not support inlining $sformatf() like this, so the RLOC
		// constraints must be removed when targeting the testbench.
`ifdef SYNTHESIS
		(* HU_SET = ID, RLOC = $sformatf("R%0dC0", Y_OFFSET + 1) *)
`endif
		CY4 carryInUnit (
			.COUT(carryChain[0]),

			// ISE will complain if any carry pins are connected but not
			// actually used, so they must be set to 1'bz in order to force
			// Yosys to leave them unconnected.
			.A0 (CARRY_IN ? carryIn : 1'bz),
			.ADD(CARRY_IN ? 1'bz    : addMode),

			.C0(mode0[0]), .C1(mode0[1]), .C2(mode0[2]), .C3(mode0[3]),
			.C4(mode0[4]), .C5(mode0[5]), .C6(mode0[6]), .C7(mode0[7])
		);

		// If the carry input is disabled, this carry unit will invert addMode
		// and always generate a null carry input (0 for addition and 1 for
		// subtraction) for the adder's LSB.
		if (CARRY_IN)
			CY4_39 carryInMode ( // FORCE-F1
				.C0(mode0[0]), .C1(mode0[1]), .C2(mode0[2]), .C3(mode0[3]),
				.C4(mode0[4]), .C5(mode0[5]), .C6(mode0[6]), .C7(mode0[7])
			);
		else
			CY4_41 carryInMode ( // FORCE-F3-
				.C0(mode0[0]), .C1(mode0[1]), .C2(mode0[2]), .C3(mode0[3]),
				.C4(mode0[4]), .C5(mode0[5]), .C6(mode0[6]), .C7(mode0[7])
			);

		for (i = 0; i < NUM_CLBS; i = i + 1) begin
			localparam i0 = i * 2 + 0;
			localparam i1 = i * 2 + 1;
			localparam i2 = i * 2 + 2;

			/* LUTs */

			ALUBit #(
`ifdef SYNTHESIS
				.ID      (ID),
				.LUT_RLOC($sformatf("R%0dC0.F", Y_OFFSET - i))
`endif
			) bit0 (
				.addMode(addMode),

				.carryIn (carryChain[i0]),
				.operandA(operandA  [i0]),
				.operandB(operandB  [i0]),
				.result  (result    [i0])
			);

			if (i1 < WIDTH)
				ALUBit #(
`ifdef SYNTHESIS
					.ID      (ID),
					.LUT_RLOC($sformatf("R%0dC0.G", Y_OFFSET - i))
`endif
				) bit1 (
					.addMode(addMode),

					.carryIn (carryChain[i1]),
					.operandA(operandA  [i1]),
					.operandB(operandB  [i1]),
					.result  (result    [i1])
				);

			/* Carry unit */

			wire [7:0] mode;

`ifdef SYNTHESIS
			(* HU_SET = ID, RLOC = $sformatf("R%0dC0", Y_OFFSET - i) *)
`endif
			CY4 carryUnit (
				.CIN  (carryChain[i0]),
				.COUT0(carryChain[i1]),
				.COUT (carryChain[i2]),

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

		if (CARRY_OUT) begin
			/* Carry output LUTs */

			assign carryChainOut = carryChain[NUM_CLBS * 2];

			ALUCarryLogic #(
`ifdef SYNTHESIS
				.ID         (ID),
				.LUT_RLOC   ("R0C0.F"),
`endif
				.IS_OVERFLOW(0)
			) carryLogic (
				.addMode(addMode),

				.carryIn (carryChainOut),
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

				.carryIn (carryChainOut),
				.msbA    (operandA  [WIDTH - 1]),
				.msbB    (operandB  [WIDTH - 1]),
				.carryOut(overflowOut)
			);

			/* Carry output unit */

			wire [7:0] mode1;

			// This carry unit does not perform any computation but passes CIN
			// from the CLB below it to COUT0 and COUT, allowing the F/G LUTs to
			// access the carry output.
			(* HU_SET = ID, RLOC = "R0C0" *)
			CY4 carryOutUnit (
				.CIN(carryChainOut),

				.C0(mode1[0]), .C1(mode1[1]), .C2(mode1[2]), .C3(mode1[3]),
				.C4(mode1[4]), .C5(mode1[5]), .C6(mode1[6]), .C7(mode1[7])
			);
			CY4_42 carryOutMode ( // EXAMINE-CI
				.C0(mode1[0]), .C1(mode1[1]), .C2(mode1[2]), .C3(mode1[3]),
				.C4(mode1[4]), .C5(mode1[5]), .C6(mode1[6]), .C7(mode1[7])
			);
		end
	endgenerate
endmodule
