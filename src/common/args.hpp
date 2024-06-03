
#pragma once

#include <stddef.h>
#include "common/file/file.hpp"
#include "common/util.hpp"

namespace args {

/* Command line argument parsers */

static constexpr char VALUE_SEPARATOR = '=';

static constexpr int DEFAULT_BAUD_RATE     = 115200;
static constexpr int DEFAULT_SCREEN_WIDTH  = 320;
static constexpr int DEFAULT_SCREEN_HEIGHT = 240;

static constexpr size_t MAX_LAUNCHER_FRAGMENTS = 64;

class CommonArgs {
public:
	int baudRate;

#ifdef NDEBUG
	inline CommonArgs(void)
	: baudRate(0) {}
#else
	// Enable serial port logging by default in debug builds.
	inline CommonArgs(void)
	: baudRate(DEFAULT_BAUD_RATE) {}
#endif

	bool parseArgument(const char *arg);
};

class MainArgs : public CommonArgs {
public:
	int        screenWidth, screenHeight;
	bool       forceInterlace;
	const void *resourcePtr;
	size_t     resourceLength;

	inline MainArgs(void)
	: screenWidth(DEFAULT_SCREEN_WIDTH), screenHeight(DEFAULT_SCREEN_HEIGHT),
	forceInterlace(false), resourcePtr(nullptr), resourceLength(0) {}

	bool parseArgument(const char *arg);
};

class ExecutableLauncherArgs : public CommonArgs {
public:
	void *entryPoint, *initialGP, *stackTop;

	void *loadAddress;
	int  drive;

	size_t             numArgs, numFragments;
	const char         *executableArgs[util::MAX_EXECUTABLE_ARGS];
	file::FileFragment fragments[MAX_LAUNCHER_FRAGMENTS];

	inline ExecutableLauncherArgs(void)
	: entryPoint(nullptr), initialGP(nullptr), stackTop(nullptr),
	loadAddress(nullptr), drive(0), numArgs(0), numFragments(0) {}

	bool parseArgument(const char *arg);
};

}
