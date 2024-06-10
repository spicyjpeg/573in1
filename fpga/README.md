
# Digital I/O board FPGA bitstream

This directory contains the source files used to generate the FPGA configuration
bitstream uploaded by the tool to the system's digital I/O board (if present). A
prebuilt copy of the bitstream is provided in the `data` directory, so
recompiling it manually is not necessary in order to build the tool.

See [`doc/fpga.md`](../doc/fpga.md) for more details on the registers
implemented by the bitstream as well as building instructions.
