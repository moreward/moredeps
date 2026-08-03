cmake_minimum_required(VERSION 3.25)
project(ghostty LANGUAGES C)

find_program(ZIG_EXECUTABLE zig REQUIRED)
message(STATUS "Found zig: ${ZIG_EXECUTABLE}")

set(GHOSTTY_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ghostty")
set(GHOSTTY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty_src")
set(GHOSTTY_ZIG_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-cache")
set(GHOSTTY_ZIG_GLOBAL_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache")

# Detect the target platform so we know where to find the artifacts.
# Apple platforms (macOS/iOS) are built via the xcframework path.
# Linux and Windows install the library directly into zig-out/.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(GHOSTTY_IS_APPLE ON)
  set(GHOSTTY_APPLE_SLICE "macos-arm64_x86_64")
  set(GHOSTTY_APPLE_LIB_NAME "ghostty-internal.a")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(GHOSTTY_IS_APPLE ON)
  if(CMAKE_OSX_SYSROOT MATCHES "Simulator")
    set(GHOSTTY_APPLE_SLICE "ios-arm64-simulator")
  else()
    set(GHOSTTY_APPLE_SLICE "ios-arm64")
  endif()
  set(GHOSTTY_APPLE_LIB_NAME "libghostty-internal.a")
else()
  set(GHOSTTY_IS_APPLE OFF)
endif()

# Copy the upstream source tree into the build directory so the submodule stays
# clean. For non-Apple platforms we can skip the top-level macos/ directory,
# which contains the macOS/Swift app UI assets and is not needed for the
# cross-platform library build. We also exclude any git metadata or build
# artifacts from in-tree builds.
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
        OR _item STREQUAL ".zig-cache"
        OR _item STREQUAL "zig-out")
      continue()
    endif()
    # The top-level macos/ directory is only needed for the xcframework build.
    if(NOT GHOSTTY_IS_APPLE AND _item STREQUAL "macos")
      continue()
    endif()
    file(COPY ${_src_path} DESTINATION ${GHOSTTY_BUILD_DIR})
  else()
    file(COPY ${_src_path} DESTINATION ${GHOSTTY_BUILD_DIR})
  endif()
endforeach()

# Patch upstream build.zig to build only the static embedding library on
# non-Darwin platforms. The shared library drags in the full CRT chain on
# Windows and duplicates compiler-rt symbols, and the moredeps wrapper only
# consumes the static archive and header.
file(READ "${GHOSTTY_BUILD_DIR}/build.zig" _ghostty_build_zig)
string(REPLACE
[==[
        const lib_shared = try buildpkg.GhosttyLib.initShared(b, &deps);
        const lib_static = try buildpkg.GhosttyLib.initStatic(b, &deps);

        // We shouldn't have this guard but we don't currently
        // build on macOS this way ironically so we need to fix that.
        if (!config.target.result.os.tag.isDarwin()) {
            lib_shared.installHeader(); // Only need one header
            if (config.target.result.os.tag == .windows) {
                lib_shared.install("ghostty-internal.dll");
                lib_static.install("ghostty-internal-static.lib");
            } else {
                lib_shared.install("ghostty-internal.so");
                lib_static.install("ghostty-internal.a");
            }
        }
]==]
[==[
        const lib_static = try buildpkg.GhosttyLib.initStatic(b, &deps);

        // We shouldn't have this guard but we don't currently
        // build on macOS this way ironically so we need to fix that.
        if (!config.target.result.os.tag.isDarwin()) {
            lib_static.installHeader(); // Only need one header
            if (config.target.result.os.tag == .windows) {
                lib_static.install("ghostty-internal-static.lib");
            } else {
                lib_static.install("ghostty-internal.a");
            }
        }
]==]
  _ghostty_build_zig "${_ghostty_build_zig}")
file(WRITE "${GHOSTTY_BUILD_DIR}/build.zig" "${_ghostty_build_zig}")

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
  -Dapp-runtime=none
  -Demit-lib-vt=false
  -Demit-macos-app=false
  -Demit-docs=false
  -Demit-webdata=false
  -Demit-bench=false
  -Demit-helpgen=false
  -Demit-terminfo=false
  -Demit-termcap=false
  -Demit-themes=false
  ${GHOSTTY_ZIG_OPTIMIZE}
  --cache-dir ${GHOSTTY_ZIG_CACHE}
  --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
)

set(GHOSTTY_ZIG_ARGS
  build
  -Dapp-runtime=none
  -Demit-lib-vt=false
  -Demit-macos-app=false
  -Demit-docs=false
  -Demit-webdata=false
  -Demit-bench=false
  -Demit-helpgen=false
  -Demit-terminfo=false
  -Demit-termcap=false
  -Demit-themes=false
  ${GHOSTTY_ZIG_OPTIMIZE}
  --cache-dir ${GHOSTTY_ZIG_CACHE}
  --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
)

# For non-Apple platforms we must explicitly tell zig build which target we are
# building for, because the wrapper may run on a macOS host while targeting
# Linux or Windows. Use the MSVC ABI on Windows when the build itself is
# running on Windows, and the GNU ABI when cross-compiling from another host.
if(GHOSTTY_IS_APPLE)
  list(APPEND GHOSTTY_ZIG_ARGS -Demit-xcframework=true)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(CMAKE_OSX_SYSROOT MATCHES "Simulator")
      list(APPEND GHOSTTY_ZIG_ARGS -Dtarget=aarch64-ios-simulator)
    else()
      list(APPEND GHOSTTY_ZIG_ARGS -Dtarget=aarch64-ios)
    endif()
  endif()
else()
  list(APPEND GHOSTTY_ZIG_ARGS -Demit-xcframework=false)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
      set(GHOSTTY_ZIG_TARGET "x86_64-linux-gnu")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
      set(GHOSTTY_ZIG_TARGET "aarch64-linux-gnu")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
      set(_ghostty_windows_arch "aarch64")
    else()
      set(_ghostty_windows_arch "x86_64")
    endif()
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
      set(GHOSTTY_ZIG_TARGET "${_ghostty_windows_arch}-windows-msvc")
    else()
      set(GHOSTTY_ZIG_TARGET "${_ghostty_windows_arch}-windows-gnu")
    endif()
  endif()
  if(GHOSTTY_ZIG_TARGET)
    list(APPEND GHOSTTY_ZIG_ARGS -Dtarget=${GHOSTTY_ZIG_TARGET})
  endif()
endif()

# Locate the output artifacts after the build.
if(GHOSTTY_IS_APPLE)
  set(GHOSTTY_XCFRAMEWORK_DIR "${GHOSTTY_BUILD_DIR}/macos/GhosttyKit.xcframework")
  set(GHOSTTY_LIB "${GHOSTTY_XCFRAMEWORK_DIR}/${GHOSTTY_APPLE_SLICE}/${GHOSTTY_APPLE_LIB_NAME}")
  set(GHOSTTY_HEADER "${GHOSTTY_XCFRAMEWORK_DIR}/${GHOSTTY_APPLE_SLICE}/Headers/ghostty.h")
else()
  set(GHOSTTY_ZIG_OUT_DIR "${GHOSTTY_BUILD_DIR}/zig-out")
  if(WIN32)
    set(GHOSTTY_LIB "${GHOSTTY_ZIG_OUT_DIR}/lib/ghostty-internal-static.lib")
  else()
    set(GHOSTTY_LIB "${GHOSTTY_ZIG_OUT_DIR}/lib/ghostty-internal.a")
  endif()
  set(GHOSTTY_HEADER "${GHOSTTY_ZIG_OUT_DIR}/include/ghostty.h")
endif()

# The vt headers are always produced in zig-out/include/ghostty/ regardless of
# the target platform. Install them alongside the main embedding header.
set(GHOSTTY_VT_HEADERS_DIR "${GHOSTTY_BUILD_DIR}/zig-out/include/ghostty")

add_custom_command(
  OUTPUT ${GHOSTTY_LIB}
  COMMAND ${ZIG_EXECUTABLE} ${GHOSTTY_ZIG_ARGS}
  WORKING_DIRECTORY ${GHOSTTY_BUILD_DIR}
  COMMENT "Building libghostty (full API)"
  USES_TERMINAL
)

add_custom_target(ghostty_build ALL DEPENDS ${GHOSTTY_LIB})

# Install the static library with a conventional Unix name (lib prefix). On
# Windows we keep the upstream name because MSVC .lib files do not use the
# lib prefix convention.
if(WIN32)
  install(FILES ${GHOSTTY_LIB} DESTINATION lib)
else()
  install(FILES ${GHOSTTY_LIB} DESTINATION lib RENAME libghostty-internal.a)
endif()

install(FILES ${GHOSTTY_HEADER} DESTINATION include)
install(DIRECTORY ${GHOSTTY_VT_HEADERS_DIR} DESTINATION include)
