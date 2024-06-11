@echo off
setlocal

set TARGET=xcs40xl-pq208-4
set COVER_MODE=area
set OPTIMIZATION_MODE=speed
set OPTIMIZATION_LEVEL=high

if not exist "%XILINX%\bin\nt\" (
	echo The XILINX environment variable must be set to the root of a valid ^
Xilinx ISE ^(Windows^) installation in order to run this script. Note that the ^
path cannot contain spaces due to ISE limitations.
	exit /b 1
)

set PATH="%XILINX%\bin\nt";%PATH%
cd /d "%~dp0\build"

ngdbuild synth.edf synth.ngd ^
	-uc ..\fpga.ucf ^
	-p %TARGET% ^
	|| exit /b 2
map -o mapped.ncd synth.ngd ^
	-p %TARGET% ^
	-cm %COVER_MODE% ^
	-os %OPTIMIZATION_MODE% ^
	-oe %OPTIMIZATION_LEVEL% ^
	-pr b ^
	-detail ^
	|| exit /b 2

par mapped.ncd fpga.ncd ^
	-w ^
	-detail ^
	|| exit /b 3

xdl -ncd2xdl fpga.ncd fpga.xdl ^
	|| exit /b 4
bitgen fpga.ncd fpga.bit ^
	-w ^
	-g DonePin:Pullup ^
	-g TdoPin:Pullup ^
	-g PowerDown:Pullup ^
	-g ReadCapture:Disable ^
	-g ExpressMode:Disable ^
	-g 5V_Tolerant_IO:On ^
	|| exit /b 4

endlocal
exit /b 0
