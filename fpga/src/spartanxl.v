
/* Logic primitives */

module BUF (input I, output O);
`ifndef SYNTHESIS
	assign O = I;
`endif
endmodule

module INV (input I, output O);
`ifndef SYNTHESIS
	assign O = ~I;
`endif
endmodule

module AND2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 & I1;
`endif
endmodule

module NAND2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 & I1);
`endif
endmodule

module AND2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 & ~I1;
`endif
endmodule

module OR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 | I1;
`endif
endmodule

module NOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = ~(I0 | I1);
`endif
endmodule

module OR2B1 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = I0 | ~I1;
`endif
endmodule

module XOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = (I0 != I1);
`endif
endmodule

module XNOR2 (input I0, input I1, output O);
`ifndef SYNTHESIS
	assign O = (I0 == I1);
`endif
endmodule

module BUFT (input I, input T, output O);
`ifndef SYNTHESIS
	assign O = T ? 1'bz : I;
`endif
endmodule

module FDCE (input D, input C, input CLR, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b0;
	always @(CLR)
		data <= 1'b0;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module FDPE (input D, input C, input PRE, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b1;
	always @(PRE)
		data <= 1'b1;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

module LDCE_1 (input D, input G, input CLR, input GE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b0;
	always @(CLR)
		data <= 1'b0;
	always @(~G)
		data <= GE ? D : data;
`endif
endmodule

module LDPE_1 (input D, input G, input PRE, input GE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b1;
	always @(PRE)
		data <= 1'b1;
	always @(~G)
		data <= GE ? D : data;
`endif
endmodule

/* I/O primitives */

(* keep *) module BUFGLS (input I, output O);
`ifndef SYNTHESIS
	BUF buffer (.I(I), .O(O));
`endif
endmodule

(* keep *) module IBUF (input I, output O);
`ifndef SYNTHESIS
	BUF buffer (.I(I), .O(O));
`endif
endmodule

(* keep *) module OBUF (input I, output O);
`ifndef SYNTHESIS
	BUF buffer (.I(I), .O(O));
`endif
endmodule

(* keep *) module OBUFT (input I, input T, output O);
`ifndef SYNTHESIS
	BUFT buffer (.I(I), .T(T), .O(O));
`endif
endmodule

(* keep *) module IOBUFT (input I, input T, output O, inout IO);
`ifndef SYNTHESIS
	assign O = IO;

	BUFT buffer (.I(I), .T(T), .O(IO));
`endif
endmodule

(* keep *) module IFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b0;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

(* keep *) module IFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	reg    data;
	assign Q = data;

	initial
		data  = 1'b1;
	always @(posedge C)
		data <= CE ? D : data;
`endif
endmodule

(* keep *) module OFDX (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDX ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

(* keep *) module OFDXI (input D, input C, input CE, output Q);
`ifndef SYNTHESIS
	IFDXI ff (.D(D), .C(C), .CE(CE), .Q(Q));
`endif
endmodule

(* keep *) module OFDTX (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire Q;

	IFDX ff     (.D(D), .C(C), .CE(CE), .Q(Q));
	BUFT buffer (.I(Q), .T(T), .O(O));
`endif
endmodule

(* keep *) module OFDTXI (input D, input C, input CE, input T, output O);
`ifndef SYNTHESIS
	wire Q;

	IFDXI ff     (.D(D), .C(C), .CE(CE), .Q(Q));
	BUFT  buffer (.I(Q), .T(T), .O(O));
`endif
endmodule

/* I/O pads */

(* keep *) module IPAD (output IPAD);
endmodule

(* keep *) module OPAD (input OPAD);
endmodule

(* keep *) module IOPAD (inout IOPAD);
endmodule

(* keep *) module UPAD (inout UPAD);
endmodule

(* keep *) module TDI (inout I);
endmodule

(* keep *) module TDO (input O);
endmodule

(* keep *) module TCK (inout I);
endmodule

(* keep *) module TMS (inout I);
endmodule

/* Fixed-function blocks (not simulated) */

(* keep *) module STARTUP (
	input GSR,
	input GTS,
	input CLK,

	output DONEIN,
	output Q1Q4,
	output Q2,
	output Q3
);
endmodule

(* keep *) module OSC4 (
	output F8M,
	output F500K,
	output F16K,
	output F490,
	output F15
);
endmodule

(* keep *) module BSCAN(
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

(* keep *) module RDBK (input TRIG, output DATA, output RIP);
endmodule

(* keep *) module RDCLK (input I);
endmodule

/* ISE built-in macros */

module CC8CE (
	input C, input CLR, input CE, output [7:0] Q, output CEO, output TC
);
`ifndef SYNTHESIS
	reg [7:0] data;

	assign Q   = data;
	assign TC  = &data;
	assign CEO = TC & CE;

	initial
		data  = 8'h00;
	always @(CLR)
		data <= 8'h00;
	always @(posedge C)
		data <= data + CE;
`endif
endmodule

module CC8CLE (
	input [7:0] D, input L, input C, input CLR, input CE, output [7:0] Q,
	output CEO, output TC
);
`ifndef SYNTHESIS
	reg [7:0] data;

	assign Q   = data;
	assign TC  = &data;
	assign CEO = TC & CE;

	initial
		data  = 8'h00;
	always @(CLR)
		data <= 8'h00;
	always @(posedge C)
		if (L)
			data <= D;
		else
			data <= data + CE;
`endif
endmodule

module CC16CE (
	input C, input CLR, input CE, output [15:0] Q, output CEO, output TC
);
`ifndef SYNTHESIS
	reg [15:0] data;

	assign Q   = data;
	assign TC  = &data;
	assign CEO = TC & CE;

	initial
		data  = 16'h0000;
	always @(CLR)
		data <= 16'h0000;
	always @(posedge C)
		data <= data + CE;
`endif
endmodule

module CC16CLE (
	input [15:0] D, input L, input C, input CLR, input CE, output [15:0] Q,
	output CEO, output TC
);
`ifndef SYNTHESIS
	reg [15:0] data;

	assign Q   = data;
	assign TC  = &data;
	assign CEO = TC & CE;

	initial
		data  = 16'h0000;
	always @(CLR)
		data <= 16'h0000;
	always @(posedge C)
		if (L)
			data <= D;
		else
			data <= data + CE;
`endif
endmodule
