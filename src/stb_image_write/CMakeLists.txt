cmake_minimum_required(VERSION 3.25)
project(stb_image_write LANGUAGES C)

add_library(stb_image_write stb_image_write.c)
target_include_directories(stb_image_write PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(stb_image_write PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(stb_image_write PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS stb_image_write
  EXPORT stb_image_writeTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_image_write.h DESTINATION include)

install(EXPORT stb_image_writeTargets
  FILE stb_image_writeTargets.cmake
  NAMESPACE stb_image_write::
  DESTINATION lib/cmake/stb_image_write
)
