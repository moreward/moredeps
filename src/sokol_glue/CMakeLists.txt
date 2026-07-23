cmake_minimum_required(VERSION 3.25)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(sokol_glue LANGUAGES C OBJC)
else()
  project(sokol_glue LANGUAGES C)
endif()

add_library(sokol_glue STATIC sokol_glue.c)
target_include_directories(sokol_glue PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_glue PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if(APPLE AND (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  target_compile_options(sokol_glue PRIVATE -x objective-c)
  target_link_libraries(sokol_glue PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_glue
  EXPORT sokol_glueTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_glue.h DESTINATION include)

install(EXPORT sokol_glueTargets
  FILE sokol_glueTargets.cmake
  NAMESPACE sokol_glue::
  DESTINATION lib/cmake/sokol_glue
)
