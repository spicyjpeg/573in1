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

module CompareConst #(
	parameter WIDTH = 4,
	parameter VALUE = 0
) (
	input [WIDTH - 1:0] value,
	output              result
);
	genvar i;

	wire [8:0] valueIn;
	wire       lutFOut, lutGOut;

	generate
		/* Input inverters */

		for (i = 0; i < 9; i = i + 1) begin
			if (i >= WIDTH)
				assign valueIn[i] = 1'b1;
			else if (VALUE[i])
				assign valueIn[i] = value[i];
			else
				INV inv (.I(value[i]), .O(valueIn[i]));
		end

		/* AND gates */

		AND4 andF (
			.I0(valueIn[0]),
			.I1(valueIn[1]),
			.I2(valueIn[2]),
			.I3(valueIn[3]),
			.O (lutFOut)
		);
		FMAP lutF (
			.I1((WIDTH > 0) ? value[0] : 1'bz),
			.I2((WIDTH > 1) ? value[1] : 1'bz),
			.I3((WIDTH > 2) ? value[2] : 1'bz),
			.I4((WIDTH > 3) ? value[3] : 1'bz),
			.O (lutFOut)
		);

		if (WIDTH < 5) begin
			assign result = lutFOut;
		end else if (WIDTH < 7) begin
			AND3 andH (
				.I0(lutFOut),
				.I1(valueIn[4]),
				.I2(valueIn[5]),
				.O (result)
			);
			HMAP lutH (
				.I1(lutFOut),
				.I2((WIDTH > 4) ? value[4] : 1'bz),
				.I3((WIDTH > 5) ? value[5] : 1'bz),
				.O (result)
			);
		end else begin
			AND4 andG (
				.I0(valueIn[4]),
				.I1(valueIn[5]),
				.I2(valueIn[6]),
				.I3(valueIn[7]),
				.O (lutGOut)
			);
			FMAP lutG (
				.I1((WIDTH > 4) ? value[4] : 1'bz),
				.I2((WIDTH > 5) ? value[5] : 1'bz),
				.I3((WIDTH > 6) ? value[6] : 1'bz),
				.I4((WIDTH > 7) ? value[7] : 1'bz),
				.O (lutGOut)
			);

			AND3 andH (
				.I0(lutFOut),
				.I1(lutGOut),
				.I2(valueIn[8]),
				.O (result)
			);
			HMAP lutH (
				.I1(lutFOut),
				.I2(lutGOut),
				.I3((WIDTH > 8) ? value[8] : 1'bz),
				.O (result)
			);
		end
	endgenerate
endmodule
