
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	cart_tool_private
	LANGUAGES    C CXX ASM
	VERSION      0.3.2
	DESCRIPTION  "Konami System 573 security cartridge tool"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(
	CHDMAN_PATH chdman
	DOC "Path to MAME chdman tool (optional)"
)

## Main executable

add_executable(
	cart_tool
	src/cart.cpp
	src/cartdata.cpp
	src/cartio.cpp
	src/file.cpp
	src/gpu.cpp
	src/ide.cpp
	src/io.cpp
	src/main.cpp
	src/pad.cpp
	src/spu.cpp
	src/uibase.cpp
	src/uicommon.cpp
	src/util.cpp
	src/zs01.cpp
	src/app/app.cpp
	src/app/cartactions.cpp
	src/app/cartunlock.cpp
	src/app/misc.cpp
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
	src/vendor/ff.c
	src/vendor/ffunicode.c
	src/vendor/miniz.c
	src/vendor/printf.c
	src/vendor/qrcodegen.c
)
target_include_directories(
	cart_tool PRIVATE
	src
	src/libc
)
target_compile_definitions(
	cart_tool PRIVATE
	VERSION="${PROJECT_VERSION}"
	$<IF:$<CONFIG:Debug>,
		ENABLE_DUMMY_DRIVER=1
		ENABLE_I2C_LOGGING=1
		ENABLE_PS1_CONTROLLER=1
	,
		#ENABLE_ARGV=1
		ENABLE_PS1_CONTROLLER=1
	>
)

add_custom_command(
	TARGET cart_tool POST_BUILD
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/convertExecutable.py"
		-r "cart_tool build ${PROJECT_VERSION} - (C) 2022-2023 spicyjpeg"
		-s 0x801dfff0
		$<TARGET_FILE:cart_tool> cart_tool.psexe
	BYPRODUCTS cart_tool.psexe
	COMMENT    "Converting executable"
	VERBATIM
)

## Default resource archive

add_custom_command(
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/buildResourceArchive.py"
		"${PROJECT_SOURCE_DIR}/resources.json" resources.zip
	OUTPUT  resources.zip
	DEPENDS resources.json
	COMMENT "Building resource archive"
	VERBATIM
)
ps1_target_incbin(
	cart_tool PRIVATE
	.rodata _resources _resourcesSize
	"${PROJECT_BINARY_DIR}/resources.zip"
)

## CD-ROM image

#configure_file(assets/cdreadme.txt cdreadme.txt NEWLINE_STYLE CRLF)
#add_custom_command(
#	COMMAND
#		"${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/buildCDImage.py"
#		-s "${PROJECT_BINARY_DIR}"
#		"${PROJECT_SOURCE_DIR}/cd.json" cart_tool.iso
#	OUTPUT  cart_tool.iso
#	DEPENDS cd.json cart_tool
#	COMMENT "Building CD-ROM image"
#	VERBATIM
#)

#if(EXISTS "${CHDMAN_PATH}")
#	add_custom_command(
#		COMMAND
#			"${CHDMAN_PATH}" createcd -f -i cart_tool.iso -o cart_tool.chd
#		OUTPUT  cart_tool.chd
#		DEPENDS cart_tool.iso
#		COMMENT "Building MAME CHD image"
#		VERBATIM
#	)
#
#	# Add a dummy target that depends on the CD image to ensure it gets built.
#	add_custom_target(cd ALL DEPENDS cart_tool.iso cart_tool.chd)
#else()
#	add_custom_target(cd ALL DEPENDS cart_tool.iso)
#endif()
