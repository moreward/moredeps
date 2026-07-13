cmake_minimum_required(VERSION 3.25)
project(ghostty LANGUAGES C)

find_program(ZIG_EXECUTABLE zig REQUIRED)

set(GHOSTTY_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ghostty")
set(GHOSTTY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty_src")
set(GHOSTTY_ZIG_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-cache")
set(GHOSTTY_ZIG_GLOBAL_CACHE "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache")

# Ghostty writes build artifacts (xcodebuild, xcframework) into its source tree.
# Copy it to the build tree so the submodule stays clean.
file(REMOVE_RECURSE ${GHOSTTY_BUILD_DIR})
file(COPY ${GHOSTTY_SRC_DIR}/ DESTINATION ${GHOSTTY_BUILD_DIR})

set(GHOSTTY_XCFRAMEWORK "${GHOSTTY_BUILD_DIR}/macos/GhosttyKit.xcframework")
set(GHOSTTY_LIB "${GHOSTTY_XCFRAMEWORK}/macos-arm64_x86_64/libghostty.a")
set(GHOSTTY_HEADER "${GHOSTTY_BUILD_DIR}/include/ghostty.h")

add_custom_command(
  OUTPUT ${GHOSTTY_LIB}
  COMMAND ${ZIG_EXECUTABLE} build
    -Doptimize=ReleaseFast
    -Dapp-runtime=none
    -Demit-macos-app=false
    -Demit-xcframework=true
    -p ${CMAKE_INSTALL_PREFIX}
    --cache-dir ${GHOSTTY_ZIG_CACHE}
    --global-cache-dir ${GHOSTTY_ZIG_GLOBAL_CACHE}
  WORKING_DIRECTORY ${GHOSTTY_BUILD_DIR}
  COMMENT "Building libghostty"
)

add_custom_target(ghostty_build ALL DEPENDS ${GHOSTTY_LIB})

install(FILES ${GHOSTTY_LIB} DESTINATION lib)
install(FILES ${GHOSTTY_HEADER} DESTINATION include)
install(DIRECTORY ${GHOSTTY_BUILD_DIR}/include/ghostty DESTINATION include)
