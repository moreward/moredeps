cmake_minimum_required(VERSION 3.25)
project(ubench LANGUAGES C)

add_library(ubench ubench.c)
target_include_directories(ubench PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ubench>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(ubench PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(ubench PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS ubench
  EXPORT ubenchTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/ubench/ubench.h DESTINATION include)

install(EXPORT ubenchTargets
  FILE ubenchTargets.cmake
  NAMESPACE ubench::
  DESTINATION lib/cmake/ubench
)
