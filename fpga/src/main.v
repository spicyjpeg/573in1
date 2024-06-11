
/* Spartan-XL primitive library */

module BUF    (input I, output O);                               endmodule
module INV    (input I, output O);                               endmodule
module AND2   (input I0, input I1, output O);                    endmodule
module NAND2  (input I0, input I1, output O);                    endmodule
module AND2B1 (input I0, input I1, output O);                    endmodule
module OR2    (input I0, input I1, output O);                    endmodule
module NOR2   (input I0, input I1, output O);                    endmodule
module OR2B1  (input I0, input I1, output O);                    endmodule
module XOR2   (input I0, input I1, output O);                    endmodule
module XNOR2  (input I0, input I1, output O);                    endmodule
module BUFT   (input I, input T, output O);                      endmodule
module FDCE   (input D, input C, input CLR, input CE, output Q); endmodule
module FDPE   (input D, input C, input PRE, input CE, output Q); endmodule
module LDCE_1 (input D, input G, input CLR, input GE, output Q); endmodule
module LDPE_1 (input D, input G, input PRE, input GE, output Q); endmodule

module IPAD   (output IPAD);                                   endmodule
module OPAD   (input OPAD);                                    endmodule
module IOPAD  (inout IOPAD);                                   endmodule
module IBUF   (input I, output O);                             endmodule
module OBUF   (input I, output O);                             endmodule
module OBUFT  (input I, input T, output O);                    endmodule
module IOBUFT (input I, input T, output O, inout IO);          endmodule
module BUFGLS (input I, output O);                             endmodule
module IFDX   (input D, input C, input CE, output Q);          endmodule
module IFDXI  (input D, input C, input CE, output Q);          endmodule
module OFDX   (input D, input C, input CE, output Q);          endmodule
module OFDXI  (input D, input C, input CE, output Q);          endmodule
module OFDTX  (input D, input C, input CE, input T, output O); endmodule
module OFDTXI (input D, input C, input CE, input T, output O); endmodule

/* Top-level module */

