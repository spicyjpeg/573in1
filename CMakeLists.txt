
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	cart_tool
	LANGUAGES    C CXX ASM
	VERSION      0.4.2
	DESCRIPTION  "Konami System 573 security cartridge tool"
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

string(TOUPPER "${PROJECT_NAME}" _name)
string(REPLACE "." "_" _version "${PROJECT_VERSION}")

set(
	RELEASE_INFO "${PROJECT_NAME} ${PROJECT_VERSION} - (C) 2022-2024 spicyjpeg"
	CACHE STRING "Executable description and version string (optional)"
)
set(
	RELEASE_NAME "${PROJECT_NAME}_${PROJECT_VERSION}"
	CACHE STRING "CD-ROM image and release package file name"
)
set(
	CD_VOLUME_NAME "${_name}_${_version}"
	CACHE STRING   "CD-ROM image volume label"
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
target_compile_definitions(common PUBLIC VERSION="${PROJECT_VERSION}")
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
# heap and stack allocations as well as the resource archive) is <448 KB
# (0x70000 bytes).
addExecutable(
	main 80080000 801dfff0
	src/common/args.cpp
	src/common/file.cpp
	src/common/file9660.cpp
	src/common/filefat.cpp
	src/common/filemisc.cpp
	src/common/filezip.cpp
	src/common/gpu.cpp
	src/common/gpufont.cpp
	src/common/ide.cpp
	src/common/io.cpp
	src/common/pad.cpp
	src/common/rom.cpp
	src/common/spu.cpp
	src/common/util.cpp
	src/libc/crt0.c
	src/main/cart.cpp
	src/main/cartdata.cpp
	src/main/cartio.cpp
	src/main/main.cpp
	src/main/uibase.cpp
	src/main/uicommon.cpp
	src/main/uimodals.cpp
	src/main/zs01.cpp
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
	src/vendor/ff.c
	src/vendor/ffunicode.c
	src/vendor/miniz.c
	src/vendor/printf.c
	src/vendor/qrcodegen.c
)
target_compile_definitions(
	main PRIVATE
	$<IF:$<CONFIG:Debug>,
		ENABLE_DUMMY_DRIVER=1
		ENABLE_FULL_IDE_DRIVER=1
		ENABLE_I2C_LOGGING=1
		ENABLE_LOG_BUFFER=1
		ENABLE_LOGGING=1
		ENABLE_PS1_CONTROLLER=1
		#ENABLE_X76F100_DRIVER=1
	,
		#ENABLE_DUMMY_DRIVER=1
		ENABLE_FULL_IDE_DRIVER=1
		#ENABLE_I2C_LOGGING=1
		ENABLE_LOG_BUFFER=1
		ENABLE_LOGGING=1
		ENABLE_PS1_CONTROLLER=1
		#ENABLE_X76F100_DRIVER=1
	>
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

addExecutable(
	boot 80010000 0
	src/boot/crt0.s
	src/boot/main.cpp
	src/common/util.cpp
)
target_link_libraries(boot PRIVATE bootFlags)

function(addLauncher address stackTop)
	addExecutable(
		launcher${address} ${address} ${stackTop}
		src/common/args.cpp
		src/common/file.cpp
		src/common/filefat.cpp
		src/common/ide.cpp
		src/common/io.cpp
		src/common/util.cpp
		src/launcher/launcher.cpp
		src/launcher/main.cpp
		src/libc/crt0.c
		src/vendor/ff.c
		src/vendor/ffunicode.c
		src/vendor/printf.c
	)
	target_link_libraries(launcher${address} PRIVATE bootFlags)
endfunction()

# IMPORTANT: these addresses assume the launcher's total size (including code,
# heap and stack allocations, but excluding the executable header) is <32 KB
# (0x8000 bytes).
addLauncher(801f8000 801ffff0)
addLauncher(803f8000 803ffff0)

## Default resource archive

function(addBinaryFile target name sizeName path)
	set(_file "${PROJECT_BINARY_DIR}/includes/${target}_${name}.s")
	cmake_path(ABSOLUTE_PATH path OUTPUT_VARIABLE _path)

	file(
		CONFIGURE
		OUTPUT  "${_file}"
		CONTENT [[
.section .rodata.${name}, "a"
.balign 8

.global ${name}
.type ${name}, @object
.size ${name}, (${name}_end - ${name})

${name}:
	.incbin "${_path}"
${name}_end:

.section .rodata.${sizeName}, "a"
.balign 4

.global ${sizeName}
.type ${sizeName}, @object
.size ${sizeName}, 4

${sizeName}:
	.int (${name}_end - ${name})
]]
		ESCAPE_QUOTES
		NEWLINE_STYLE LF
	)

	target_sources(${target} PRIVATE "${_file}")
	set_source_files_properties("${_file}" PROPERTIES OBJECT_DEPENDS "${_path}")
endfunction()

configure_file(resources.json resources.json ESCAPE_QUOTES)

add_custom_command(
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/buildResourceArchive.py"
		resources.json
		resources.zip
	OUTPUT  resources.zip
	DEPENDS
		resources.json
		assets/app.palette.json
		assets/app.strings.json
		main.psexe
		launcher801f8000.psexe
		launcher803f8000.psexe
	COMMENT "Building resource archive"
	VERBATIM
)
addBinaryFile(
	boot _resourceArchive _resourceArchiveLength
	"${PROJECT_BINARY_DIR}/resources.zip"
)

## CD-ROM image

configure_file(cd.json             cd.json    ESCAPE_QUOTES)
configure_file(assets/cdreadme.txt readme.txt NEWLINE_STYLE CRLF)

add_custom_command(
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/buildCDImage.py"
		cd.json
		"${RELEASE_NAME}.iso"
	OUTPUT  "${RELEASE_NAME}.iso"
	DEPENDS
		cd.json
		boot.psexe
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

	list(APPEND packageFiles "${RELEASE_NAME}.chd")
endif()

## Release package

list(
	APPEND packageFiles
	readme.txt
	boot.psexe
	"${RELEASE_NAME}.iso"
)

add_custom_command(
	COMMAND
		"${CMAKE_COMMAND}" -E tar cf
		"${RELEASE_NAME}.zip"
		--format=zip
		${packageFiles}
	OUTPUT  "${RELEASE_NAME}.zip"
	DEPENDS ${packageFiles}
	COMMENT "Packaging built files"
	VERBATIM
)

add_custom_target(package ALL DEPENDS "${RELEASE_NAME}.zip")
