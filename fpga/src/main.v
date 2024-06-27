
module FPGA (
	// These are technically inputs, however they are already wired up to pad
	// primitives within the module. They are only exposed here in order to
	// allow for a testbench to inject the clocks during simulation.
	inout clockIn29M,
	inout clockIn19M,

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

	output [11:0] dramControl,
	output [11:0] dramAddress,
	inout  [15:0] dramData,

	output mp3Reset,
	input  mp3Ready,
	output mp3ClockIn,
	input  mp3ClockOut,
	inout  mp3SDA,
	inout  mp3SCL,

	output mp3StatusCS,
	input  mp3StatusError,
	input  mp3StatusFrameSync,
	input  mp3StatusDataReq,

	output mp3InSDIN,
	output mp3InBCLK,
	output mp3InLRCK,
	input  mp3OutSDOUT,
	input  mp3OutBCLK,
	input  mp3OutLRCK,

	output dacSDIN,
	output dacBCLK,
	output dacLRCK,
	output dacMCLK,

	output [3:0] lightBankAH,
	output [3:0] lightBankAL,
	output [3:0] lightBankBH,
	output [3:0] lightBankD,

	output networkTXEnable,
	output networkTX,
	input  networkRX,

	output serialTX,
	input  serialRX,
	output serialRTS,
	input  serialCTS,
	output serialDTR,
	input  serialDSR,

	inout ds2433,
	inout ds2401
);
	genvar i;

	/* Register definitions */

	localparam SYS573D_FPGA_MAGIC = 8'h80;

	localparam SYS573D_FPGA_MP3_PTR_H     = 8'ha0;
	localparam SYS573D_FPGA_MP3_PTR_L     = 8'ha2;
	localparam SYS573D_FPGA_MP3_ENDPTR_H  = 8'ha4;
	localparam SYS573D_FPGA_MP3_ENDPTR_L  = 8'ha6;
	localparam SYS573D_FPGA_MP3_COUNTER   = 8'ha8;
	localparam SYS573D_FPGA_MP3_KEY1      = 8'ha8;
	localparam SYS573D_FPGA_MP3_FEED_STAT = 8'haa;
	localparam SYS573D_FPGA_MP3_I2C       = 8'hac;
	localparam SYS573D_FPGA_MP3_FEED_CTRL = 8'hae;

	localparam SYS573D_FPGA_DRAM_WRPTR_H = 8'hb0;
	localparam SYS573D_FPGA_DRAM_WRPTR_L = 8'hb2;
	localparam SYS573D_FPGA_DRAM_DATA    = 8'hb4;
	localparam SYS573D_FPGA_DRAM_RDPTR_H = 8'hb6;
	localparam SYS573D_FPGA_DRAM_RDPTR_L = 8'hb8;

	localparam SYS573D_FPGA_NET_DATA      = 8'hc0;
	localparam SYS573D_FPGA_DAC_COUNTER_H = 8'hca;
	localparam SYS573D_FPGA_DAC_COUNTER_L = 8'hcc;
	localparam SYS573D_FPGA_DAC_COUNTER_D = 8'hce;

	localparam SYS573D_FPGA_LIGHTS_AH = 8'he0;
	localparam SYS573D_FPGA_LIGHTS_AL = 8'he2;
	localparam SYS573D_FPGA_LIGHTS_BH = 8'he4;
	localparam SYS573D_FPGA_LIGHTS_D  = 8'he6;
	localparam SYS573D_FPGA_INIT      = 8'he8;
	localparam SYS573D_FPGA_MP3_KEY2  = 8'hea;
	localparam SYS573D_FPGA_MP3_KEY3  = 8'hec;
	localparam SYS573D_FPGA_DS_BUS    = 8'hee;

	/* System clocks */

	wire clock29M, clock19M;

	// ISE rejects global buffer primitives unless they are wired either to an
	// IBUF (which results in suboptimal routing, as the dedicated IOB clock
	// output is left unused) or directly to a pad primitive.
	IPAD   clockPad29M (.IPAD(clockIn29M));
	IPAD   clockPad19M (.IPAD(clockIn19M));
	BUFGLS clockBuf29M (.I(clockIn29M), .O(clock29M));
	BUFGLS clockBuf19M (.I(clockIn19M), .O(clock19M));

	/* Host address decoding */

	// The FPGA shall only respond to addresses in 0x80-0xef range, as 0xf0-0xff
	// is used by the CPLD and 0x00-0x7f seems to be reserved for debugging
	// hardware. Bit 0 of the 573's address bus is not wired to the FPGA as all
	// registers are 16 bits wide.
	wire hostAddrValid = hostAddress[6] & (hostAddress[5:3] != 3'b111);
	wire hostRegRead   = ~nHostEnable & ~nHostRead & nHostWrite;
	wire hostRegWrite  = ~nHostEnable & nHostRead  & ~nHostWrite;

	wire [7:0] hostRegister = { 1'b1, hostAddress[5:0], 1'b0 };

	/* Host interface */

	reg [2:0] _delayedHostRegRead = 3'b000;
	wire      _hostDataInClock;

	wire [15:0] hostDataIn;
	wor  [15:0] hostDataOut;

	reg hostDataInValid  = 1'b0;
	reg hostDataOutValid = 1'b0;

	// Data is latched in the input flip flops once the 573 *deasserts* either
	// the chip select or the write strobe. Konami's bitstream routes this
	// signal through a global net (_hostDataInClock), possibly since it is
	// asynchronous and not tied to the main clock.
	BUFGLS hostDataInUpdateBuf(.I(hostRegWrite), .O(_hostDataInClock));

	wire hostDataInPending = ~hostRegWrite & hostDataInValid;

	always @(posedge clock29M) begin
		// Konami's bitstream pulls the output flip flops' clock enable low
		// after 3 cycles if the register being read is in 0xa0-0xaf range (i.e.
		// MP3 status and counters), in order to prevent any further updates
		// while the counters are running. A 3-bit shift register is used to
		// implement the delay.
		_delayedHostRegRead <= { _delayedHostRegRead[1:0], hostRegRead };

		// The direction of the bus is only changed on clock edges in order to
		// prevent any data from being output before the output flip flops are
		// updated.
		hostDataInValid  <= hostRegWrite;
		hostDataOutValid <= hostAddrValid & hostRegRead;
	end

	generate
		for (i = 0; i < 16; i = i + 1) begin
			IFDX hostDataInReg (
				.D(hostData[i]),
				.C(~_hostDataInClock),
				.CE(1'b1),
				.Q(hostDataIn[i])
			);
			OFDTX hostDataOutReg (
				.D(hostDataOut[i]),
				.C(clock29M),
				.CE(~_delayedHostRegRead[2]),
				.T(~hostDataOutValid),
				.O(hostData[i])
			);
		end
	endgenerate

	/* SRAM interface (currently unused) */

	assign nSRAMRead   = 1'b1;
	assign nSRAMWrite  = 1'b1;
	assign nSRAMEnable = 1'b1;
	assign sramAddress = 17'h1ffff;

	/* DRAM interface (currently unused) */

	assign dramControl = 12'hfff;
	assign dramAddress = 12'hfff;

	/* MP3 decoder interface (currently unused) */

	assign mp3Reset    = 1'b1;
	assign mp3ClockIn  = 1'b1;
	assign mp3StatusCS = 1'b1;

	assign mp3InSDIN = 1'b1;
	assign mp3InBCLK = 1'b1;
	assign mp3InLRCK = 1'b1;

	assign hostDataOut = (hostRegister == SYS573D_FPGA_MP3_I2C)
		? { 2'h0, mp3SCL, mp3SDA, 12'h000 }
		: 16'h0000;

	reg mp3SDAState = 1'b0;
	reg mp3SCLState = 1'b0;

	always @(posedge clock29M)
		if (hostDataInPending & (hostRegister == SYS573D_FPGA_MP3_I2C)) begin
			mp3SDAState <= hostDataIn[12];
			mp3SCLState <= hostDataIn[13];
		end

	assign mp3SDA = mp3SDAState ? 1'bz : 1'b0;
	assign mp3SCL = mp3SCLState ? 1'bz : 1'b0;

	/* I2S audio output */

	assign dacSDIN = mp3OutSDOUT;
	assign dacBCLK = mp3OutBCLK;
	assign dacLRCK = mp3OutLRCK;
	assign dacMCLK = mp3ClockOut;

	/* Magic number */

	assign hostDataOut = (hostRegister == SYS573D_FPGA_MAGIC)
		? 16'h573f
		: 16'h0000;

	/* Light outputs */

	generate
		for (i = 0; i < 4; i = i + 1) begin
			reg [3:0] lightBankState = 4'b1111;

			wire dataIn = hostDataIn[i + 12];

			always @(posedge clock29M)
				if (hostDataInPending)
					case (hostRegister)
						SYS573D_FPGA_LIGHTS_AH:
							lightBankState[0] <= dataIn;
						SYS573D_FPGA_LIGHTS_AL:
							lightBankState[1] <= dataIn;
						SYS573D_FPGA_LIGHTS_BH:
							lightBankState[2] <= dataIn;
						SYS573D_FPGA_LIGHTS_D:
							lightBankState[3] <= dataIn;
					endcase

			// Note that XCS40XL IOBs actually have a tristate flip flop, but
			// there seems to be no primitive exposed by ISE to force its usage.
			assign lightBankAH[i] = lightBankState[0] ? 1'bz : 1'b0;
			assign lightBankAL[i] = lightBankState[1] ? 1'bz : 1'b0;
			assign lightBankBH[i] = lightBankState[2] ? 1'bz : 1'b0;
			assign lightBankD[i]  = lightBankState[3] ? 1'bz : 1'b0;
		end
	endgenerate

	/* Serial interfaces (currently unused) */

	assign networkTXEnable = 1'b1;
	assign networkTX       = 1'b1;

	assign serialTX  = 1'b1;
	assign serialRTS = 1'b1;
	assign serialDTR = 1'b1;

	/* 1-wire bus */

	assign hostDataOut = (hostRegister == SYS573D_FPGA_DS_BUS)
		? { 3'h0, ds2401, 3'h0, ds2433, 8'h00 }
		: 16'h0000;

	reg ds2433State = 1'b0;
	reg ds2401State = 1'b0;

	always @(posedge clock29M)
		if (hostDataInPending & (hostRegister == SYS573D_FPGA_DS_BUS)) begin
			ds2433State <= hostDataIn[8];
			ds2401State <= hostDataIn[12];
		end

	// The 1-wire pins are pulled low by writing 1 (rather than 0) to the
	// respective register bits, but not inverted when read.
	assign ds2433 = ds2433State ? 1'b0 : 1'bz;
	assign ds2401 = ds2401State ? 1'b0 : 1'bz;
endmodule
