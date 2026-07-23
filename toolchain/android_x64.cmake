# Toolchain for cross-compiling to Android x86_64 (emulator/testing).
# Requires the Android NDK.
#
# Usage:
#   export ANDROID_NDK=/path/to/ndk
#   cmake -S . -B _b/android_x64 -DCMAKE_TOOLCHAIN_FILE=toolchain/android_x64.cmake

set(MOREDEPS_PLATFORM "android_x64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 21 CACHE STRING "Minimum Android API level")
set(ANDROID_PLATFORM_LEVEL 21 CACHE STRING "Android API level (for deps that use this var)")
set(CMAKE_ANDROID_ARCH_ABI x86_64)
set(CMAKE_ANDROID_STL_TYPE c++_static)

# NDK discovery (shared logic with android_arm64.cmake).
if(NOT DEFINED ANDROID_NDK OR ANDROID_NDK STREQUAL "")
  if(APPLE AND EXISTS "/opt/homebrew/share/android-ndk")
    set(ANDROID_NDK "/opt/homebrew/share/android-ndk")
  elseif(EXISTS "/usr/local/share/android-ndk")
    set(ANDROID_NDK "/usr/local/share/android-ndk")
  else()
    find_program(NDK_BUILD ndk-build)
    if(NDK_BUILD)
      get_filename_component(ANDROID_NDK "${NDK_BUILD}" DIRECTORY)
      get_filename_component(ANDROID_NDK "${ANDROID_NDK}" DIRECTORY)
    endif()
  endif()
endif()

if(ANDROID_NDK)
  set(CMAKE_ANDROID_NDK "${ANDROID_NDK}" CACHE PATH "Android NDK path")
else()
  message(FATAL_ERROR "Android NDK not found. Set ANDROID_NDK or install via:\n"
          "  brew install android-ndk  (macOS)\n"
          "  https://developer.android.com/ndk/downloads")
endif()

set(MOREDEPS_BUILD_SHARED OFF CACHE BOOL "Skip shared library pass on Android" FORCE)
