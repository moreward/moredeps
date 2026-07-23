# Toolchain for cross-compiling to iOS simulator (arm64 – Apple Silicon host).
# Requires Xcode with the iOS SDK.
#
# Usage:
#   cmake -S . -B _b/ios_simulator_arm64 -DCMAKE_TOOLCHAIN_FILE=toolchain/ios_simulator_arm64.cmake

set(MOREDEPS_PLATFORM "ios_simulator_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_OSX_ARCHITECTURES arm64)

set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS deployment target")

# Use the simulator SDK.
set(CMAKE_OSX_SYSROOT iphonesimulator CACHE STRING "iOS SDK")

set(CMAKE_C_COMPILER "cc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "c++" CACHE STRING "C++ compiler")

set(MOREDEPS_BUILD_SHARED OFF CACHE BOOL "Skip shared library pass on iOS" FORCE)

# Disable MACOSX_BUNDLE so executables like bssl don't trigger bundle 
# install rules (we only care about static libraries).
set(CMAKE_MACOSX_BUNDLE OFF CACHE BOOL "Disable bundle generation on iOS" FORCE)
