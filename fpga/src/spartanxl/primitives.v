
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

/* Flip flops */

module FDCE (input D, input C, input CLR, input CE, output Q);
`ifndef SYNTHESIS
	reg    _Q = 1'b0;
	assign Q  = _Q;

	always @(posedge C, CLR)
		if (CLR)
			_Q <= 1'b0;
		else if (CE)
			_Q <= D;
`endif
endmodule

module FDPE (input D, input C, input PRE, input CE, output Q);
`ifndef SYNTHESIS
	reg    _Q = 1'b1;
	assign Q  = _Q;

	always @(posedge C, PRE)
		if (PRE)
			_Q <= 1'b1;
		else if (CE)
			_Q <= D;
`endif
endmodule

/* Latches */

module LDCE_1 (input D, input G, input CLR, input GE, output Q);
`ifndef SYNTHESIS
	reg    _Q = 1'b0;
	assign Q  = _Q;

	always @*
		if (CLR)
			_Q <= 1'b0;
		else if (GE)
			_Q <= D;
`endif
endmodule

module LDPE_1 (input D, input G, input PRE, input GE, output Q);
`ifndef SYNTHESIS
	reg    _Q = 1'b1;
	assign Q  = _Q;

	always @*
		if (PRE)
			_Q <= 1'b1;
		else if (GE)
			_Q <= D;
`endif
endmodule

/* CLB placement control primitives */

module FMAP (input I1, input I2, input I3, input I4, input O);
endmodule

module HMAP (input I1, input I2, input I3, input O);
endmodule

/* I/O buffers */

module BUFGLS (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module IBUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module OBUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module OBUFT (input I, input T, output O);
`ifndef SYNTHESIS
	assign O = T ? 1'bz : I;
`endif
endmodule

/* I/O flip flops */

module IFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b0;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module IFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b1;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module OFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDX ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDXI ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDTX (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire   Q;
	assign O = T ? 1'bz : Q;

	IFDX ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

module OFDTXI (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire   Q;
	assign O = T ? 1'bz : Q;

	IFDXI ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

/* I/O pads */

module IPAD (output IPAD);
endmodule

module OPAD (input OPAD);
endmodule

module IOPAD (inout IOPAD);
endmodule

module UPAD (inout UPAD);
endmodule

module TDI (inout I);
endmodule

module TDO (input O);
endmodule

module TCK (inout I);
endmodule

module TMS (inout I);
endmodule

/* Fixed-function blocks */

module STARTUP (
	input GSR,
	input GTS,
	input CLK,

	output DONEIN,
	output Q1Q4,
	output Q2,
	output Q3
);
endmodule

module OSC4 (
	output F8M,
	output F500K,
	output F16K,
	output F490,
	output F15
);
endmodule

module BSCAN(
	input TDI,
	input TDO1,
	input TDO2,
	input TCK,
	input TMS,

	output TDO,
	output DRCK,
	output IDLE,
	output SEL1,
	output SEL2
);
endmodule

module RDBK (input TRIG, output DATA, output RIP);
endmodule

module RDCLK (input I);
endmodule
