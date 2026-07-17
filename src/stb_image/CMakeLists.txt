cmake_minimum_required(VERSION 3.25)
project(stb_image LANGUAGES C)

add_library(stb_image stb_image.c)
target_include_directories(stb_image PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(stb_image PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(stb_image PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS stb_image
  EXPORT stb_imageTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_image.h DESTINATION include)

install(EXPORT stb_imageTargets
  FILE stb_imageTargets.cmake
  NAMESPACE stb_image::
  DESTINATION lib/cmake/stb_image
)
