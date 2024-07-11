
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	573in1
	LANGUAGES    C CXX ASM
	VERSION      1.0.0
	DESCRIPTION  "Konami System 573 maintenance tool"
	HOMEPAGE_URL "https://github.com/spicyjpeg/573in1"
)

set(
	RELEASE_INFO "${PROJECT_NAME} ${PROJECT_VERSION} - (C) 2022-2024 spicyjpeg"
	CACHE STRING "Executable description and version string (optional)"
)
set(
	RELEASE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}"
	CACHE STRING "CD-ROM image and release package file name"
)

string(TOUPPER "${RELEASE_NAME}" _cdVolumeName)
string(REGEX REPLACE "[^0-9A-Z_]" "_" _cdVolumeName "${_cdVolumeName}")
set(
	CD_VOLUME_NAME "${_cdVolumeName}"
	CACHE STRING   "CD-ROM image volume label"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(
	CHDMAN_PATH chdman
	PATHS
		"C:/Program Files/MAME"
		"C:/Program Files (x86)/MAME"
		"/opt/mame"
	DOC "Path to MAME chdman tool (optional)"
)

## Files common to all executables

add_library(
	common OBJECT
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
target_include_directories(
	common PUBLIC
	src
	src/libc
)
target_compile_options(
	common PUBLIC
	-Wall
	-Wextra
	-Wno-unused-parameter
	$<$<COMPILE_LANGUAGE:CXX>:
		-Wno-pmf-conversions
	>
)
target_compile_definitions(
	common PUBLIC
	VERSION="${PROJECT_VERSION}"
	EXTERNAL_DATA_DIR="hdd:/${PROJECT_NAME}"
)
link_libraries(common)

function(addExecutable name address stackTop)
	add_executable(${name} ${ARGN})
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

## Main executable

# IMPORTANT: these addresses assume the boot executable's size (including code,
# heap and stack allocations as well as the resource archive) is <576 KB
# (0x90000 bytes).
addExecutable(
	main 800a0000 801dfff0
	src/common/file/fat.cpp
	src/common/file/file.cpp
	src/common/file/iso9660.cpp
	src/common/file/misc.cpp
	src/common/file/zip.cpp
	src/common/args.cpp
	src/common/gpu.cpp
	src/common/gpufont.cpp
	src/common/ide.cpp
	src/common/io.cpp
	src/common/pad.cpp
	src/common/rom.cpp
	src/common/romdrivers.cpp
	src/common/spu.cpp
	src/common/util.cpp
	src/libc/crt0.c
	src/main/app/app.cpp
	src/main/app/cartactions.cpp
	src/main/app/cartunlock.cpp
	src/main/app/cartworkers.cpp
	src/main/app/main.cpp
	src/main/app/misc.cpp
	src/main/app/miscworkers.cpp
	src/main/app/modals.cpp
	src/main/app/romactions.cpp
	src/main/app/romworkers.cpp
	src/main/app/tests.cpp
	src/main/cart/cart.cpp
	src/main/cart/cartdata.cpp
	src/main/cart/cartio.cpp
	src/main/cart/zs01.cpp
	src/main/main.cpp
	src/main/uibase.cpp
	src/main/uicommon.cpp
	src/main/uimodals.cpp
	src/vendor/ff.c
	src/vendor/ffunicode.c
	src/vendor/miniz.c
	src/vendor/printf.c
	src/vendor/qrcodegen.c
)
target_compile_definitions(
	main PRIVATE
	# Logging options
	ENABLE_APP_LOGGING=1
	ENABLE_CART_IO_LOGGING=1
	ENABLE_CART_DATA_LOGGING=1
	ENABLE_ROM_LOGGING=1
	ENABLE_IDE_LOGGING=1
	ENABLE_FS_LOGGING=1
	# Security cartridge driver options
	#ENABLE_DUMMY_CART_DRIVER=1
	ENABLE_X76F041_CART_DRIVER=1
	#ENABLE_X76F100_CART_DRIVER=1
	ENABLE_ZS01_CART_DRIVER=1
	# Misc. options
	ENABLE_FULL_IDE_DRIVER=1
	ENABLE_LOG_BUFFER=1
	#ENABLE_ARGV=1
	#ENABLE_PCDRV=1
	ENABLE_PS1_CONTROLLER=1
	ENABLE_AUTOBOOT=1
)

## Boot stub and executable launchers

# NOTE: in order to make sure -Os is passed after -Og or -O3 (see
# cmake/setup.cmake) and thus overrides it, it must be added to a separate
# target rather than directly to the executables.
add_library(bootFlags INTERFACE)
target_compile_options(bootFlags INTERFACE -Os)
target_compile_definitions(
	bootFlags INTERFACE
	$<$<CONFIG:Debug>:
		#ENABLE_ARGV=1
		#ENABLE_LOGGING=1
	>
)

function(addLauncher address stackTop)
	addExecutable(
		launcher${address} ${address} ${stackTop}
		src/common/args.cpp
		src/common/ide.cpp
		src/common/io.cpp
		src/common/util.cpp
		src/launcher/main.cpp
		src/libc/crt0.c
		src/vendor/printf.c
	)
	target_link_libraries(launcher${address} PRIVATE bootFlags)
endfunction()

# IMPORTANT: these addresses assume the launcher's total size (including code,
# heap and stack allocations, but excluding the executable header) is <12 KB
# (0x3000 bytes).
addLauncher(801fd000 801ffff0)
addLauncher(803fd000 803ffff0)

## Boot stub and resource archive

configure_file(assets/about.txt about.txt NEWLINE_STYLE LF)

function(addBootStub name resourceName)
	configure_file(${resourceName}.json ${resourceName}.json ESCAPE_QUOTES)

	add_custom_command(
		COMMAND
			"${Python3_EXECUTABLE}"
			"${PROJECT_SOURCE_DIR}/tools/buildResourceArchive.py"
			${resourceName}.json
			${resourceName}.zip
		OUTPUT  ${resourceName}.zip
		DEPENDS
			${resourceName}.json
			assets/app.palette.json
			assets/app.strings.json
			main.psexe
			launcher801fd000.psexe
			launcher803fd000.psexe
		COMMENT "Building ${name} resource archive"
		VERBATIM
	)

	addExecutable(
		${name} 80010000 0
		src/boot/crt0.s
		src/boot/main.cpp
		src/common/io.cpp
		src/common/util.cpp
	)
	addBinaryFile(
		${name} _resourceArchive _resourceArchiveLength
		"${PROJECT_BINARY_DIR}/${resourceName}.zip"
	)
	target_link_libraries(${name} PRIVATE bootFlags)
endfunction()

addBootStub("${RELEASE_NAME}"      resources)
addBootStub("${RELEASE_NAME}-tiny" resourcestiny)

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
