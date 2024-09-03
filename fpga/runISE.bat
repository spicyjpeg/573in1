@echo off
setlocal

rem Modifying the settings below may yield significant improvements to the
rem timings (but also major regressions).
set COVER_MODE=speed
set OPTIMIZATION_MODE=speed
set OPTIMIZATION_LEVEL=normal
set PLACER_EFFORT=3
set ROUTER_EFFORT=3

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
	|| exit /b 2
map -o mapped.ncd synth.ngd mapped.pcf ^
	-cm %COVER_MODE% ^
	-os %OPTIMIZATION_MODE% ^
	-oe %OPTIMIZATION_LEVEL% ^
	-pr b ^
	-detail ^
	|| exit /b 2

par mapped.ncd fpga.ncd mapped.pcf ^
	-w ^
	-pl %PLACER_EFFORT% ^
	-rl %ROUTER_EFFORT% ^
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
