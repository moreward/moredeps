# Toolchain for Emscripten (wasm32) builds.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.
# It locates emcc and then delegates to the upstream Emscripten CMake module.

set(MOREDEPS_PLATFORM "wasm_emscripten" CACHE STRING "moredeps target platform")

# Locate the Emscripten compiler.
find_program(EMCC_PROGRAM emcc REQUIRED)
get_filename_component(EMSCRIPTEN_ROOT "${EMCC_PROGRAM}" DIRECTORY ABSOLUTE)

set(EMSCRIPTEN_ROOT_PATH "${EMSCRIPTEN_ROOT}" CACHE PATH "Emscripten root directory" FORCE)

# Include the upstream Emscripten toolchain file.
set(_emscripten_toolchain "${EMSCRIPTEN_ROOT_PATH}/cmake/Modules/Platform/Emscripten.cmake")
if(NOT EXISTS "${_emscripten_toolchain}")
  message(FATAL_ERROR "Could not find upstream Emscripten toolchain at ${_emscripten_toolchain}")
endif()
include("${_emscripten_toolchain}")
