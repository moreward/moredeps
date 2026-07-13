# Toolchain for native Linux x64 builds.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.

set(MOREDEPS_PLATFORM "linux_x64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "gcc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "g++" CACHE STRING "C++ compiler")
