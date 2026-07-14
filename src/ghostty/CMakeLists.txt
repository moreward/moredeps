cmake_minimum_required(VERSION 3.25)
project(ghostty LANGUAGES C)

find_program(ZIG_EXECUTABLE zig REQUIRED)

set(GHOSTTY_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ghostty")
set(GHOSTTY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty_src")
set(GHOSTTY_ZIG_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-cache")
set(GHOSTTY_ZIG_GLOBAL_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache")

# Ghostty writes build artifacts into its source tree. Copy it to the build
# tree so the submodule stays clean.
file(REMOVE_RECURSE ${GHOSTTY_BUILD_DIR})
file(COPY ${GHOSTTY_SRC_DIR}/ DESTINATION ${GHOSTTY_BUILD_DIR})

set(GHOSTTY_HEADER "${GHOSTTY_BUILD_DIR}/include/ghostty.h")

# Platform-specific build flags and output paths.
# Ghostty's zig build produces different artifacts per platform:
# - macOS: xcframework (when -Demit-xcframework=true)
# - Linux/Windows: static/shared libs directly
if(APPLE)
  set(GHOSTTY_ZIG_ARGS
    -Doptimize=ReleaseFast
    -Dapp-runtime=none
    -Demit-macos-app=false
    -Demit-xcframework=true
    -p ${CMAKE_INSTALL_PREFIX}
    --cache-dir ${GHOSTTY_ZIG_CACHE}
    --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
  )
  set(GHOSTTY_XCFRAMEWORK "${GHOSTTY_BUILD_DIR}/macos/GhosttyKit.xcframework")
  set(GHOSTTY_LIB "${GHOSTTY_XCFRAMEWORK}/macos-arm64_x86_64/libghostty.a")
else()
  # On non-Apple platforms, ghostty's zig build installs libghostty.a and
  # libghostty.so directly into the prefix. We run zig build with the
  # install step and then find the installed artifacts.
  set(GHOSTTY_ZIG_ARGS
    build
    -Doptimize=ReleaseFast
    -Dapp-runtime=none
    -p ${CMAKE_INSTALL_PREFIX}
    --cache-dir ${GHOSTTY_ZIG_CACHE}
    --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
  )
  # After zig build installs, the static lib is in the prefix.
  if(WIN32)
    set(GHOSTTY_LIB "${CMAKE_INSTALL_PREFIX}/lib/ghostty.lib")
  else()
    set(GHOSTTY_LIB "${CMAKE_INSTALL_PREFIX}/lib/libghostty.a")
  endif()
endif()

if(APPLE)
  add_custom_command(
    OUTPUT ${GHOSTTY_LIB}
    COMMAND ${ZIG_EXECUTABLE} build ${GHOSTTY_ZIG_ARGS}
    WORKING_DIRECTORY ${GHOSTTY_BUILD_DIR}
    COMMENT "Building libghostty (macOS xcframework)"
  )
else()
  add_custom_command(
    OUTPUT ${GHOSTTY_LIB}
    COMMAND ${ZIG_EXECUTABLE} ${GHOSTTY_ZIG_ARGS}
    WORKING_DIRECTORY ${GHOSTTY_BUILD_DIR}
    COMMENT "Building libghostty"
  )
endif()

add_custom_target(ghostty_build ALL DEPENDS ${GHOSTTY_LIB})

install(FILES ${GHOSTTY_LIB} DESTINATION lib)
install(FILES ${GHOSTTY_HEADER} DESTINATION include)
install(DIRECTORY ${GHOSTTY_BUILD_DIR}/include/ghostty DESTINATION include)
