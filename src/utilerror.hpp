
#pragma once

#include "cartio.hpp"
#include "ide.hpp"
#include "util.hpp"
#include "vendor/ff.h"
#include "vendor/miniz.h"

namespace util {

static inline const char *getErrorString(cart::DriverError error) {
	return CART_DRIVER_ERROR_NAMES[error];
}

static inline const char *getErrorString(ide::DeviceError error) {
	return IDE_DEVICE_ERROR_NAMES[error];
}

static inline const char *getErrorString(FRESULT error) {
	return FATFS_ERROR_NAMES[error];
}

static inline const char *getErrorString(int error) {
	return MINIZ_ERROR_NAMES[error - MZ_VERSION_ERROR];
}

static inline const char *getErrorString(mz_zip_error error) {
	return MINIZ_ZIP_ERROR_NAMES[error];
}

}
