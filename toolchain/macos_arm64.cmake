# Toolchain for native macOS arm64 builds.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.

set(MOREDEPS_PLATFORM "macos_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_OSX_ARCHITECTURES arm64)

# Use the system Apple Clang. The user can override via CC/CXX env vars.
set(CMAKE_C_COMPILER "cc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "c++" CACHE STRING "C++ compiler")
