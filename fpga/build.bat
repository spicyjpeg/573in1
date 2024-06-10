@echo off
setlocal

set BUILD_DIR="build"

where yosys >nul 2>&1
if errorlevel 1 (
	echo Yosys ^(https://github.com/YosysHQ/yosys^) must be installed and ^
added to PATH in order to run this script.
	exit /b 1
)
if not exist "%XILINX%\bin\nt\" (
	echo The XILINX environment variable must be set to the root of a valid ^
Xilinx ISE 3.3 ^(Windows^) installation in order to run this script. Note that ^
the path cannot contain spaces due to ISE limitations.
	exit /b 1
)

cd /d "%~dp0"

if exist "%BUILD_DIR%" (
	del /q "%BUILD_DIR%"
)
mkdir "%BUILD_DIR%"

yosys ^
	-p "read_verilog -lib -specify %XILINX%\verilog\src\iSE\unisim_comp.v" ^
	-p "read_verilog -lib -specify %XILINX%\verilog\src\glbl.v" ^
	-p "read_verilog %XILINX%\verilog\src\iSE\spartan\spartan_macro.v" ^
	-p "script fpga.ys" ^
	|| exit /b 2

cmd /c runISE.bat ^
	|| exit /b 3

endlocal
exit /b 0
