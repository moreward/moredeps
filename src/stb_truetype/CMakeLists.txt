cmake_minimum_required(VERSION 3.25)
project(stb_truetype LANGUAGES C)

add_library(stb_truetype STATIC stb_truetype.c)
target_include_directories(stb_truetype PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(stb_truetype PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

install(TARGETS stb_truetype
  EXPORT stb_truetypeTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/stb/stb_truetype.h DESTINATION include)

install(EXPORT stb_truetypeTargets
  FILE stb_truetypeTargets.cmake
  NAMESPACE stb_truetype::
  DESTINATION lib/cmake/stb_truetype
)
