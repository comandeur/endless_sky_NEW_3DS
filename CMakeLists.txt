# Endless Sky for the Nintendo 3DS.
#
# The game itself comes from the upstream repository (the "upstream" submodule).
# tools/prepare-source.sh assembles the sources that are compiled here: the
# upstream sources, with the patches from port/patches applied, the files of
# port/overlay replacing their upstream counterparts, and the 3DS platform layer
# from port/src3ds (in the "ctr" subfolder).
#
# Configure with the devkitPro toolchain file, e.g.:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake
# or simply use tools/docker-build.sh.

cmake_minimum_required(VERSION 3.13)

project("Endless Sky 3DS" VERSION 0.11.3 LANGUAGES C CXX ASM)

if(NOT NINTENDO_3DS)
	message(FATAL_ERROR "Use the devkitPro 3DS toolchain: -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# Assemble the sources.
set(ES_SOURCE_DIR "${CMAKE_BINARY_DIR}/src")
execute_process(
	COMMAND bash "${CMAKE_SOURCE_DIR}/tools/prepare-source.sh" "${ES_SOURCE_DIR}"
	RESULT_VARIABLE PREPARE_RESULT)
if(NOT PREPARE_RESULT EQUAL 0)
	message(FATAL_ERROR "Could not prepare the sources (tools/prepare-source.sh).")
endif()
# Re-run the preparation whenever a port file or patch changes.
file(GLOB_RECURSE ES_PORT_FILES CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/port/overlay/*"
	"${CMAKE_SOURCE_DIR}/port/src3ds/*"
	"${CMAKE_SOURCE_DIR}/port/patches/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ES_PORT_FILES})

file(GLOB_RECURSE ES_SOURCES CONFIGURE_DEPENDS "${ES_SOURCE_DIR}/*.cpp")

# The vertex shader.
ctr_add_shader_library(es port/shaders/es.v.pica)
dkp_add_embedded_binary_library(es_shader es)

add_executable(endless-sky ${ES_SOURCES})
target_include_directories(endless-sky PRIVATE
	"${CMAKE_SOURCE_DIR}/port/compat"
	"${DEVKITPRO}/portlibs/3ds/include"
	"${DEVKITPRO}/portlibs/3ds/include/minizip")
target_compile_options(endless-sky PRIVATE
	-Wall -Wno-unused-parameter -Wno-psabi
	$<$<CONFIG:Release>:-O2>)
target_link_directories(endless-sky PRIVATE "${DEVKITPRO}/portlibs/3ds/lib")
target_link_libraries(endless-sky PRIVATE
	es_shader
	citro3d
	png16 jpeg
	mad
	FLAC++ FLAC ogg
	minizip z
	ctru m)

# Metadata shown by the Homebrew Launcher.
ctr_generate_smdh(endless-sky.smdh
	NAME "Endless Sky"
	DESCRIPTION "Space exploration, trading and combat"
	AUTHOR "Endless Sky contributors / 3DS port"
	ICON "${CMAKE_SOURCE_DIR}/port/meta/icon.png")
# Release builds can embed the converted game data (tools/convert-assets.sh)
# in the romfs, so that a single file is enough:
#   cmake -DES_ROMFS=assets-out/endless-sky ...
set(ES_ROMFS "" CACHE PATH "Converted game data to embed in the romfs (empty: data on the SD card)")
if(ES_ROMFS)
	get_filename_component(ES_ROMFS "${ES_ROMFS}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
	if(NOT EXISTS "${ES_ROMFS}/credits.txt" OR NOT EXISTS "${ES_ROMFS}/images.pak")
		message(FATAL_ERROR "ES_ROMFS: ${ES_ROMFS} does not contain converted game data.")
	endif()
	message(STATUS "Embedding the game data from ${ES_ROMFS}")
	ctr_create_3dsx(endless-sky SMDH endless-sky.smdh ROMFS "${ES_ROMFS}")
else()
	ctr_create_3dsx(endless-sky SMDH endless-sky.smdh)
endif()

# The CIA version (installed to the HOME Menu), which gets the New 3DS memory
# mode. It needs makerom and bannertool, which the Docker image provides.
find_program(MAKEROM makerom)
find_program(BANNERTOOL bannertool)
if(MAKEROM AND BANNERTOOL)
	add_custom_command(OUTPUT endless-sky.bnr
		COMMAND "${BANNERTOOL}" makebanner
			-i "${CMAKE_SOURCE_DIR}/port/meta/banner.png"
			-a "${CMAKE_SOURCE_DIR}/port/meta/banner.wav"
			-o endless-sky.bnr
		DEPENDS "${CMAKE_SOURCE_DIR}/port/meta/banner.png" "${CMAKE_SOURCE_DIR}/port/meta/banner.wav"
		VERBATIM)
	# The RSF, plus the romfs location for release builds.
	set(CIA_RSF "${CMAKE_CURRENT_BINARY_DIR}/endless-sky.rsf")
	file(READ "${CMAKE_SOURCE_DIR}/port/meta/endless-sky.rsf" RSF_TEXT)
	if(ES_ROMFS)
		string(APPEND RSF_TEXT "\nRomFs:\n  RootPath: \"${ES_ROMFS}\"\n")
	endif()
	file(WRITE "${CIA_RSF}" "${RSF_TEXT}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/port/meta/endless-sky.rsf")
	# Rebuild the CIA when the embedded data changes.
	set(CIA_DATA "")
	if(ES_ROMFS)
		set(CIA_DATA "${ES_ROMFS}/images.pak" "${ES_ROMFS}/images.idx" "${ES_ROMFS}/credits.txt")
	endif()
	add_custom_command(OUTPUT endless-sky.cia
		COMMAND "${MAKEROM}" -f cia -o endless-sky.cia -target t -exefslogo
			-elf "$<TARGET_FILE:endless-sky>"
			-rsf "${CIA_RSF}"
			-icon "${CMAKE_CURRENT_BINARY_DIR}/endless-sky.smdh"
			-banner endless-sky.bnr
		DEPENDS endless-sky endless-sky.bnr "${CMAKE_SOURCE_DIR}/port/meta/endless-sky.rsf" ${CIA_DATA}
			"${CMAKE_CURRENT_BINARY_DIR}/endless-sky.smdh"
		VERBATIM)
	add_custom_target(cia ALL DEPENDS endless-sky.cia)
else()
	message(STATUS "makerom or bannertool not found: the CIA will not be built.")
endif()
