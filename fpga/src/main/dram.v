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

/* DRAM controller */

`define STATE_IDLE       2'h0
`define STATE_REFRESHING 2'h1
`define STATE_READING    2'h2
`define STATE_WRITING    2'h3

module DRAMController #(
	parameter ROW_WIDTH  = 8,
	parameter COL_WIDTH  = 8,
	parameter DATA_WIDTH = 8,
	parameter NUM_CHIPS  = 1,

	parameter REFRESH_PERIOD = 250,

	parameter _CHIP_INDEX_WIDTH = $clog2(NUM_CHIPS),
	parameter _APB_ADDR_WIDTH   = ROW_WIDTH + COL_WIDTH + _CHIP_INDEX_WIDTH,
	parameter _DRAM_ADDR_WIDTH  = (ROW_WIDTH > COL_WIDTH) ? ROW_WIDTH : COL_WIDTH,
	parameter _REF_TIMER_WIDTH  = $clog2(REFRESH_PERIOD + 1)
) (
	input clk,
	input reset,

	input                              apbEnable,
	input                              apbWrite,
	output                             apbReady,
	input      [_APB_ADDR_WIDTH - 1:0] apbAddr,
	output reg [DATA_WIDTH      - 1:0] apbRData,
	input      [DATA_WIDTH      - 1:0] apbWData,

	output reg                          nDRAMRead  = 1'b1,
	output reg                          nDRAMWrite = 1'b1,
	output reg [NUM_CHIPS        - 1:0] nDRAMRAS   = {NUM_CHIPS{1'b1}},
	output reg [NUM_CHIPS        - 1:0] nDRAMCAS   = {NUM_CHIPS{1'b1}},
	output reg [_DRAM_ADDR_WIDTH - 1:0] dramAddr   = {_DRAM_ADDR_WIDTH{1'b1}},
	inout      [DATA_WIDTH       - 1:0] dramData
);
	genvar i;

	/* State register */

	reg [1:0] currentState = `STATE_IDLE;

	wire isIdle       = (currentState == `STATE_IDLE);
	wire isRefreshing = (currentState == `STATE_REFRESHING);
	wire isReading    = (currentState == `STATE_READING);
	wire isWriting    = (currentState == `STATE_WRITING);

	/* Counters */

	wire [2:0] stateCycle;
	wire       isLastCycle = (stateCycle == 3'h5);

	wire refreshTimeout;
	wire refreshTrigger = refreshTimeout & (isIdle | isLastCycle);

	Counter #(
		.ID   ("DRAMController.stateCounter"),
		.WIDTH(3)
	) stateCounter (
		.clk    (clk),
		.reset  (reset),
		.countEn(~isIdle),

		.loadEn  (isLastCycle),
		.valueIn (3'h0),
		.valueOut(stateCycle)
	);
	Counter #(
		.ID   ("DRAMController.refreshTimer"),
		.WIDTH(_REF_TIMER_WIDTH)
	) refreshTimer (
		.clk    (clk),
		.reset  (reset),
		.countEn(~refreshTimeout),

		.loadEn  (refreshTimeout & isRefreshing),
		.valueIn ((1 << _REF_TIMER_WIDTH) - REFRESH_PERIOD),
		.carryOut(refreshTimeout)
	);

	/* State machine */

	assign apbReady = isLastCycle & (isReading | isWriting);

	always @(posedge clk, posedge reset) begin
		if (reset)
			currentState <= `STATE_IDLE;
		else if (refreshTrigger)
			// Refreshing is always given priority over reading or writing.
			currentState <= `STATE_REFRESHING;
		else if (isLastCycle)
			currentState <= `STATE_IDLE;
		else if (isIdle & apbEnable)
			currentState <= apbWrite ? `STATE_WRITING : `STATE_READING;
	end

	/* Address and data buffers */

	wire [ROW_WIDTH - 1:0] rowIndex = apbAddr[ROW_WIDTH - 1:0];
	wire [COL_WIDTH - 1:0] colIndex =
		apbAddr[(ROW_WIDTH + COL_WIDTH) - 1:ROW_WIDTH];

	reg dramDataDir = 1'b0;

	assign dramData = dramDataDir ? apbWData : {DATA_WIDTH{1'bz}};

	/* Control signal generator */

	/*
	 * All DRAM accesses are performed in 6 cycles. Read and write transactions
	 * are done by asserting /CAS after /RAS for a single chip, alongside either
	 * /OE (nDRAMRead) or /WE (nDRAMWrite).
	 *             __    __    __    __    __    __
	 * clk        |  |__|  |__|  |__|  |__|  |__|  |__
	 *            __________________             _____
	 * nDRAMRead                    \___________/
	 *            ______                         _____
	 * nDRAMWrite       \_______________________/
	 *            ______                         _____
	 * nDRAMRAS         \_______________________/
	 *            __________________             _____
	 * nDRAMCAS                     \___________/
	 *             ___________ ___________ ___________
	 * dramAddr   <____Row____X__Column___X___________
	 */
	wire nReadStrobe  = 1'b1
		& (stateCycle != 3'h3)
		& (stateCycle != 3'h4);
	wire nWriteStrobe = 1'b0
		| (stateCycle == 3'h0)
		| (stateCycle == 3'h5);

	wire [NUM_CHIPS - 1:0] nReadWriteRAS;
	wire [NUM_CHIPS - 1:0] nReadWriteCAS;

	generate
		if (_CHIP_INDEX_WIDTH) begin
			wire [_CHIP_INDEX_WIDTH - 1:0] chipIndex = apbAddr[
				_APB_ADDR_WIDTH - 1:(ROW_WIDTH + COL_WIDTH)
			];

			for (i = 0; i < NUM_CHIPS; i = i + 1) begin
				wire cs = (chipIndex == i);

				assign nReadWriteRAS[i] = ~(cs & ~nWriteStrobe);
				assign nReadWriteCAS[i] = ~(cs & ~nReadStrobe);
			end
		end else begin
			assign nReadWriteRAS[0] = nWriteStrobe;
			assign nReadWriteCAS[0] = nReadStrobe;
		end
	endgenerate

	wire [_DRAM_ADDR_WIDTH - 1:0] readWriteAddr = stateCycle[1]
		? { {_DRAM_ADDR_WIDTH - COL_WIDTH{1'b0}}, colIndex }
		: { {_DRAM_ADDR_WIDTH - ROW_WIDTH{1'b0}}, rowIndex };

	/*
	 * Refreshing is handled slightly differently and issued to all chips at
	 * once as a /CAS-before-/RAS sequence. Providing a valid address is not
	 * necessary as a refresh counter is built into each DRAM chip.
	 *             __    __    __    __    __    __
	 * clk        |  |__|  |__|  |__|  |__|  |__|  |__
	 *            ____________                   _____
	 * nDRAMRAS               \_________________/
	 *            ______                   ___________
	 * nDRAMCAS         \_________________/
	 */
	wire nRefreshRAS = 1'b0
		| (stateCycle == 3'h0)
		| (stateCycle == 3'h1)
		| (stateCycle == 3'h5);
	wire nRefreshCAS = 1'b0
		| (stateCycle == 3'h0)
		| (stateCycle == 3'h4)
		| (stateCycle == 3'h5);

	always @(posedge clk) begin
		nDRAMRead   <= ~isReading | nReadStrobe;
		nDRAMWrite  <= ~isWriting | nWriteStrobe;
		nDRAMRAS    <= isIdle |
			(isRefreshing ? {NUM_CHIPS{nRefreshRAS}} : nReadWriteRAS);
		nDRAMCAS    <= isIdle |
			(isRefreshing ? {NUM_CHIPS{nRefreshCAS}} : nReadWriteCAS);
		dramAddr    <= readWriteAddr;
		dramDataDir <= isWriting;

		if (stateCycle == 3'h4)
			apbRData <= dramData;
	end
endmodule

/* DRAM access arbiter */

module DRAMArbiter #(
	parameter ADDR_WIDTH  = 32,
	parameter DATA_WIDTH  = 32,
	parameter ROUND_ROBIN = 1
) (
	input clk,
	input reset,

	input [ADDR_WIDTH - 1:0] addrValue,
	input                    mainWAddrLoad,
	input                    mainRAddrLoad,
	input                    auxRAddrLoad,

	output     [ADDR_WIDTH - 1:0] mainWAddr,
	input      [DATA_WIDTH - 1:0] mainWData,
	input                         mainWReq,
	output reg                    mainWReady = 1'b1,

	output     [ADDR_WIDTH - 1:0] mainRAddr,
	output reg [DATA_WIDTH - 1:0] mainRData,
	input                         mainRAck,
	output reg                    mainRReady = 1'b1,

	output     [ADDR_WIDTH - 1:0] auxRAddr,
	output reg [DATA_WIDTH - 1:0] auxRData,
	input                         auxRAck,
	output reg                    auxRReady = 1'b1,

	output reg                    apbEnable = 1'b0,
	output                        apbWrite,
	input                         apbReady,
	output reg [ADDR_WIDTH - 1:0] apbAddr,
	input      [DATA_WIDTH - 1:0] apbRData,
	output reg [DATA_WIDTH - 1:0] apbWData
);
	/* Address counters */

	wire mainWAddrInc, mainRAddrInc, auxRAddrInc;

	Counter #(
		.ID   ("DRAMArbiter.mainWAddrReg"),
		.WIDTH(ADDR_WIDTH)
	) mainWAddrReg (
		.clk    (clk),
		.reset  (reset),
		.countEn(mainWAddrInc),

		.loadEn  (mainWAddrLoad),
		.valueIn (addrValue),
		.valueOut(mainWAddr)
	);
	Counter #(
		.ID   ("DRAMArbiter.mainRAddrReg"),
		.WIDTH(ADDR_WIDTH)
	) mainRAddrReg (
		.clk    (clk),
		.reset  (reset),
		.countEn(mainRAddrInc),

		.loadEn  (mainRAddrLoad),
		.valueIn (addrValue),
		.valueOut(mainRAddr)
	);
	Counter #(
		.ID   ("DRAMArbiter.auxRAddrReg"),
		.WIDTH(ADDR_WIDTH)
	) auxRAddrReg (
		.clk    (clk),
		.reset  (reset),
		.countEn(auxRAddrInc),

		.loadEn  (auxRAddrLoad),
		.valueIn (addrValue),
		.valueOut(auxRAddr)
	);

	/* Arbitration logic */

	wire doMainWrite, doMainRead, doAuxRead, isBusy;

	generate
		if (ROUND_ROBIN) begin
			// Use a Gray counter to cycle through all ready bits. This will
			// interleave read and write transactions, preventing higher
			// priority burst transfers from starving lower priority ones, at
			// the cost of more complexity and latency.
			reg [1:0] roundRobinReg = 2'b00;

			assign doMainWrite = ~mainWReady & (roundRobinReg == 2'b00);
			assign doMainRead  = ~mainRReady & (roundRobinReg == 2'b01);
			assign doAuxRead   = ~auxRReady  & (roundRobinReg == 2'b11);
			assign isBusy      = doMainWrite | doMainRead | doAuxRead;

			always @(posedge clk, posedge reset) begin
				if (reset)
					roundRobinReg <= 2'b00;
				else if (~isBusy)
					roundRobinReg <= { roundRobinReg[0], ~roundRobinReg[1] };
			end
		end else begin
			// Choose which transaction to process based on its priority; writes
			// take priority over main bus reads, which in turn take priority
			// over auxiliary bus reads.
			assign doMainWrite = ~mainWReady;
			assign doMainRead  =  mainWReady & ~mainRReady;
			assign doAuxRead   =  mainWReady &  mainRReady & ~auxRReady;
			assign isBusy      = ~mainWReady | ~mainRReady | ~auxRReady;
		end
	endgenerate

	/* State machine */

	wire isLastCycle = apbEnable & apbReady;

	// Read pointers are incremented immediately when the respective read is
	// acknowledged as the arbiter moves onto caching the next word, while the
	// write pointer is only updated after the transaction is carried out.
	assign apbWrite     = doMainWrite;
	assign mainWAddrInc = doMainWrite & isLastCycle;
	assign mainRAddrInc = mainRReady  & mainRAck;
	assign auxRAddrInc  = auxRReady   & auxRAck;

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			mainWReady <= 1'b1;
			mainRReady <= 1'b1;
			auxRReady  <= 1'b1;
			apbEnable  <= 1'b0;
		end else begin
			apbEnable <= isBusy & ~isLastCycle;

			if (doMainWrite) begin
				mainWReady <= isLastCycle;
				apbAddr    <= mainWAddr;
			end else if (mainWReq) begin
				// Writes are only carried out on demand, so a write request is
				// the only way to invalidate the pointer.
				mainWReady <= 1'b0;
				apbWData   <= mainWData;
			end

			if (doMainRead) begin
				mainRReady <= isLastCycle;
				apbAddr    <= mainRAddr;

				if (isLastCycle)
					mainRData <= apbRData;
			end else if (mainRAck | mainRAddrLoad) begin
				// As the arbiter reads data in advance, read pointers can be
				// invalidated by either acknowledging the last word read (thus
				// incrementing the pointer) or loading a new address.
				mainRReady <= 1'b0;
			end

			if (doAuxRead) begin
				auxRReady <= isLastCycle;
				apbAddr   <= auxRAddr;

				if (isLastCycle)
					auxRData <= apbRData;
			end else if (auxRAck | auxRAddrLoad) begin
				auxRReady <= 1'b0;
			end
		end
	end
endmodule
