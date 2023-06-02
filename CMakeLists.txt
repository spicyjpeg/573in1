
cmake_minimum_required(VERSION 3.25)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake")

project(
	cart_tool_private
	LANGUAGES    C CXX ASM
	VERSION      0.3.0
	DESCRIPTION  "Konami System 573 security cartridge tool"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

## Main executable

add_executable(
	cart_tool
	src/asset.cpp
	src/cartdata.cpp
	src/cartio.cpp
	src/gpu.cpp
	src/io.cpp
	src/main.cpp
	src/pad.cpp
	src/spu.cpp
	src/uibase.cpp
	src/uicommon.cpp
	src/util.cpp
	src/zs01.cpp
	src/app/actions.cpp
	src/app/app.cpp
	src/app/misc.cpp
	src/app/unlock.cpp
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
	#ENABLE_ARGV=1
	ENABLE_PS1_CONTROLLER=1
	MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS=1
	MINIZ_NO_ARCHIVE_WRITING_APIS=1
	MINIZ_NO_STDIO=1
	MINIZ_NO_TIME=1
	PRINTF_DISABLE_SUPPORT_FLOAT=1
)

add_custom_command(
	TARGET cart_tool POST_BUILD
	COMMAND
		"${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/convertExecutable.py"
		$<TARGET_FILE:cart_tool> cart_tool.psexe
	BYPRODUCTS cart_tool.psexe
	COMMENT    "Converting executable"
)

## Default resource archive

add_custom_command(
	COMMAND
		"${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/buildResourceArchive.py"
		"${PROJECT_SOURCE_DIR}/resources.json" resources.zip
	OUTPUT  resources.zip
	DEPENDS resources.json
	COMMENT "Building resource archive"
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
#		"${PROJECT_SOURCE_DIR}/cd.json" cart_tool.iso
#	OUTPUT  cart_tool.iso
#	DEPENDS cd.json cart_tool
#	COMMENT "Building CD-ROM image"
#)

# Add a dummy target that depends on the CD image to make sure it gets built.
#add_custom_target(cd ALL DEPENDS cart_tool.iso)
