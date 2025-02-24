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

#include "common/nvram/region.hpp"

namespace nvram {

/* Utilities */

const char *const REGION_ERROR_NAMES[]{
	"NO_ERROR",
	"UNSUPPORTED_OP",
	"NO_DEVICE",
	"CHIP_TIMEOUT",
	"CHIP_ERROR",
	"VERIFY_MISMATCH",
	"WRITE_PROTECTED"
};

}
