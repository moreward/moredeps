cmake_minimum_required(VERSION 3.25)
if(APPLE AND False)
  project(sokol_audio LANGUAGES C OBJC)
else()
  project(sokol_audio LANGUAGES C)
endif()

add_library(sokol_audio STATIC sokol_audio.c)
target_include_directories(sokol_audio PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_audio PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if(APPLE AND False)
  target_compile_options(sokol_audio PRIVATE -x objective-c)
  target_link_libraries(sokol_audio PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_audio
  EXPORT sokol_audioTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_audio.h DESTINATION include)

install(EXPORT sokol_audioTargets
  FILE sokol_audioTargets.cmake
  NAMESPACE sokol_audio::
  DESTINATION lib/cmake/sokol_audio
)
