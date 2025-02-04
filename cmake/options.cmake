# 573in1 - Copyright (C) 2022-2025 spicyjpeg
#
# 573in1 is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# 573in1 is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# 573in1. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.25)

## External tools

find_program(
	CHDMAN_PATH chdman
	PATHS
		"C:/Program Files/MAME"
		"C:/Program Files (x86)/MAME"
		"/opt/mame"
	DOC "Path to MAME chdman tool for generating CHD image (optional)"
)

## Release information

set(
	RELEASE_INFO "${PROJECT_NAME} ${PROJECT_VERSION} - (C) 2022-2025 spicyjpeg"
	CACHE STRING "Executable description and version string, placed in the \
executable header (optional)"
)
set(
	RELEASE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}"
	CACHE STRING "CD-ROM image and release package file name"
)

string(TOUPPER "${RELEASE_NAME}" _cdVolumeName)
string(REGEX REPLACE "[^0-9A-Z_]" "_" _cdVolumeName "${_cdVolumeName}")

set(
	CD_VOLUME_NAME "${_cdVolumeName}"
	CACHE STRING "CD-ROM image volume label"
)

## Compile-time options

set(
	ENABLE_LOG_BUFFER ON
	CACHE BOOL "Buffer the last few log messages in memory and enable the \
on-screen log window"
)
set(
	ENABLE_APP_LOGGING ON
	CACHE BOOL "Log messages from application code"
)
set(
	ENABLE_CART_DATA_LOGGING ON
	CACHE BOOL "Log messages from security cartridge data parsers and builders"
)
set(
	ENABLE_IO_LOGGING ON
	CACHE BOOL "Log messages from System 573 and I/O board drivers"
)
set(
	ENABLE_CART_LOGGING ON
	CACHE BOOL "Log messages from security cartridge drivers"
)
set(
	ENABLE_NVRAM_LOGGING ON
	CACHE BOOL "Log messages from NVRAM (BIOS, RTC, flash) drivers"
)
set(
	ENABLE_BLKDEV_LOGGING ON
	CACHE BOOL "Log messages from IDE/ATAPI and other block device drivers"
)
set(
	ENABLE_FS_LOGGING ON
	CACHE BOOL "Log messages from filesystem drivers"
)

set(
	ENABLE_DUMMY_CART_DRIVER OFF
	CACHE BOOL "Enable support for simulating a dummy security cartridge (if \
data/dummy.dmp is present in the resource package)"
)

set(
	ENABLE_ARGV_PARSER OFF
	CACHE BOOL "Pass any command-line arguments given to the boot stub (using \
\$a0 and \$a1 as argc/argv) to the main executable"
)
set(
	ENABLE_AUTOBOOT ON
	CACHE BOOL "Enable support for automatic launching of executables on IDE \
and flash devices (if enabled via DIP switches)"
)
set(
	ENABLE_PCDRV OFF
	CACHE BOOL "Enable support for browsing the debugger or emulator's host \
filesystem through the PCDRV API"
)
set(
	ENABLE_PS1_CONTROLLER ON
	CACHE BOOL "Enable support for PS1/PS2 controllers wired to the SIO0 header"
)

set(
	mainOptions
	$<$<BOOL:${ENABLE_LOG_BUFFER}       >:ENABLE_LOG_BUFFER=1>
	$<$<BOOL:${ENABLE_APP_LOGGING}      >:ENABLE_APP_LOGGING=1>
	$<$<BOOL:${ENABLE_CART_DATA_LOGGING}>:ENABLE_CART_DATA_LOGGING=1>
	$<$<BOOL:${ENABLE_IO_LOGGING}       >:ENABLE_IO_LOGGING=1>
	$<$<BOOL:${ENABLE_CART_LOGGING}     >:ENABLE_CART_LOGGING=1>
	$<$<BOOL:${ENABLE_NVRAM_LOGGING}    >:ENABLE_NVRAM_LOGGING=1>
	$<$<BOOL:${ENABLE_BLKDEV_LOGGING}   >:ENABLE_BLKDEV_LOGGING=1>
	$<$<BOOL:${ENABLE_FS_LOGGING}       >:ENABLE_FS_LOGGING=1>
	$<$<BOOL:${ENABLE_DUMMY_CART_DRIVER}>:ENABLE_DUMMY_CART_DRIVER=1>
	$<$<BOOL:${ENABLE_AUTOBOOT}         >:ENABLE_AUTOBOOT=1>
	$<$<BOOL:${ENABLE_PCDRV}            >:ENABLE_PCDRV=1>
	$<$<BOOL:${ENABLE_PS1_CONTROLLER}   >:ENABLE_PS1_CONTROLLER=1>
)
set(
	subExecutableOptions
	$<$<BOOL:${ENABLE_ARGV_PARSER}>:ENABLE_ARGV_PARSER=1>
)
