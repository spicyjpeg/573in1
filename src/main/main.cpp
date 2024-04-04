
#include "common/args.hpp"
#include "common/file.hpp"
#include "common/gpu.hpp"
#include "common/io.hpp"
#include "common/spu.hpp"
#include "common/util.hpp"
#include "main/app/app.hpp"
#include "main/uibase.hpp"
#include "ps1/gpucmd.h"
#include "ps1/system.h"

int main(int argc, const char **argv) {
	installExceptionHandler();
	gpu::init();
	spu::init();
	io::init();
	util::initZipCRC32();

	args::MainArgs args;

	for (; argc > 0; argc--)
		args.parseArgument(*(argv++));

#ifdef ENABLE_LOGGING
	util::logger.setupSyslog(args.baudRate);
#endif

	// A pointer to the resource archive is always provided on the command line
	// by the boot stub.
	if (!args.resourcePtr || !args.resourceLength) {
		LOG("required arguments missing");
		return 1;
	}

	auto resourceProvider = new file::ZIPProvider;

	resourceProvider->init(args.resourcePtr, args.resourceLength);
	io::clearWatchdog();

	auto gpuCtx = new gpu::Context(
		GP1_MODE_NTSC, args.screenWidth, args.screenHeight, args.forceInterlace
	);
	auto uiCtx  = new ui::Context(*gpuCtx);
	auto app    = new App(*uiCtx, *resourceProvider);

	gpu::enableDisplay(true);
	spu::setMasterVolume(spu::MAX_VOLUME);
	io::setMiscOutput(io::MISC_SPU_ENABLE, true);
	app->run();

	delete app;
	delete uiCtx;
	delete gpuCtx;

	uninstallExceptionHandler();
	return 0;
}
