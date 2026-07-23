# Toolchain for cross-compiling to iOS device (arm64).
# Requires Xcode with the iOS SDK.
#
# Usage:
#   cmake -S . -B _b/ios_arm64 -DCMAKE_TOOLCHAIN_FILE=toolchain/ios_arm64.cmake

set(MOREDEPS_PLATFORM "ios_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_OSX_ARCHITECTURES arm64)

# Target iOS 15.0 as the minimum deployment version.
# This covers all devices from iPhone 13 onward.
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS deployment target")

# Use the device SDK.
set(CMAKE_OSX_SYSROOT iphoneos CACHE STRING "iOS SDK")

# Xcode-provided Clang. We set explicitly so ExternalProject passes it through.
set(CMAKE_C_COMPILER "cc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "c++" CACHE STRING "C++ compiler")

# Shared libraries are not useful on iOS (all code is linked into a single
# binary, and dynamic frameworks use a different mechanism).
set(MOREDEPS_BUILD_SHARED OFF CACHE BOOL "Skip shared library pass on iOS" FORCE)

# Disable MACOSX_BUNDLE so executables like bssl don't trigger bundle 
# install rules (we only care about static libraries).
set(CMAKE_MACOSX_BUNDLE OFF CACHE BOOL "Disable bundle generation on iOS" FORCE)