module FPGA (
	input        nHostRead,
	input        nHostWrite,
	input        nHostEnable,
	input [6:0]  hostAddress,
	inout [15:0] hostData,

	output        nSRAMRead,
	output        nSRAMWrite,
	output        nSRAMEnable,
	output [16:0] sramAddress,
	inout  [7:0]  sramData,

	output [7:0] lightBankA,
	output [3:0] lightBankB,
	output [3:0] lightBankD,

	inout ds2401,
	inout ds2433
);
	genvar i;

	/* System clocks */

	wire clockIn29M, _clock29M;
	wire clockIn19M, _clock19M;

	IPAD   _clockPad29M ( .IPAD(clockIn29M) );
	IPAD   _clockPad19M ( .IPAD(clockIn19M) );
	BUFGLS _clockBuf29M ( .I(clockIn29M), .O(_clock29M) );
	BUFGLS _clockBuf19M ( .I(clockIn19M), .O(_clock19M) );

	/* Host interface */

	wire        _nHostRead, _nHostWrite, _nHostEnable;
	wire [6:0]  _hostAddress;
	wire [15:0] _hostDataIn;
	wire [15:0] _hostDataOut;

	wire _nHostReadFPGA  = _nHostRead  || _nHostEnable;
	wire _nHostWriteFPGA = _nHostWrite || _nHostEnable;

	// IOB flip-flop primitives (IFDX, OFDX) are explicitly used whenever
	// possible in order to minimize propagation delays and CLB usage.
	IFDX _nHostReadBuf   ( .C(_clock29M), .D(nHostRead),   .Q(_nHostRead),   .CE(1'b1) );
	IFDX _nHostWriteBuf  ( .C(_clock29M), .D(nHostWrite),  .Q(_nHostWrite),  .CE(1'b1) );
	IFDX _nHostEnableBuf ( .C(_clock29M), .D(nHostEnable), .Q(_nHostEnable), .CE(1'b1) );

	generate
		for (i = 0; i < 7; i++)
			IFDX _hostAddressBuf (
				.C(_clock29M), .D(hostAddress[i]), .Q(_hostAddress[i])
			);

		for (i = 0; i < 16; i++) begin
			IFDX  _hostDataInBuf  (
				.C(_clock29M), .D(hostData[i]), .Q(_hostDataIn[i]), .CE(1'b1)
			);
			OFDTX _hostDataOutBuf (
				.C(_clock29M), .O(hostData[i]), .D(_hostDataOut[i]),
				.CE(~_nHostReadFPGA), .T(_nHostReadFPGA)
			);
		end
	endgenerate

	/* SRAM interface (currently unused) */

	assign nSRAMRead   = 1'b1;
	assign nSRAMWrite  = 1'b1;
	assign nSRAMEnable = 1'b1;
	assign sramAddress = 17'h1ffff;
	assign sramData    = 8'hff;

	/* Light outputs */

	wire [3:0] _lightBankData = _hostDataIn[15:12];

	wire _lightBankA0Write = ~_nHostWriteFPGA & (_hostAddress == 7'h70);
	wire _lightBankA1Write = ~_nHostWriteFPGA & (_hostAddress == 7'h71);
	wire _lightBankBWrite  = ~_nHostWriteFPGA & (_hostAddress == 7'h72);
	wire _lightBankDWrite  = ~_nHostWriteFPGA & (_hostAddress == 7'h73);

	generate
		for (i = 0; i < 4; i++) begin
			// Use "inverted" flip-flops so that all lights are turned off on
			// startup.
			OFDXI _lightBankA0Buf (
				.C(_clock29M), .Q(lightBankA[i + 4]), .D(_lightBankData[i]),
				.CE(_lightBankA0Write)
			);
			OFDXI _lightBankA1Buf (
				.C(_clock29M), .Q(lightBankA[i]), .D(_lightBankData[i]),
				.CE(_lightBankA1Write)
			);
			OFDXI _lightBankBBuf  (
				.C(_clock29M), .Q(lightBankB[i]), .D(_lightBankData[i]),
				.CE(_lightBankBWrite)
			);
			OFDXI _lightBankDBuf  (
				.C(_clock29M), .Q(lightBankD[i]), .D(_lightBankData[i]),
				.CE(_lightBankDWrite)
			);
		end
	endgenerate

	/* 1-wire bus */

	wire _ds2401In,  _ds2433In;
	reg  _ds2401Out, _ds2433Out;

	// Note that the 1-wire pins are open drain and pulled low by writing 1
	// (rather than 0) to the respective register bits, but not inverted when
	// read.
	IFDX  _ds2401InBuf  ( .C(_clock29M), .D(ds2401), .Q(_ds2401In), .CE(1'b1) );
	IFDX  _ds2433InBuf  ( .C(_clock29M), .D(ds2433), .Q(_ds2433In), .CE(1'b1) );
	OBUFT _ds2401OutBuf ( .O(ds2401), .I(1'b0), .T(~_ds2401Out) );
	OBUFT _ds2433OutBuf ( .O(ds2433), .I(1'b0), .T(~_ds2433Out) );

	wire _dsBusWrite = ~_nHostWriteFPGA & (_hostAddress == 7'h77);

	always @(posedge _clock29M)
		if (_dsBusWrite) begin
			_ds2401Out <= _hostDataIn[12];
			_ds2433Out <= _hostDataIn[13];
		end

	/* Readable registers */

	wire [15:0] _magicNumberData = 16'h573f;
	wire [15:0] _dsBusData       = { 2'h0, _ds2433In, _ds2401In, 12'h000 };

	wire _magicNumberRead = ~_nHostReadFPGA & (_hostAddress == 7'h40);
	wire _dsBusRead       = ~_nHostReadFPGA & (_hostAddress == 7'h77);

	assign _hostDataOut =
		_magicNumberRead ? _magicNumberData :
		_dsBusRead       ? _dsBusData :
		16'h0000;
endmodule
