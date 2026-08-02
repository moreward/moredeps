cmake_minimum_required(VERSION 3.25)
project(ghostty LANGUAGES C)

find_program(ZIG_EXECUTABLE zig REQUIRED)
message(STATUS "Found zig: ${ZIG_EXECUTABLE}")

set(GHOSTTY_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ghostty")
set(GHOSTTY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty_src")
set(GHOSTTY_ZIG_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-cache")
set(GHOSTTY_ZIG_GLOBAL_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache")

# Copy the upstream source tree into the build directory so the submodule stays
# clean. We exclude top-level "macos" because that directory contains the
# macOS/Swift app UI assets and is not needed for the cross-platform
# libghostty-vt library (the pkg/macos Zig package is still copied). We also
# exclude any git metadata or build artifacts from in-tree builds.
file(REMOVE_RECURSE ${GHOSTTY_BUILD_DIR})
file(MAKE_DIRECTORY ${GHOSTTY_BUILD_DIR})

file(GLOB _ghostty_top
  RELATIVE ${GHOSTTY_SRC_DIR}
  ${GHOSTTY_SRC_DIR}/*
)
foreach(_item IN LISTS _ghostty_top)
  set(_src_path ${GHOSTTY_SRC_DIR}/${_item})
  if(IS_DIRECTORY ${_src_path})
    if(_item STREQUAL ".git"
        OR _item STREQUAL "macos"
        OR _item STREQUAL ".zig-cache"
        OR _item STREQUAL "zig-out")
      continue()
    endif()
    file(COPY ${_src_path} DESTINATION ${GHOSTTY_BUILD_DIR})
  else()
    file(COPY ${_src_path} DESTINATION ${GHOSTTY_BUILD_DIR})
  endif()
endforeach()

# Map the moredeps CMake build type to a Zig optimization mode.
if(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
  if(_bt STREQUAL "DEBUG")
    set(GHOSTTY_ZIG_OPTIMIZE "-Doptimize=Debug")
  elseif(_bt STREQUAL "MINSIZEREL")
    set(GHOSTTY_ZIG_OPTIMIZE "-Doptimize=ReleaseSmall")
  elseif(_bt STREQUAL "RELWITHDEBINFO")
    set(GHOSTTY_ZIG_OPTIMIZE "-Doptimize=ReleaseFast")
  else()
    set(GHOSTTY_ZIG_OPTIMIZE "-Doptimize=ReleaseFast")
  endif()
else()
  set(GHOSTTY_ZIG_OPTIMIZE "-Doptimize=ReleaseFast")
endif()

set(GHOSTTY_ZIG_ARGS
  build
  -Demit-lib-vt
  -Demit-xcframework=false
  -Demit-macos-app=false
  ${GHOSTTY_ZIG_OPTIMIZE}
  --cache-dir ${GHOSTTY_ZIG_CACHE}
  --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
)

# The cross-platform vt library artifacts produced by zig build.
if(WIN32)
  set(GHOSTTY_STATIC_LIB "${GHOSTTY_BUILD_DIR}/zig-out/lib/ghostty-vt-static.lib")
else()
  set(GHOSTTY_STATIC_LIB "${GHOSTTY_BUILD_DIR}/zig-out/lib/libghostty-vt.a")
endif()
set(GHOSTTY_INCLUDE_DIR "${GHOSTTY_BUILD_DIR}/zig-out/include/ghostty")

add_custom_command(
  OUTPUT ${GHOSTTY_STATIC_LIB}
  COMMAND ${ZIG_EXECUTABLE} ${GHOSTTY_ZIG_ARGS}
  WORKING_DIRECTORY ${GHOSTTY_BUILD_DIR}
  COMMENT "Building libghostty-vt"
  USES_TERMINAL
)

add_custom_target(ghostty_build ALL DEPENDS ${GHOSTTY_STATIC_LIB})

install(FILES ${GHOSTTY_STATIC_LIB} DESTINATION lib)
install(DIRECTORY ${GHOSTTY_INCLUDE_DIR} DESTINATION include)
