
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
	common PUBLIC
	src
	src/libc
)
target_compile_definitions(
	common PUBLIC
	VERSION="${PROJECT_VERSION}"
	$<IF:$<CONFIG:Debug>,
		ENABLE_LOGGING=1
	,
		ENABLE_LOGGING=1
	>
)

## Main executable

add_executable(
	main
	src/main/cart.cpp
	src/main/cartdata.cpp
	src/main/cartio.cpp
	src/main/main.cpp
	src/main/uibase.cpp
	src/main/uicommon.cpp
	src/main/zs01.cpp
	src/main/app/app.cpp
	src/main/app/appworkers.cpp
	src/main/app/cartactions.cpp
	src/main/app/cartunlock.cpp
	src/main/app/main.cpp
	src/main/app/misc.cpp
)
target_compile_definitions(
	main PRIVATE
	VERSION="${PROJECT_VERSION}"
	$<IF:$<CONFIG:Debug>,
		#ENABLE_ARGV=1
		ENABLE_PS1_CONTROLLER=1
		ENABLE_CART_MENU=1
		#ENABLE_X76F100_DRIVER=1
		ENABLE_DUMMY_DRIVER=1
		ENABLE_I2C_LOGGING=1
	,
		#ENABLE_ARGV=1
		ENABLE_PS1_CONTROLLER=1
		ENABLE_CART_MENU=1
		#ENABLE_X76F100_DRIVER=1
		#ENABLE_DUMMY_DRIVER=1
		#ENABLE_I2C_LOGGING=1
	>
)
target_link_libraries(main PRIVATE common)

add_custom_command(
	TARGET main POST_BUILD
	COMMAND
		"${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tools/convertExecutable.py"
		-r "cart_tool build ${PROJECT_VERSION} - (C) 2022-2023 spicyjpeg"
		-s 0x801dfff0
		$<TARGET_FILE:main> main.psexe
	BYPRODUCTS main.psexe
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
	main PRIVATE
	.rodata _resources _resourcesSize
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
