
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	cart_tool_private
	LANGUAGES    C CXX ASM
	VERSION      0.3.5
	DESCRIPTION  "Konami System 573 security cartridge tool"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(
	XORRISO_PATH xorriso
	PATHS "C:/msys64/usr/bin"
	DOC   "Path to xorriso tool (optional)"
)
find_program(
	CHDMAN_PATH chdman
	PATHS "C:/Program Files/MAME" "C:/Program Files (x86)/MAME"
	DOC   "Path to MAME chdman tool (optional)"
)

## Files common to all executables

add_library(
	common OBJECT
	src/libc/crt0.c
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

function(addExecutable name stackTop)
	add_executable(${name} ${ARGN})
	target_link_libraries(${name} PRIVATE common)

	add_custom_command(
		TARGET     ${name} POST_BUILD
		BYPRODUCTS ${name}.psexe
		COMMAND
			"${Python3_EXECUTABLE}"
			"${PROJECT_SOURCE_DIR}/tools/convertExecutable.py"
			-r "cart_tool build ${PROJECT_VERSION} - (C) 2022-2024 spicyjpeg"
			-s 0x${stackTop} "$<TARGET_FILE:${name}>" ${name}.psexe
		VERBATIM
	)
endfunction()

## Main executable

addExecutable(
	main 801dfff0
	src/common/file.cpp
	src/common/gpu.cpp
	src/common/gpufont.cpp
	src/common/ide.cpp
	src/common/ideglue.cpp
	src/common/io.cpp
	src/common/pad.cpp
	src/common/rom.cpp
	src/common/spu.cpp
	src/common/util.cpp
	src/main/cart.cpp
	src/main/cartdata.cpp
	src/main/cartio.cpp
	src/main/main.cpp
	src/main/uibase.cpp
	src/main/uicommon.cpp
	src/main/zs01.cpp
	src/main/app/app.cpp
	src/main/app/cartactions.cpp
	src/main/app/cartunlock.cpp
	src/main/app/cartworkers.cpp
	src/main/app/main.cpp
	src/main/app/misc.cpp
	src/main/app/miscworkers.cpp
	src/main/app/modals.cpp
	src/vendor/ff.c
	src/vendor/ffunicode.c
	src/vendor/miniz.c
	src/vendor/printf.c
	src/vendor/qrcodegen.c
)
target_compile_definitions(
	main PRIVATE
	$<IF:$<CONFIG:Debug>,
		ENABLE_LOGGING=1
		ENABLE_FILE_WRITING=1
		#ENABLE_ARGV=1
		ENABLE_LOG_BUFFER=1
		ENABLE_PS1_CONTROLLER=1
		ENABLE_CART_MENU=1
		#ENABLE_X76F100_DRIVER=1
		ENABLE_DUMMY_DRIVER=1
		ENABLE_I2C_LOGGING=1
	,
		ENABLE_LOGGING=1
		ENABLE_FILE_WRITING=1
		#ENABLE_ARGV=1
		ENABLE_LOG_BUFFER=1
		ENABLE_PS1_CONTROLLER=1
		ENABLE_CART_MENU=1
		#ENABLE_X76F100_DRIVER=1
		#ENABLE_DUMMY_DRIVER=1
		#ENABLE_I2C_LOGGING=1
	>
)

## Executable launchers

# NOTE: in order to make sure -Os is passed after -Og or -O3 (see
# cmake/setup.cmake) and thus overrides it, it must be added to a separate
# target rather than directly to the executables.
add_library(launcherFlags INTERFACE)
target_compile_options(launcherFlags INTERFACE -Os)
target_compile_definitions(
	launcherFlags INTERFACE
	$<IF:$<CONFIG:Debug>,
		#ENABLE_LOGGING=1
	,
		#ENABLE_LOGGING=1
	>
)

function(addLauncher address stackTop)
	addExecutable(
		launcher${address} ${stackTop}
		src/common/ide.cpp
		src/common/ideglue.cpp
		src/common/io.cpp
		src/common/util.cpp
		src/launcher/main.cpp
		src/vendor/ff.c
		src/vendor/ffunicode.c
		src/vendor/printf.c
	)
	target_link_options(launcher${address} PRIVATE -Ttext=0x${address})
	target_link_libraries(launcher${address} PRIVATE launcherFlags)
endfunction()

# Note that the launchers must be <40 KB (0xa000 bytes) in order for this to
# work properly.
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
		resources.json resources.zip
	OUTPUT  resources.zip
	DEPENDS resources.json launcher801f8000 launcher803f8000
	COMMENT "Building resource archive"
	VERBATIM
)
addBinaryFile(
	main _resources _resourcesSize
	"${PROJECT_BINARY_DIR}/resources.zip"
)

## CD-ROM image

if(EXISTS "${XORRISO_PATH}")
	configure_file(assets/cdreadme.txt cdreadme.txt NEWLINE_STYLE CRLF)
	string(REPLACE "." "_" _version "${PROJECT_VERSION}")

	add_custom_command(
		COMMAND
			"${XORRISO_PATH}"
			-outdev cart_tool.iso -blank all
			-rockridge off -joliet off -padding 2M
			-volid "CART_TOOL_${_version}"
			-volset_id "CART_TOOL_${_version}"
			-publisher "SPICYJPEG"
			-application_id "PLAYSTATION"
			-system_id "PLAYSTATION"
			-preparer_id "CART_TOOL BUILD SCRIPT"
			-map cdreadme.txt README.TXT
			-map main.psexe PSX.EXE
			-clone PSX.EXE GSE.NXX
			-clone PSX.EXE NSE.GXX
			-clone PSX.EXE OSE.FXX
			-clone PSX.EXE QSU.DXH
			-clone PSX.EXE QSX.DXE
			-clone PSX.EXE QSY.DXD
			-clone PSX.EXE QSZ.DXC
			-clone PSX.EXE RSU.CXH
			-clone PSX.EXE RSV.CXG
			-clone PSX.EXE RSW.CXF
			-clone PSX.EXE RSZ.CXC
			-clone PSX.EXE SSW.BXF
			-clone PSX.EXE SSX.BXE
			-clone PSX.EXE SSY.BXD
			-clone PSX.EXE TSV.AXG
			-clone PSX.EXE TSW.AXF
			-clone PSX.EXE TSX.AXE
			-clone PSX.EXE TSY.AXD
			-clone PSX.EXE TSZ.AXC
		OUTPUT  cart_tool.iso
		DEPENDS main
		COMMENT "Building CD-ROM image"
		VERBATIM
	)

	if(EXISTS "${CHDMAN_PATH}")
		add_custom_command(
			COMMAND
				"${CHDMAN_PATH}" createcd -f -i cart_tool.iso -o cart_tool.chd
			OUTPUT  cart_tool.chd
			DEPENDS cart_tool.iso
			COMMENT "Building MAME CHD image"
			VERBATIM
		)

		add_custom_target(cd ALL DEPENDS cart_tool.iso cart_tool.chd)
	else()
		add_custom_target(cd ALL DEPENDS cart_tool.iso)
	endif()
endif()
