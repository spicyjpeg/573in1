#!/bin/bash

BUILD_DIR="build"

case "$(uname -s)" in
	CYGWIN*|MINGW*|MSYS*)
		ISE_RUNNER=""
		;;
	*)
		if ! which wine >/dev/null 2>&1; then
			echo \
				"Wine must be installed in order to run this script on" \
				"non-Windows platforms."
			exit 1
		fi

		ISE_RUNNER="wine"
		;;
esac

if ! which yosys >/dev/null 2>&1; then
	echo \
		"Yosys (https://github.com/YosysHQ/yosys) must be installed and added" \
		" to PATH in order to run this script."
	exit 1
fi
if [ ! -d "$XILINX/bin/nt" ]; then
	echo \
		"The XILINX environment variable must be set to the root of a valid" \
		"Xilinx ISE 3.3 (Windows) installation in order to run this script." \
		"Note that the path cannot contain spaces due to ISE limitations."
	exit 1
fi

cd "$(dirname "$0")"
mkdir -p "$BUILD_DIR"

yosys \
	-p "read_verilog -lib -specify $XILINX/verilog/src/glbl.v" \
	-p "read_verilog -lib -specify $XILINX/verilog/src/iSE/unisim_comp.v" \
	-p "read_verilog $XILINX/verilog/src/iSE/spartan/spartan_macro.v" \
	-p "script fpga.ys" \
	|| exit 2

$ISE_RUNNER cmd /c runISE.bat \
	|| exit 3
