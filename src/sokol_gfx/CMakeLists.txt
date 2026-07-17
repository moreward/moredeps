cmake_minimum_required(VERSION 3.25)
if(APPLE AND TRUE)
  project(sokol_gfx LANGUAGES C OBJC)
else()
  project(sokol_gfx LANGUAGES C)
endif()

add_library(sokol_gfx sokol_gfx.c)
target_include_directories(sokol_gfx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_gfx PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_gfx PUBLIC SOKOL_DLL)
endif()



if(APPLE AND TRUE)
  target_compile_options(sokol_gfx PRIVATE -x objective-c)
  target_link_libraries(sokol_gfx PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_gfx
  EXPORT sokol_gfxTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_gfx.h DESTINATION include)

install(EXPORT sokol_gfxTargets
  FILE sokol_gfxTargets.cmake
  NAMESPACE sokol_gfx::
  DESTINATION lib/cmake/sokol_gfx
)
