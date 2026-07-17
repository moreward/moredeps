cmake_minimum_required(VERSION 3.25)
project(fontstash LANGUAGES C)

add_library(fontstash fontstash.c)
target_include_directories(fontstash PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/fontstash/src>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(fontstash PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(fontstash PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS fontstash
  EXPORT fontstashTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/fontstash/src/fontstash.h DESTINATION include)

install(EXPORT fontstashTargets
  FILE fontstashTargets.cmake
  NAMESPACE fontstash::
  DESTINATION lib/cmake/fontstash
)
