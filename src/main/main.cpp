/*
 * 573in1 - Copyright (C) 2022-2025 spicyjpeg
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

#include "common/sys573/base.hpp"
#include "common/util/hash.hpp"
#include "common/util/log.hpp"
#include "common/args.hpp"
#include "common/gpu.hpp"
#include "common/mdec.hpp"
#include "common/pad.hpp"
#include "common/spu.hpp"
#include "main/app/app.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"
#include "ps1/system.h"

enum ExitCode {
	NO_ERROR     = 0,
	INVALID_ARGS = 1
};

int main(int argc, const char **argv) {
	installExceptionHandler();
	gpu::init();
	spu::init();
	mdec::init();
	pad::init();
	sys573::init();
	util::zipCRC32.init();

	args::MainArgs args;

	for (; argc > 0; argc--)
		args.parseArgument(*(argv++));

	util::logger.setupSyslog(args.baudRate);

	// A pointer to the resource package is always provided on the command line
	// by the boot stub.
	if (!args.resourcePtr || !args.resourceLength) {
		LOG_APP("required arguments missing");
		return INVALID_ARGS;
	}

	auto gpuCtx = new gpu::Context(
		GP1_MODE_NTSC, args.screenWidth, args.screenHeight, args.forceInterlace
	);
	auto uiCtx  = new ui::Context(*gpuCtx);
	auto app    = new App(*uiCtx);

	gpu::enableDisplay(true);
	spu::setMasterVolume(spu::MAX_VOLUME / 2);
	sys573::setMiscOutput(sys573::MISC_OUT_SPU_ENABLE, true);

	app->run(args.resourcePtr, args.resourceLength);

	delete app;
	delete uiCtx;
	delete gpuCtx;

	uninstallExceptionHandler();
	return NO_ERROR;
}
