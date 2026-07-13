cmake_minimum_required(VERSION 3.25)
project(nanovg LANGUAGES C)

add_library(nanovg STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/nanovg/src/nanovg.c
)
target_include_directories(nanovg PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/nanovg/src>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(nanovg PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
install(TARGETS nanovg EXPORT nanovgTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/nanovg/src/nanovg.h DESTINATION include)
install(EXPORT nanovgTargets FILE nanovgTargets.cmake NAMESPACE nanovg:: DESTINATION lib/cmake/nanovg)
