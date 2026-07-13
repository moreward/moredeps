cmake_minimum_required(VERSION 3.25)
if(APPLE)
  project(sokol_gp LANGUAGES C OBJC)
else()
  project(sokol_gp LANGUAGES C)
endif()

add_library(sokol_gp STATIC sokol_gp.c)
target_include_directories(sokol_gp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol_gp>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_gp PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if(APPLE)
  target_compile_options(sokol_gp PRIVATE -x objective-c)
  target_link_libraries(sokol_gp PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_gp
  EXPORT sokol_gpTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol_gp/sokol_gp.h DESTINATION include)

install(EXPORT sokol_gpTargets
  FILE sokol_gpTargets.cmake
  NAMESPACE sokol_gp::
  DESTINATION lib/cmake/sokol_gp
)
