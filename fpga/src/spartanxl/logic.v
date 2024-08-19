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

/* Constants */

module VCC (output P);
`ifndef SYNTHESIS
	assign P = 1'b1;
`endif
endmodule

module GND (output G);
`ifndef SYNTHESIS
	assign G = 1'b0;
`endif
endmodule

/* Buffers */

module BUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module BUFT (input I, input T, output O);
`ifndef SYNTHESIS
	assign O = T ? 1'bz : I;
`endif
endmodule

module INV (input I, output O);
`ifndef SYNTHESIS
	assign O = ~I;
`endif
endmodule

/* AND gates */

module AND2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 & I1;
`endif
endmodule

module AND2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & I1;
`endif
endmodule

module AND3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = I0 & I1 & I2;
`endif
endmodule

module AND3B1 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & I1 & I2;
`endif
endmodule

module AND3B2 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & I2;
`endif
endmodule

module AND4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = I0 & I1 & I2 & I3;
`endif
endmodule

module AND4B1 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & I1 & I2 & I3;
`endif
endmodule

module AND4B2 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & I2 & I3;
`endif
endmodule

module AND4B3 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & ~I2 & I3;
`endif
endmodule

module AND5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = I0 & I1 & I2 & I3 & I4;
`endif
endmodule

module AND5B1 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & I1 & I2 & I3 & I4;
`endif
endmodule

module AND5B2 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & I2 & I3 & I4;
`endif
endmodule

module AND5B3 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & ~I2 & I3 & I4;
`endif
endmodule

module AND5B4 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 & ~I1 & ~I2 & ~I3 & I4;
`endif
endmodule

/* NAND gates */

module NAND2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 & I1);
`endif
endmodule

module NAND2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & I1);
`endif
endmodule

module NAND3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 & I1 & I2);
`endif
endmodule

module NAND3B1 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & I1 & I2);
`endif
endmodule

module NAND3B2 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & I2);
`endif
endmodule

module NAND4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 & I1 & I2 & I3);
`endif
endmodule

module NAND4B1 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & I1 & I2 & I3);
`endif
endmodule

module NAND4B2 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & I2 & I3);
`endif
endmodule

module NAND4B3 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & ~I2 & I3);
`endif
endmodule

module NAND5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 & I1 & I2 & I3 & I4);
`endif
endmodule

module NAND5B1 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & I1 & I2 & I3 & I4);
`endif
endmodule

module NAND5B2 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & I2 & I3 & I4);
`endif
endmodule

module NAND5B3 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & ~I2 & I3 & I4);
`endif
endmodule

module NAND5B4 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 & ~I1 & ~I2 & ~I3 & I4);
`endif
endmodule

/* OR gates */

module OR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 | I1;
`endif
endmodule

module OR2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | I1;
`endif
endmodule

module OR3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = I0 | I1 | I2;
`endif
endmodule

module OR3B1 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | I1 | I2;
`endif
endmodule

module OR3B2 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | I2;
`endif
endmodule

module OR4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = I0 | I1 | I2 | I3;
`endif
endmodule

module OR4B1 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | I1 | I2 | I3;
`endif
endmodule

module OR4B2 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | I2 | I3;
`endif
endmodule

module OR4B3 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | ~I2 | I3;
`endif
endmodule

module OR5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = I0 | I1 | I2 | I3 | I4;
`endif
endmodule

module OR5B1 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | I1 | I2 | I3 | I4;
`endif
endmodule

module OR5B2 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | I2 | I3 | I4;
`endif
endmodule

module OR5B3 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | ~I2 | I3 | I4;
`endif
endmodule

module OR5B4 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~I0 | ~I1 | ~I2 | ~I3 | I4;
`endif
endmodule

/* NOR gates */

module NOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 | I1);
`endif
endmodule

module NOR2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | I1);
`endif
endmodule

module NOR3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 | I1 | I2);
`endif
endmodule

module NOR3B1 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | I1 | I2);
`endif
endmodule

module NOR3B2 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | I2);
`endif
endmodule

module NOR4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 | I1 | I2 | I3);
`endif
endmodule

module NOR4B1 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | I1 | I2 | I3);
`endif
endmodule

module NOR4B2 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | I2 | I3);
`endif
endmodule

module NOR4B3 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | ~I2 | I3);
`endif
endmodule

module NOR5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 | I1 | I2 | I3 | I4);
`endif
endmodule

module NOR5B1 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | I1 | I2 | I3 | I4);
`endif
endmodule

module NOR5B2 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | I2 | I3 | I4);
`endif
endmodule

module NOR5B3 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | ~I2 | I3 | I4);
`endif
endmodule

module NOR5B4 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(~I0 | ~I1 | ~I2 | ~I3 | I4);
`endif
endmodule

/* XOR gates */

module XOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 ^ I1;
`endif
endmodule

module XOR3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = I0 ^ I1 ^ I2;
`endif
endmodule

module XOR4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = I0 ^ I1 ^ I2 ^ I3;
`endif
endmodule

module XOR5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = I0 ^ I1 ^ I2 ^ I3 ^ I4;
`endif
endmodule

/* XNOR gates */

module XNOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 ^ I1);
`endif
endmodule

module XNOR3 (input I0, input I1, input I2, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 ^ I1 ^ I2);
`endif
endmodule

module XNOR4 (input I0, input I1, input I2, input I3, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 ^ I1 ^ I2 ^ I3);
`endif
endmodule

module XNOR5 (input I0, input I1, input I2, input I3, input I4, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 ^ I1 ^ I2 ^ I3 ^ I4);
`endif
endmodule

/* CLB placement control primitives */

(* keep *) module FMAP (input I1, input I2, input I3, input I4, input O);
endmodule

(* keep *) module HMAP (input I1, input I2, input I3, input O);
endmodule
