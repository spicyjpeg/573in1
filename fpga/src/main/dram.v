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

module DRAMController #(
	parameter ROW_WIDTH  = 8,
	parameter COL_WIDTH  = 8,
	parameter DATA_WIDTH = 8,
	parameter NUM_CHIPS  = 1,

	parameter REFRESH_PERIOD = 250,

	parameter _CHIP_INDEX_WIDTH = $clog2(NUM_CHIPS),
	parameter _APB_ADDR_WIDTH   = ROW_WIDTH + COL_WIDTH + _CHIP_INDEX_WIDTH,
	parameter _DRAM_ADDR_WIDTH  = (ROW_WIDTH > COL_WIDTH) ? ROW_WIDTH : COL_WIDTH,
	parameter _REF_TIMER_WIDTH  = $clog2(REFRESH_PERIOD + 1),
	parameter _REFRESH_RELOAD   = {_REF_TIMER_WIDTH{1'b1}} - REFRESH_PERIOD
) (
	input clk,
	input reset,

	input                              apbEnable,
	input                              apbWrite,
	output reg                         apbReady = 1'b0,
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

	/* State machine counters */

	wire [2:0] stateCycle;
	wire       isIdle, refreshTimeout;

	wire trigger      = apbEnable | refreshTimeout;
	reg  isRefreshing = 1'b0;

	always @(posedge clk, posedge reset) begin
		if (reset)
			isRefreshing <= 1'b0;
		else if (isIdle)
			isRefreshing <= refreshTimeout;
	end

	Counter #(
		.ID       ("DRAMController.stateCounter"),
		.WIDTH    (3),
		.INIT     (3'h7),
		.CARRY_OUT(1)
	) stateCounter (
		.clk  (clk),
		.reset(reset),
		.clkEn(~isIdle | trigger),

		.load    (3'h0),
		.valueIn (3'h0),
		.valueOut(stateCycle),
		.carryOut(isIdle)
	);
	Counter #(
		.ID       ("DRAMController.refreshTimer"),
		.WIDTH    (_REF_TIMER_WIDTH),
		.CARRY_OUT(1)
	) refreshTimer (
		.clk  (clk),
		.reset(reset),
		.clkEn(~refreshTimeout | isRefreshing),

		.load    ({_REF_TIMER_WIDTH{isRefreshing}}),
		.valueIn (_REFRESH_RELOAD[_REF_TIMER_WIDTH - 1:0]),
		.carryOut(refreshTimeout)
	);

	/* Address and data buffers */

	wire [ROW_WIDTH - 1:0] rowIndex = apbAddr[ROW_WIDTH - 1:0];
	wire [COL_WIDTH - 1:0] colIndex =
		apbAddr[(ROW_WIDTH + COL_WIDTH) - 1:ROW_WIDTH];

	reg dramDataDir = 1'b0;

	assign dramData = dramDataDir ? apbWData : {DATA_WIDTH{1'bz}};

	/* Control signal sequencer */

	/*
	 * All DRAM accesses are performed in 6 cycles, plus 1 cycle of delay
	 * between sequential accesses. Read and write transactions are done by
	 * asserting /CAS after /RAS for a single chip, alongside either /OE
	 * (nDRAMRead) or /WE (nDRAMWrite).
	 *             __    __    __    __    __    __    __
	 * clk        |  |__|  |__|  |__|  |__|  |__|  |__|  |__|
	 *             _____ _____ _____ _____ _____ _____ _____
	 * stateCycle <__0__X__1__X__2__X__3__X__4__X__5__X__6__>
	 *            __________________             ____________
	 * nDRAMRead                    \___________/
	 *            ______                         ____________
	 * nDRAMWrite       \_______________________/
	 *            ______                         ____________
	 * nDRAMRAS         \_______________________/
	 *            __________________             ____________
	 * nDRAMCAS                     \___________/
	 *             ___________ ___________ __________________
	 * dramAddr   <____Row____X__Column___X__________________
	 *                                           _____
	 * apbReady   ______________________________/     \______
	 *            ______________________________ ____________
	 * apbRData   ______________________________X____Data____
	 */
	wire nReadStrobe  = 1'b1
		& (stateCycle != 3'h2)
		& (stateCycle != 3'h3);
	wire nWriteStrobe = 1'b1
		& (stateCycle != 3'h0)
		& (stateCycle != 3'h1)
		& (stateCycle != 3'h2)
		& (stateCycle != 3'h3);
	wire sendColAddr = 1'b0
		| (stateCycle == 3'h1)
		| (stateCycle == 3'h2);

	wire [NUM_CHIPS        - 1:0] nReadWriteRAS;
	wire [NUM_CHIPS        - 1:0] nReadWriteCAS;
	wire [_DRAM_ADDR_WIDTH - 1:0] dramAddrOut;

	generate
		if (_CHIP_INDEX_WIDTH) begin
			wire [_CHIP_INDEX_WIDTH - 1:0] chipIndex = apbAddr[
				_APB_ADDR_WIDTH - 1:(ROW_WIDTH + COL_WIDTH)
			];

			for (i = 0; i < NUM_CHIPS; i = i + 1) begin
				wire sel = (chipIndex == i);

				assign nReadWriteRAS[i] = ~(sel & ~nWriteStrobe);
				assign nReadWriteCAS[i] = ~(sel & ~nReadStrobe);
			end
		end else begin
			assign nReadWriteRAS[0] = nWriteStrobe;
			assign nReadWriteCAS[0] = nReadStrobe;
		end
	endgenerate

	Multiplexer #(
		.ID   ("DRAMController.dramAddrMux"),
		.WIDTH(_DRAM_ADDR_WIDTH)
	) dramAddrMux (
		.sel(sendColAddr),

		.valueA  ({ {_DRAM_ADDR_WIDTH - ROW_WIDTH{1'b0}}, rowIndex }),
		.valueB  ({ {_DRAM_ADDR_WIDTH - COL_WIDTH{1'b0}}, colIndex }),
		.valueOut(dramAddrOut)
	);

	/*
	 * Refreshing is handled slightly differently and issued to all chips at
	 * once as a /CAS-before-/RAS sequence. Providing a valid address is not
	 * necessary as a refresh counter is built into each DRAM chip.
	 *             __    __    __    __    __    __    __
	 * clk        |  |__|  |__|  |__|  |__|  |__|  |__|  |__|
	 *             _____ _____ _____ _____ _____ _____ _____
	 * stateCycle <__0__X__1__X__2__X__3__X__4__X__5__X__6__>
	 *            ____________                   ____________
	 * nDRAMRAS               \_________________/
	 *            ______                   __________________
	 * nDRAMCAS         \_________________/
	 */
	wire nRefreshRAS = 1'b1
		& (stateCycle != 3'h1)
		& (stateCycle != 3'h2)
		& (stateCycle != 3'h3);
	wire nRefreshCAS = 1'b1
		& (stateCycle != 3'h0)
		& (stateCycle != 3'h1)
		& (stateCycle != 3'h2);

	/* Output registers */

	wire sampleData = ~isRefreshing & (stateCycle == 3'h4);

	always @(posedge clk) begin
		nDRAMRead   <= isRefreshing |  apbWrite | nReadStrobe;
		nDRAMWrite  <= isRefreshing | ~apbWrite | nWriteStrobe;
		nDRAMRAS    <= isRefreshing ? {NUM_CHIPS{nRefreshRAS}} : nReadWriteRAS;
		nDRAMCAS    <= isRefreshing ? {NUM_CHIPS{nRefreshCAS}} : nReadWriteCAS;
		dramAddr    <= dramAddrOut;
		dramDataDir <= apbWrite & ~nWriteStrobe;

		if (sampleData) begin
			apbReady <= 1'b1;
			apbRData <= dramData;
		end else begin
			apbReady <= 1'b0;
		end
	end
endmodule

module DRAMArbiter #(
	parameter ADDR_H_WIDTH = 16,
	parameter ADDR_L_WIDTH = 16,
	parameter DATA_WIDTH   = 32,
	parameter ROUND_ROBIN  = 1,

	parameter _ADDR_WIDTH = ADDR_H_WIDTH + ADDR_L_WIDTH
) (
	input clk,
	input reset,

	input                      mainWAddrHWrite,
	input                      mainWAddrLWrite,
	input                      mainWrite,
	output reg                 mainWReady = 1'b1,
	output [_ADDR_WIDTH - 1:0] mainWAddr,
	input  [_ADDR_WIDTH - 1:0] mainWAddrWData,
	input  [DATA_WIDTH  - 1:0] mainWData,

	input                      mainRAddrHWrite,
	input                      mainRAddrLWrite,
	input                      mainRDataAck,
	output reg                 mainRReady = 1'b1,
	output [_ADDR_WIDTH - 1:0] mainRAddr,
	input  [_ADDR_WIDTH - 1:0] mainRAddrWData,
	output [DATA_WIDTH  - 1:0] mainRData,

	input                      auxRAddrHWrite,
	input                      auxRAddrLWrite,
	input                      auxRDataAck,
	output reg                 auxRReady = 1'b1,
	output [_ADDR_WIDTH - 1:0] auxRAddr,
	input  [_ADDR_WIDTH - 1:0] auxRAddrWData,
	output [DATA_WIDTH  - 1:0] auxRData,

	output reg                 apbEnable = 1'b0,
	output                     apbWrite,
	input                      apbReady,
	output [_ADDR_WIDTH - 1:0] apbAddr,
	input  [DATA_WIDTH  - 1:0] apbRData,
	output [DATA_WIDTH  - 1:0] apbWData
);
	/* Address counters */

	wire mainWAddrInc, mainRAddrInc, auxRAddrInc;

	Counter #(
		.ID      ("DRAMArbiter.mainWAddrReg"),
		.WIDTH   (_ADDR_WIDTH),
		.CARRY_IN(1)
	) mainWAddrReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.carryIn (mainWAddrInc),
		.load    ({
			{ADDR_H_WIDTH{mainWAddrHWrite}},
			{ADDR_L_WIDTH{mainWAddrLWrite}}
		}),
		.valueIn (mainWAddrWData),
		.valueOut(mainWAddr)
	);
	Counter #(
		.ID      ("DRAMArbiter.mainRAddrReg"),
		.WIDTH   (_ADDR_WIDTH),
		.CARRY_IN(1)
	) mainRAddrReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.carryIn (mainRAddrInc),
		.load    ({
			{ADDR_H_WIDTH{mainRAddrHWrite}},
			{ADDR_L_WIDTH{mainRAddrLWrite}}
		}),
		.valueIn (mainRAddrWData),
		.valueOut(mainRAddr)
	);
	Counter #(
		.ID      ("DRAMArbiter.auxRAddrReg"),
		.WIDTH   (_ADDR_WIDTH),
		.CARRY_IN(1)
	) auxRAddrReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),

		.carryIn (auxRAddrInc),
		.load    ({
			{ADDR_H_WIDTH{auxRAddrHWrite}},
			{ADDR_L_WIDTH{auxRAddrLWrite}}
		}),
		.valueIn (auxRAddrWData),
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
					roundRobinReg <= {
						roundRobinReg[0] & ~roundRobinReg[1], ~roundRobinReg[1]
					};
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

	/* Address multiplexer */

	wire [_ADDR_WIDTH - 1:0] currentRAddr;

	assign apbWrite = doMainWrite;

	Multiplexer #(
		.ID   ("DRAMArbiter.apbRAddrMux"),
		.WIDTH(_ADDR_WIDTH)
	) apbRAddrMux (
		.sel(doMainRead),

		.valueA  (auxRAddr),
		.valueB  (mainRAddr),
		.valueOut(currentRAddr)
	);
	MultiplexerReg #(
		.ID   ("DRAMArbiter.apbAddrMux"),
		.WIDTH(_ADDR_WIDTH)
	) apbAddrMux (
		.clk  (clk),
		.reset(reset),
		.clkEn(1'b1),
		.sel  (doMainWrite),

		.valueA  (currentRAddr),
		.valueB  (mainWAddr),
		.valueOut(apbAddr)
	);

	/* Data registers */

	wire dataReady = apbEnable & apbReady;

	Register #(
		.ID   ("DRAMArbiter.mainRDataReg"),
		.WIDTH(DATA_WIDTH)
	) mainRDataReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(doMainRead & dataReady),

		.valueIn (apbRData),
		.valueOut(mainRData)
	);
	Register #(
		.ID   ("DRAMArbiter.auxRDataReg"),
		.WIDTH(DATA_WIDTH)
	) auxRDataReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(doAuxRead & dataReady),

		.valueIn (apbRData),
		.valueOut(auxRData)
	);
	Register #(
		.ID   ("DRAMArbiter.apbWDataReg"),
		.WIDTH(DATA_WIDTH)
	) apbWDataReg (
		.clk  (clk),
		.reset(reset),
		.clkEn(~doMainWrite & mainWrite),

		.valueIn (mainWData),
		.valueOut(apbWData)
	);

	/* State machine */

	// Read pointers are incremented immediately when the respective read is
	// acknowledged as the arbiter moves onto prefetching the next word, while
	// the write pointer is only updated after the transaction is carried out.
	// This also means that settting a read address will invalidate the
	// respective prefetched word.
	wire mainRDataReq = mainRDataAck | mainRAddrHWrite | mainRAddrLWrite;
	wire auxRDataReq  = auxRDataAck  | auxRAddrHWrite  | auxRAddrLWrite;

	assign mainWAddrInc = doMainWrite & dataReady;
	assign mainRAddrInc = mainRReady  & mainRDataAck;
	assign auxRAddrInc  = auxRReady   & auxRDataAck;

	always @(posedge clk, posedge reset) begin
		if (reset) begin
			mainWReady <= 1'b1;
			mainRReady <= 1'b1;
			auxRReady  <= 1'b1;
			apbEnable  <= 1'b0;
		end else begin
			apbEnable <= isBusy & ~dataReady;

			if (doMainWrite)
				mainWReady <= dataReady;
			else if (mainWrite)
				// Writes are only carried out on demand, so a write request is
				// the only way to invalidate the pointer.
				mainWReady <= 1'b0;

			if (doMainRead)
				mainRReady <= dataReady;
			else if (mainRDataReq)
				// As the arbiter reads data in advance, read pointers can be
				// invalidated by either acknowledging the last word read (thus
				// incrementing the pointer) or loading a new address.
				mainRReady <= 1'b0;

			if (doAuxRead)
				auxRReady <= dataReady;
			else if (auxRDataReq)
				auxRReady <= 1'b0;
		end
	end
endmodule
