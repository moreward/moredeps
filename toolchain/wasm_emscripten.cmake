# Toolchain for Emscripten (wasm32) builds.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.
# It locates emcc and then delegates to the upstream Emscripten CMake module.

set(MOREDEPS_PLATFORM "wasm_emscripten" CACHE STRING "moredeps target platform")

# Locate the Emscripten compiler. On Homebrew the emcc in PATH is a wrapper
# in bin/; the real SDK root is the adjacent libexec/ directory, which also
# contains the tools/ subdir that Dawn's generator expects.
find_program(EMCC_PROGRAM emcc REQUIRED)
get_filename_component(_emcc_real_path "${EMCC_PROGRAM}" REALPATH)
get_filename_component(_emcc_bin_dir "${_emcc_real_path}" DIRECTORY)
get_filename_component(_emscripten_prefix "${_emcc_bin_dir}" DIRECTORY ABSOLUTE)

# Try the standard SDK root layout first, then fall back to the bin directory.
if(EXISTS "${_emscripten_prefix}/libexec/emcc")
  set(EMSCRIPTEN_ROOT_PATH "${_emscripten_prefix}/libexec" CACHE PATH "Emscripten root directory" FORCE)
elseif(EXISTS "${_emscripten_prefix}/emcc")
  set(EMSCRIPTEN_ROOT_PATH "${_emscripten_prefix}" CACHE PATH "Emscripten root directory" FORCE)
else()
  set(EMSCRIPTEN_ROOT_PATH "${_emcc_bin_dir}" CACHE PATH "Emscripten root directory" FORCE)
endif()

# The upstream Emscripten toolchain is usually installed next to the emcc
# binary as cmake/Modules/Platform/Emscripten.cmake, or in libexec/cmake/Modules.
set(_emscripten_toolchain "")
foreach(_dir
  "${EMSCRIPTEN_ROOT_PATH}/cmake/Modules/Platform/Emscripten.cmake"
  "${EMSCRIPTEN_ROOT_PATH}/../libexec/cmake/Modules/Platform/Emscripten.cmake"
)
  if(EXISTS "${_dir}")
    set(_emscripten_toolchain "${_dir}")
    break()
  endif()
endforeach()

if(NOT EXISTS "${_emscripten_toolchain}")
  message(FATAL_ERROR "Could not find upstream Emscripten toolchain. Looked for cmake/Modules/Platform/Emscripten.cmake relative to emcc.")
endif()
include("${_emscripten_toolchain}")

unset(_emcc_real_path)
unset(_emcc_bin_dir)
unset(_emscripten_prefix)
unset(_emscripten_toolchain)
