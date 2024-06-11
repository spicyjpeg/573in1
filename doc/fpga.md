
# Digital I/O board FPGA bitstream

## Overview

The System 573's digital I/O board has the bulk of its logic split across two
different chips:

- an XCS40XL Spartan-XL FPGA, implementing pretty much all of the board's
  functionality and driving most of the light outputs;
- an XC9536 CPLD, responsible for driving the remaining outputs and bringing up
  the FPGA.

While the CPLD is factory-programmed and its registers can be accessed without
any prior initialization, the FPGA must be configured by uploading a bitstream
prior to accessing anything connected to it. This includes the DS2401 that holds
the board's identifier, so a bitstream is required by the tool even though it
does not otherwise make use of the MP3 decoder, additional RAM or any other
hardware on the board.

The `fpga` directory contains the source code for a simple bitstream that
implements a small subset of the functionality provided by Konami's bitstreams,
allowing the tool to control light outputs and read the DS2401 without having to
redistribute any files extracted from games. See below for instructions on
building it.

For more information about the board's hardware and wiring, see:

- [Digital I/O board](https://psx-spx.consoledev.net/konamisystem573/#digital-io-board-gx894-pwbba)
- [XCS40XL FPGA pin mapping](https://psx-spx.consoledev.net/konamisystem573/#xcs40xl-fpga-pin-mapping)

## Register map

### `0x1f640080`: Magic number

| Bits | RW | Description             |
| ---: | :- | :---------------------- |
| 0-15 | R  | Magic number (`0x573f`) |

Note that the number is different from the one used by Konami (`0x1234`).

### `0x1f6400e0`: Light output bank A

| Bits | RW | Description                          |
| ---: | :- | :----------------------------------- |
| 0-11 |    | _Unused_                             |
|   12 | W  | Output A4 (0 = grounded, 1 = high-z) |
|   13 | W  | Output A5 (0 = grounded, 1 = high-z) |
|   14 | W  | Output A6 (0 = grounded, 1 = high-z) |
|   15 | W  | Output A7 (0 = grounded, 1 = high-z) |

### `0x1f6400e2`: Light output bank A

| Bits | RW | Description                          |
| ---: | :- | :----------------------------------- |
| 0-11 |    | _Unused_                             |
|   12 | W  | Output A0 (0 = grounded, 1 = high-z) |
|   13 | W  | Output A1 (0 = grounded, 1 = high-z) |
|   14 | W  | Output A2 (0 = grounded, 1 = high-z) |
|   15 | W  | Output A3 (0 = grounded, 1 = high-z) |

### `0x1f6400e4`: Light output bank B

| Bits | RW | Description                          |
| ---: | :- | :----------------------------------- |
| 0-11 |    | _Unused_                             |
|   12 | W  | Output B4 (0 = grounded, 1 = high-z) |
|   13 | W  | Output B5 (0 = grounded, 1 = high-z) |
|   14 | W  | Output B6 (0 = grounded, 1 = high-z) |
|   15 | W  | Output B7 (0 = grounded, 1 = high-z) |

### `0x1f6400e6`: Light output bank D

| Bits | RW | Description                          |
| ---: | :- | :----------------------------------- |
| 0-11 |    | _Unused_                             |
|   12 | W  | Output D0 (0 = grounded, 1 = high-z) |
|   13 | W  | Output D1 (0 = grounded, 1 = high-z) |
|   14 | W  | Output D2 (0 = grounded, 1 = high-z) |
|   15 | W  | Output D3 (0 = grounded, 1 = high-z) |

### `0x1f6400ee` (FPGA, DDR/Mambo bitstream): **1-wire bus**

When read:

| Bits  | RW | Description               |
| ----: | :- | :------------------------ |
|  0-11 |    | _Unused_                  |
|    12 | R  | DS2401 1-wire bus readout |
|    13 | R  | DS2433 1-wire bus readout |
| 14-15 |    | _Unused_                  |

When written:

| Bits  | RW | Description                                                  |
| ----: | :- | :----------------------------------------------------------- |
|  0-11 |    | _Unused_                                                     |
|    12 | W  | Drive DS2401 1-wire bus low (1 = pull to ground, 0 = high-z) |
|    13 | W  | Drive DS2433 1-wire bus low (1 = pull to ground, 0 = high-z) |
| 14-15 |    | _Unused_                                                     |

Bit 13 is mapped to the bus of the (normally unpopulated) DS2433 footprint. It
is currently unclear whether and how Konami's bitstreams expose this bus.

## Building the bitstream

**NOTE**: building the bitstream is *not* required in order to compile the
project as a prebuilt copy is provided in the `data` directory. This section is
only relevant if you wish to modify the source files in the `fpga/src`
directory, for instance to add new functionality.

You will have to obtain and install a copy of Xilinx ISE 4.2 (the last release
to support Spartan-XL devices). The toolchain is Windows only but seems to work
under Wine; the installer does not, however it is possible to sidestep it by
manually invoking the Java-based extractor included in the installer as follows:

```bash
# Replace /opt/xilinx with a suitable target location and run from the
# installation package's root
find car -iname '*.car' -exec \
    java -cp ce/CarExpand.jar:ce/marimba.zip:ce/tuner.zip \
    com.xilinx.carexp.CarExp '{}' /opt/xilinx \;
```

Due to ISE's limitations, the full absolute path to the target directory
(`C:\Xilinx` by default) must be less than 64 characters long and cannot contain
any spaces. You will additionally need a recent version of
[Yosys](https://github.com/YosysHQ/yosys), which can be installed as part of
the [OSS CAD Suite](https://github.com/YosysHQ/oss-cad-suite-build#installation)
and should be added to the `PATH` environment variable.

Once both are installed, you may synthesize the bitstream by running the
following commands from the project's `fpga` directory (replace the ISE path
appropriately):

```bash
# Windows
set XILINX=C:\Xilinx
mkdir build
yosys fpga.ys
.\runISE.bat

# Linux (using Wine)
export XILINX=/opt/xilinx
mkdir -p build
yosys fpga.ys
wine runISE.bat
```

The bitstream can then be visually inspected using the ISE FPGA editor:

```bash
"%XILINX%\bin\nt\fpga_editor.exe" build\fpga.ncd       # Windows
wine "$XILINX/bit/nt/fpga_editor.exe" build/fpga.ncd   # Linux (using Wine)
```
