cmake_minimum_required(VERSION 3.25)
if(APPLE AND False)
  project(sokol_time LANGUAGES C OBJC)
else()
  project(sokol_time LANGUAGES C)
endif()

add_library(sokol_time sokol_time.c)
target_include_directories(sokol_time PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_time PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_time PUBLIC SOKOL_DLL)
endif()



if(APPLE AND False)
  target_compile_options(sokol_time PRIVATE -x objective-c)
  target_link_libraries(sokol_time PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_time
  EXPORT sokol_timeTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_time.h DESTINATION include)

install(EXPORT sokol_timeTargets
  FILE sokol_timeTargets.cmake
  NAMESPACE sokol_time::
  DESTINATION lib/cmake/sokol_time
)
