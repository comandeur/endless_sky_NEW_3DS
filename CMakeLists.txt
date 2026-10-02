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
ctr_create_3dsx(endless-sky SMDH endless-sky.smdh)
