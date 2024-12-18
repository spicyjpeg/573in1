# 573in1 - Copyright (C) 2022-2024 spicyjpeg
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

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	573in1
	LANGUAGES    C CXX ASM
	VERSION      1.0.3
	DESCRIPTION  "Konami System 573 maintenance tool"
	HOMEPAGE_URL "https://github.com/spicyjpeg/573in1"
)

include(cmake/options.cmake)
include(cmake/setup.cmake)
include(cmake/virtualenv.cmake)

## Source files

set(
	ps1Sources
	#src/libc/crt0.c
	src/libc/cxxsupport.cpp
	src/libc/malloc.c
	src/libc/memset.s
	src/libc/misc.c
	src/libc/misc.s
	src/libc/string.c
	src/ps1/pcdrv.s
	src/ps1/system.c
	src/ps1/system.s
	src/ps1/unhandledexc.c
)
set(
	vendorSources
	src/vendor/ff.c
	src/vendor/ffunicode.c
	src/vendor/miniz.c
	src/vendor/printf.c
	src/vendor/qrcodegen.c
)

set(
	commonSources
	src/common/args.cpp
	src/common/gpu.cpp
	src/common/gpufont.cpp
	src/common/io.cpp
	src/common/ioboard.cpp
	src/common/mdec.cpp
	src/common/mdec.s
	src/common/pad.cpp
	src/common/rom.cpp
	src/common/romdrivers.cpp
	src/common/spu.cpp
)
set(
	exthwSources
	src/common/exthw/lcd.cpp
)
set(
	fsSources
	src/common/fs/fat.cpp
	src/common/fs/file.cpp
	src/common/fs/iso9660.cpp
	src/common/fs/misc.cpp
	src/common/fs/package.cpp
	src/common/fs/zip.cpp
)
set(
	storageSources
	src/common/storage/ata.cpp
	src/common/storage/atapi.cpp
	src/common/storage/device.cpp
)
set(
	utilSources
	src/common/util/hash.cpp
	src/common/util/log.cpp
	src/common/util/misc.cpp
	src/common/util/string.cpp
	src/common/util/string.s
	src/common/util/tween.cpp
)

set(
	mainSources
	${ps1Sources}
	${vendorSources}
	${commonSources}
	${exthwSources}
	${fsSources}
	${storageSources}
	${utilSources}
	src/libc/crt0.c
	src/main/app/app.cpp
	src/main/app/cartactions.cpp
	src/main/app/cartunlock.cpp
	src/main/app/mainmenu.cpp
	src/main/app/misc.cpp
	src/main/app/modals.cpp
	src/main/app/romactions.cpp
	src/main/app/tests.cpp
	src/main/app/threads.cpp
	src/main/cart/cart.cpp
	src/main/cart/cartdata.cpp
	src/main/cart/cartio.cpp
	src/main/cart/zs01.cpp
	src/main/workers/cartworkers.cpp
	src/main/workers/miscworkers.cpp
	src/main/workers/romworkers.cpp
	src/main/formats.cpp
	src/main/main.cpp
	src/main/uibase.cpp
	src/main/uicommon.cpp
	src/main/uimodals.cpp
)
set(
	launcherSources
	${ps1Sources}
	${storageSources}
	${utilSources}
	src/common/args.cpp
	src/common/io.cpp
	src/launcher/exchandler.s
	src/launcher/main.cpp
	src/libc/crt0.c
)
set(
	bootStubSources
	${ps1Sources}
	${utilSources}
	src/boot/crt0.s
	src/boot/main.cpp
	src/common/io.cpp
)

## Flags and compile-time options

set(
	commonOptions
	-Wall
	-Wextra
	-Wno-unused-parameter
	$<$<COMPILE_LANGUAGE:CXX>:
		-Wno-pmf-conversions
		-Wno-delete-non-virtual-dtor
	>
)

add_library(mainFlags          INTERFACE)
add_library(subExecutableFlags INTERFACE)

target_include_directories(mainFlags          INTERFACE src src/libc)
target_include_directories(subExecutableFlags INTERFACE src src/libc)

# NOTE: in order to make sure -Os is passed after -Og or -O3 (see
# cmake/setup.cmake) and thus overrides it, it must be added to a separate
# target rather than directly to the executables.
target_compile_options(mainFlags          INTERFACE ${commonOptions})
target_compile_options(subExecutableFlags INTERFACE ${commonOptions} -Os)

