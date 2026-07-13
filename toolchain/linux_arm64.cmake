# Toolchain for cross-compiling Linux arm64 from x64.
# This is used by the top-level CMakeLists.txt and scripts/build_all.sh.
# Assumes a GNU cross toolchain such as aarch64-linux-gnu-gcc is installed.

set(MOREDEPS_PLATFORM "linux_arm64" CACHE STRING "moredeps target platform")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc" CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++" CACHE STRING "C++ compiler")

# Try to discover the cross compiler's sysroot automatically. If the compiler
# reports a sysroot, use it so that system headers and libraries are found
# correctly. If it reports "/" or nothing, leave it unset and rely on the
# compiler's built-in search paths.
execute_process(
  COMMAND ${CMAKE_C_COMPILER} -print-sysroot
  OUTPUT_VARIABLE _LINUX_ARM64_SYSROOT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(_LINUX_ARM64_SYSROOT AND NOT _LINUX_ARM64_SYSROOT STREQUAL "/")
  set(CMAKE_SYSROOT "${_LINUX_ARM64_SYSROOT}" CACHE PATH "Cross-compilation sysroot")
endif()
unset(_LINUX_ARM64_SYSROOT)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Make sure the common install prefix is searched for packages built earlier
# in this super-build. This is appended by the top-level CMakeLists as well,
# but setting it here ensures the toolchain is self-describing.
list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_INSTALL_PREFIX}")
