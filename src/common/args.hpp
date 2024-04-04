
#pragma once

#include <stddef.h>
#include "common/util.hpp"

namespace args {

/* Command line argument parsers */

static constexpr char VALUE_SEPARATOR = '=';

static constexpr int DEFAULT_BAUD_RATE     = 115200;
static constexpr int DEFAULT_SCREEN_WIDTH  = 320;
static constexpr int DEFAULT_SCREEN_HEIGHT = 240;

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
	int        argCount;
	const char *drive, *path;
	const char *executableArgs[util::MAX_EXECUTABLE_ARGS];

	inline ExecutableLauncherArgs(void)
	: argCount(0), drive(nullptr), path(nullptr) {}

	bool parseArgument(const char *arg);
};

}