target_compile_definitions(
	mainFlags INTERFACE
	${mainOptions}
	VERSION="${PROJECT_VERSION}"
	EXTERNAL_DATA_DIR="hdd:/${PROJECT_NAME}"
)
target_compile_definitions(
	subExecutableFlags INTERFACE
	${subExecutableOptions}
	NDEBUG=1
)

## Main executables

function(addPS1Executable name address stackTop)
	add_executable     (${name} ${ARGN})
	target_link_options(${name} PRIVATE -Ttext=0x${address})

	add_custom_command(
		TARGET     ${name} POST_BUILD
		BYPRODUCTS "${name}.psexe"
		COMMAND
			"${Python3_EXECUTABLE}"
			"${PROJECT_SOURCE_DIR}/tools/convertExecutable.py"
			-r "${RELEASE_INFO}"
			-s 0x${stackTop}
			"$<TARGET_FILE:${name}>"
			"${name}.psexe"
		VERBATIM
	)
endfunction()

# IMPORTANT: these addresses assume the boot executable's size (including code,
# heap and stack allocations as well as the resource package, but excluding the
# executable header) is 576 KB (0x90000 bytes) or less, and that each launcher's
# size is 12 KB (0x3000 bytes) or less.
addPS1Executable(main             800a0000 801dfff0 ${mainSources})
addPS1Executable(launcher801fd000 801fd000 801ffff0)
addPS1Executable(launcher803fd000 803fd000 803ffff0)

add_library(launcher OBJECT ${launcherSources})
add_library(bootStub OBJECT ${bootStubSources})

target_link_libraries(main     PUBLIC mainFlags)
target_link_libraries(launcher PUBLIC subExecutableFlags)
target_link_libraries(bootStub PUBLIC subExecutableFlags)

target_link_libraries(launcher801fd000 PUBLIC launcher)
target_link_libraries(launcher803fd000 PUBLIC launcher)

## Boot stubs and resource packages

file(GLOB_RECURSE assetList RELATIVE "${PROJECT_SOURCE_DIR}" assets/*)

configure_file(assets/about.txt about.txt NEWLINE_STYLE LF)

function(addBuildVariant name resourceName)
	configure_file(${resourceName}.json ${resourceName}.json ESCAPE_QUOTES)

	add_custom_command(
		COMMAND
			"${Python3_EXECUTABLE}"
			"${PROJECT_SOURCE_DIR}/tools/buildResourcePackage.py"
			-a 64
			${resourceName}.json
			${resourceName}.pkg
		OUTPUT  ${resourceName}.pkg
		DEPENDS
			${resourceName}.json
			${assetList}
			main.psexe
			launcher801fd000.psexe
			launcher803fd000.psexe
		COMMENT "Building ${name} resource package"
		VERBATIM
	)

	addPS1Executable(${name} 80010000 0)
	addBinaryFileWithSize(
		${name} _resourcePackage _resourcePackageLength
		"${PROJECT_BINARY_DIR}/${resourceName}.pkg"
	)

	target_link_libraries(${name} PUBLIC bootStub)
endfunction()

addBuildVariant("${RELEASE_NAME}"      resources)
addBuildVariant("${RELEASE_NAME}-tiny" resourcestiny)

## CD-ROM image

configure_file(cdrom.json          cdrom.json  ESCAPE_QUOTES)
configure_file(assets/cdreadme.txt readme.txt  NEWLINE_STYLE CRLF)
configure_file(LICENSE             license.txt NEWLINE_STYLE CRLF)

add_custom_command(
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/buildCDImage.py"
		cdrom.json
		"${RELEASE_NAME}.iso"
	OUTPUT  "${RELEASE_NAME}.iso"
	DEPENDS
		cdrom.json
		"${RELEASE_NAME}"
	COMMENT "Building CD-ROM image"
	VERBATIM
)

if(EXISTS "${CHDMAN_PATH}")
	add_custom_command(
		COMMAND
			"${CHDMAN_PATH}" createcd -f
			-i "${RELEASE_NAME}.iso"
			-o "${RELEASE_NAME}.chd"
		OUTPUT  "${RELEASE_NAME}.chd"
		DEPENDS "${RELEASE_NAME}.iso"
		COMMENT "Building MAME CHD image"
		VERBATIM
	)

	add_custom_target(cdrom ALL DEPENDS "${RELEASE_NAME}.chd")
else()
	add_custom_target(cdrom ALL DEPENDS "${RELEASE_NAME}.iso")
endif()
