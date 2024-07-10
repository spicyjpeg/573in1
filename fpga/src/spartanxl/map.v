
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
