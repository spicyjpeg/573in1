
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

	output mp3ClockIn,
	input  mp3ClockOut,

	output mp3Reset,
	input  mp3Ready,
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

	function automatic [5:0] _hostReg (input [7:0] address);
		_hostReg = address[6:1];
	endfunction

	localparam SYS573D_FPGA_MAGIC   = _hostReg(8'h80);
	localparam SYS573D_FPGA_VERSION = _hostReg(8'h82);

	localparam SYS573D_FPGA_MP3_PTR_H     = _hostReg(8'ha0);
	localparam SYS573D_FPGA_MP3_PTR_L     = _hostReg(8'ha2);
	localparam SYS573D_FPGA_MP3_ENDPTR_H  = _hostReg(8'ha4);
	localparam SYS573D_FPGA_MP3_ENDPTR_L  = _hostReg(8'ha6);
	localparam SYS573D_FPGA_MP3_COUNTER   = _hostReg(8'ha8);
	localparam SYS573D_FPGA_MP3_KEY1      = _hostReg(8'ha8);
	localparam SYS573D_FPGA_MP3_FEED_STAT = _hostReg(8'haa);
	localparam SYS573D_FPGA_MP3_I2C       = _hostReg(8'hac);
	localparam SYS573D_FPGA_MP3_FEED_CTRL = _hostReg(8'hae);

	localparam SYS573D_FPGA_DRAM_WRPTR_H = _hostReg(8'hb0);
	localparam SYS573D_FPGA_DRAM_WRPTR_L = _hostReg(8'hb2);
	localparam SYS573D_FPGA_DRAM_DATA    = _hostReg(8'hb4);
	localparam SYS573D_FPGA_DRAM_RDPTR_H = _hostReg(8'hb6);
	localparam SYS573D_FPGA_DRAM_RDPTR_L = _hostReg(8'hb8);

	localparam SYS573D_FPGA_NET_DATA      = _hostReg(8'hc0);
	localparam SYS573D_FPGA_DAC_COUNTER_H = _hostReg(8'hca);
	localparam SYS573D_FPGA_DAC_COUNTER_L = _hostReg(8'hcc);
	localparam SYS573D_FPGA_DAC_COUNTER_D = _hostReg(8'hce);

	localparam SYS573D_FPGA_LIGHTS_AH = _hostReg(8'he0);
	localparam SYS573D_FPGA_LIGHTS_AL = _hostReg(8'he2);
	localparam SYS573D_FPGA_LIGHTS_BH = _hostReg(8'he4);
	localparam SYS573D_FPGA_LIGHTS_D  = _hostReg(8'he6);
	localparam SYS573D_FPGA_INIT      = _hostReg(8'he8);
	localparam SYS573D_FPGA_MP3_KEY2  = _hostReg(8'hea);
	localparam SYS573D_FPGA_MP3_KEY3  = _hostReg(8'hec);
	localparam SYS573D_FPGA_DS_BUS    = _hostReg(8'hee);

	/* System clocks */

	// ISE rejects global buffer primitives unless they are wired either to an
	// IBUF (which results in suboptimal routing, as the dedicated IOB clock
	// output is left unused) or directly to a pad primitive.
	wire   clock29M;
	IPAD   _clockPad29M (.IPAD(clockIn29M));
	BUFGLS _clockBuf29M (.I(clockIn29M), .O(clock29M));

	wire   clock19M;
	IPAD   _clockPad19M (.IPAD(clockIn19M));
	BUFGLS _clockBuf19M (.I(clockIn19M), .O(clock19M));

	/* Host interface */

	wire   _hostWriteLatchIn = ~nHostWrite & ~nHostEnable;
	wire   _hostWriteLatch;
	BUFGLS _hostWriteLatchBuf (.I(_hostWriteLatchIn), .O(_hostWriteLatch));

	reg [6:0]  hostRegAddress;
	reg [15:0] hostReadData;
	reg [15:0] hostWriteData;

	always @(posedge clock29M)
		hostRegAddress <= hostAddress;
	always @(negedge _hostWriteLatch)
		hostWriteData  <= hostData;

	/* Read/write strobe edge detector */

	reg [1:0] _hostReadState  = 2'b00;
	reg [1:0] _hostWriteState = 2'b00;

	always @(posedge clock29M) begin
		_hostReadState  <= { _hostReadState[0],  ~nHostRead  & ~nHostEnable };
		_hostWriteState <= { _hostWriteState[0], ~nHostWrite & ~nHostEnable };
	end

	wire hostReadAsserted  = (_hostReadState  == 2'b01);
	wire hostReadReleased  = (_hostReadState  == 2'b10);
	wire hostWriteAsserted = (_hostWriteState == 2'b01);
	wire hostWriteReleased = (_hostWriteState == 2'b10);

	// The FPGA shall only respond to addresses in 0x80-0xef range, as 0xf0-0xff
	// is used by the CPLD and 0x00-0x7f seems to be reserved for debugging
	// hardware. Bit 0 of the 573's address bus is not wired to the FPGA as all
	// registers are 16 bits wide.
	reg    _hostDataDir = 1'b0;
	assign hostData     = _hostDataDir ? hostReadData : 16'hzzzz;

	always @(posedge clock29M)
		_hostDataDir <= 1'b1
			& ~nHostRead
			& ~nHostEnable
			& hostAddress[6]
			& (hostAddress[5:3] != 3'b111);

	/* SRAM interface (currently unused) */

	assign nSRAMRead   = 1'b1;
	assign nSRAMWrite  = 1'b1;
	assign nSRAMEnable = 1'b1;
	assign sramAddress = 17'h1ffff;

	/* DRAM interface (currently unused) */

	assign dramControl = 12'hfff;
	assign dramAddress = 12'hfff;

	/* MP3 decoder interface (currently unused) */

	reg mp3SDAIn;
	reg mp3SDAOut = 1'b1;
	reg mp3SCLIn;
	reg mp3SCLOut = 1'b1;

	assign mp3ClockIn  = 1'b1;
	assign mp3Reset    = 1'b1;
	assign mp3StatusCS = 1'b1;

	assign mp3InSDIN = 1'b1;
	assign mp3InBCLK = 1'b1;
	assign mp3InLRCK = 1'b1;

	assign mp3SDA = mp3SDAOut ? 1'bz : 1'b0;
	assign mp3SCL = mp3SCLOut ? 1'bz : 1'b0;

	always @(posedge clock29M) begin
		mp3SDAIn <= mp3SDA;
		mp3SCLIn <= mp3SCL;
	end

	/* I2S audio output */

	assign dacSDIN = mp3OutSDOUT;
	assign dacBCLK = mp3OutBCLK;
	assign dacLRCK = mp3OutLRCK;
	assign dacMCLK = mp3ClockOut;

	/* Light outputs */

	reg [3:0] lightBankAHState = 4'b1111;
	reg [3:0] lightBankALState = 4'b1111;
	reg [3:0] lightBankBHState = 4'b1111;
	reg [3:0] lightBankDState  = 4'b1111;

	generate
		for (i = 0; i < 4; i = i + 1) begin
			// Note that XCS40XL IOBs actually have a built-in tristate flip
			// flop, but ISE will not use it here as it does not have a clock
			// enable input (used to gate writes to the state registers) and
			// cannot be read back nor configured to be set on startup.
			assign lightBankAH[i] = lightBankAHState[i] ? 1'bz : 1'b0;
			assign lightBankAL[i] = lightBankALState[i] ? 1'bz : 1'b0;
			assign lightBankBH[i] = lightBankBHState[i] ? 1'bz : 1'b0;
			assign lightBankD[i]  = lightBankDState[i]  ? 1'bz : 1'b0;
		end
	endgenerate

	/* Serial interfaces (currently unused) */

	assign networkTXEnable = 1'b1;
	assign networkTX       = 1'b1;

	assign serialTX  = 1'b1;
	assign serialRTS = 1'b1;
	assign serialDTR = 1'b1;

	/* 1-wire bus */

	reg ds2433In;
	reg ds2433Out = 1'b0;
	reg ds2401In;
	reg ds2401Out = 1'b0;

	// The 1-wire pins are pulled low by writing 1 (rather than 0) to the
	// respective register bits, but not inverted when read.
	assign ds2433 = ds2433Out ? 1'b0 : 1'bz;
	assign ds2401 = ds2401Out ? 1'b0 : 1'bz;

	always @(posedge clock29M) begin
		ds2433In <= ds2433;
		ds2401In <= ds2401;
	end

	/* Host registers */

	always @(posedge clock29M)
		case (hostRegAddress[5:0])
			SYS573D_FPGA_MAGIC: begin
				hostReadData <= 16'h573f;
			end

			SYS573D_FPGA_VERSION: begin
				hostReadData <= 16'h0001;
			end

			SYS573D_FPGA_MP3_I2C: begin
				hostReadData <= { 2'h0, mp3SCLIn, mp3SDAIn, 12'h000 };

				if (hostWriteReleased) begin
					mp3SDAOut <= hostWriteData[12];
					mp3SCLOut <= hostWriteData[13];
				end
			end

			SYS573D_FPGA_LIGHTS_AH: begin
				hostReadData <= { lightBankAHState, 12'h000 };

				if (hostWriteReleased)
					lightBankAHState <= hostWriteData[15:12];
			end

			SYS573D_FPGA_LIGHTS_AL: begin
				hostReadData <= { lightBankALState, 12'h000 };

				if (hostWriteReleased)
					lightBankALState <= hostWriteData[15:12];
			end

			SYS573D_FPGA_LIGHTS_BH: begin
				hostReadData <= { lightBankBHState, 12'h000 };

				if (hostWriteReleased)
					lightBankBHState <= hostWriteData[15:12];
			end

			SYS573D_FPGA_LIGHTS_D: begin
				hostReadData <= { lightBankDState, 12'h000 };

				if (hostWriteReleased)
					lightBankDState <= hostWriteData[15:12];
			end

			SYS573D_FPGA_DS_BUS: begin
				hostReadData <= { 3'h0, ds2401In, 3'h0, ds2433In, 8'h00 };

				if (hostWriteReleased) begin
					ds2433Out <= hostWriteData[8];
					ds2401Out <= hostWriteData[12];
				end
			end

			default: begin
				hostReadData <= 16'hxxxx;
			end
		endcase
endmodule
