cmake_minimum_required(VERSION 3.25)
project(FastNoiseLite LANGUAGES C)

add_library(FastNoiseLite STATIC FastNoiseLite.c)
target_include_directories(FastNoiseLite PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/FastNoiseLite/C>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(FastNoiseLite PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

install(TARGETS FastNoiseLite
  EXPORT FastNoiseLiteTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/FastNoiseLite/C/FastNoiseLite.h DESTINATION include)

install(EXPORT FastNoiseLiteTargets
  FILE FastNoiseLiteTargets.cmake
  NAMESPACE FastNoiseLite::
  DESTINATION lib/cmake/FastNoiseLite
)
