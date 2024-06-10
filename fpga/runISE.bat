@echo off
setlocal

set TARGET=xcs40xl-pq208-4
set COVER_MODE=area
set OPTIMIZATION_MODE=speed
set OPTIMIZATION_LEVEL=high

cd /d "%~dp0\build"

ngdbuild synth.edf synth.ngd ^
	-uc ..\fpga.ucf ^
	-p %TARGET% ^
	|| exit /b 1
map -o mapped.ncd synth.ngd ^
	-p %TARGET% ^
	-cm %COVER_MODE% ^
	-os %OPTIMIZATION_MODE% ^
	-oe %OPTIMIZATION_LEVEL% ^
	-pr b ^
	-detail ^
	|| exit /b 1
par mapped.ncd fpga.ncd ^
	-w ^
	-detail ^
	|| exit /b 1
xdl -ncd2xdl fpga.ncd fpga.xdl ^
	|| exit /b 1
bitgen fpga.ncd fpga.bit ^
	-w ^
	-g DonePin:Pullup ^
	-g PowerDown:Pullup ^
	-g ReadCapture:Disable ^
	-g ExpressMode:Disable ^
	-g 5V_Tolerant_IO:On ^
	|| exit /b 1

endlocal
exit /b 0
