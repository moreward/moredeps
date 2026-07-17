cmake_minimum_required(VERSION 3.25)
project(stb_rect_pack LANGUAGES C)

add_library(stb_rect_pack stb_rect_pack.c)
target_include_directories(stb_rect_pack PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(stb_rect_pack PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(stb_rect_pack PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS stb_rect_pack
  EXPORT stb_rect_packTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_rect_pack.h DESTINATION include)

install(EXPORT stb_rect_packTargets
  FILE stb_rect_packTargets.cmake
  NAMESPACE stb_rect_pack::
  DESTINATION lib/cmake/stb_rect_pack
)
