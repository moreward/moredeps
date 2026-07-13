# Toolchain for cross-compiling Linux arm64 from x64.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.
# Assumes a GNU cross toolchain such as aarch64-linux-gnu-gcc is installed.

set(MOREDEPS_PLATFORM "linux_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++" CACHE STRING "C++ compiler")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
