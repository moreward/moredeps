cmake_minimum_required(VERSION 3.25)
project(utest LANGUAGES C)

add_library(utest STATIC utest.c)
target_include_directories(utest PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/utest>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(utest PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

install(TARGETS utest
  EXPORT utestTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/utest/utest.h DESTINATION include)

install(EXPORT utestTargets
  FILE utestTargets.cmake
  NAMESPACE utest::
  DESTINATION lib/cmake/utest
)
