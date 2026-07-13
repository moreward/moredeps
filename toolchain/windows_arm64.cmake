# Toolchain for Windows arm64 builds.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.
# Assumes MSVC or clang-cl with ARM64 targeting is available in the environment.

set(MOREDEPS_PLATFORM "windows_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

set(CMAKE_C_COMPILER "cl" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "cl" CACHE STRING "C++ compiler")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
