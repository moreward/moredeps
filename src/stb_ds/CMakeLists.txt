cmake_minimum_required(VERSION 3.25)
project(stb_ds LANGUAGES C)

add_library(stb_ds stb_ds.c)
target_include_directories(stb_ds PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(stb_ds PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(stb_ds PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS stb_ds
  EXPORT stb_dsTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_ds.h DESTINATION include)

install(EXPORT stb_dsTargets
  FILE stb_dsTargets.cmake
  NAMESPACE stb_ds::
  DESTINATION lib/cmake/stb_ds
)
