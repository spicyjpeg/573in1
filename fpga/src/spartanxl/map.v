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

/* I/O cells */

module OBUFE (input I, input E, output O);
	wire T;

	INV   _TECHMAP_REPLACE_.inv  (.I(E), .O(T));
	OBUFT _TECHMAP_REPLACE_.obuf (.I(I), .T(T), .O(O));
endmodule

module IOBUFE (input I, input E, output O, inout IO);
	wire T;

	INV   _TECHMAP_REPLACE_.inv  (.I(E), .O(T));
	OBUFT _TECHMAP_REPLACE_.obuf (.I(I), .T(T), .O(IO));
	IBUF  _TECHMAP_REPLACE_.ibuf (.I(IO), .O(O));
endmodule

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
