cmake_minimum_required(VERSION 3.25)
project(stb_image_resize LANGUAGES C)

add_library(stb_image_resize STATIC stb_image_resize.c)
target_include_directories(stb_image_resize PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(stb_image_resize PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

install(TARGETS stb_image_resize
  EXPORT stb_image_resizeTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_image_resize2.h DESTINATION include)

install(EXPORT stb_image_resizeTargets
  FILE stb_image_resizeTargets.cmake
  NAMESPACE stb_image_resize::
  DESTINATION lib/cmake/stb_image_resize
)
