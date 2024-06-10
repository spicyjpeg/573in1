/*
 * This file is used by Yosys to map its internal gate representations to the
 * respective primitives supported by ISE's mapping tools. Note that ISE does
 * not expose LUT primitives for Spartan-XL devices, so LUT-based synthesis and
 * the synth_xilinx command built into Yosys cannot be used.
 */

(* techmap_celltype = "IOBUFT" *)
module _ISE_IOBUFT (inout IO, input I, output O, input T);
	IBUF  _TECHMAP_REPLACE_.ibuf ( .I(IO), .O(O) );
	OBUFT _TECHMAP_REPLACE_.obuf ( .O(IO), .I(I), .T(T) );
endmodule

(* techmap_celltype = "$_BUF_" *)
module _ISE_BUF (input A, output Y);
	BUF _TECHMAP_REPLACE_ ( .I(A), .O(Y) );
endmodule

(* techmap_celltype = "$_NOT_" *)
module _ISE_INV (input A, output Y);
	INV _TECHMAP_REPLACE_ ( .I(A), .O(Y) );
endmodule

(* techmap_celltype = "$_AND_" *)
module _ISE_AND2 (input A, input B, output Y);
	AND2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_NAND_" *)
module _ISE_NAND2 (input A, input B, output Y);
	NAND2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_ANDNOT_" *)
module _ISE_AND2B1 (input A, input B, output Y);
	AND2B1 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_OR_" *)
module _ISE_OR2 (input A, input B, output Y);
	OR2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_NOR_" *)
module _ISE_NOR2 (input A, input B, output Y);
	NOR2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_ORNOT_" *)
module _ISE_OR2B1 (input A, input B, output Y);
	OR2B1 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_XOR_" *)
module _ISE_XOR2 (input A, input B, output Y);
	XOR2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_XNOR_" *)
module _ISE_XNOR2 (input A, input B, output Y);
	XNOR2 _TECHMAP_REPLACE_ ( .I0(A), .I1(B), .O(Y) );
endmodule

(* techmap_celltype = "$_TBUF_" *)
module _ISE_BUFT (input A, input E, output Y);
	BUFT _TECHMAP_REPLACE_ ( .I(A), .T(~E), .O(Y) );
endmodule

(* techmap_celltype = "$_DFFE_PP0P_" *)
module _ISE_FDCE (input D, input C, input R, input E, output Q);
	FDCE _TECHMAP_REPLACE_ ( .D(D), .CE(E), .C(C), .CLR(R), .Q(Q) );
endmodule

(* techmap_celltype = "$_DFFE_PP1P_" *)
module _ISE_FDPE (input D, input C, input R, input E, output Q);
	FDPE _TECHMAP_REPLACE_ ( .D(D), .CE(E), .C(C), .PRE(R), .Q(Q) );
endmodule

(* techmap_celltype = "$_DLATCH_PP0_" *)
module _ISE_LDCE_1 (input E, input R, input D, output Q);
	LDCE_1 _TECHMAP_REPLACE_ ( .D(D), .GE(1), .G(E), .CLR(R), .Q(Q) );
endmodule

(* techmap_celltype = "$_DLATCH_PP1_" *)
module _ISE_LDPE_1 (input E, input R, input D, output Q);
	LDPE_1 _TECHMAP_REPLACE_ ( .D(D), .GE(1), .G(E), .PRE(R), .Q(Q) );
endmodule
