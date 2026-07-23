cmake_minimum_required(VERSION 3.25)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(sokol_app LANGUAGES C OBJC)
else()
  project(sokol_app LANGUAGES C)
endif()

add_library(sokol_app STATIC sokol_app.c)
target_include_directories(sokol_app PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(sokol_app PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

# Platform-specific 3D API selection.
if(APPLE AND (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  target_compile_definitions(sokol_app PRIVATE SOKOL_METAL)
  target_compile_options(sokol_app PRIVATE -x objective-c)
  target_link_libraries(sokol_app PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(sokol_app PRIVATE SOKOL_D3D11)
elseif(EMSCRIPTEN OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  target_compile_definitions(sokol_app PRIVATE SOKOL_GLES3)
else()
  target_compile_definitions(sokol_app PRIVATE SOKOL_GLCORE)
endif()

install(TARGETS sokol_app
  EXPORT sokol_appTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_app.h DESTINATION include)

install(EXPORT sokol_appTargets
  FILE sokol_appTargets.cmake
  NAMESPACE sokol_app::
  DESTINATION lib/cmake/sokol_app
)
