cmake_minimum_required(VERSION 3.25)
if(APPLE AND False)
  project(sokol_log LANGUAGES C OBJC)
else()
  project(sokol_log LANGUAGES C)
endif()

add_library(sokol_log sokol_log.c)
target_include_directories(sokol_log PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_log PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_log PUBLIC SOKOL_DLL)
endif()



if(APPLE AND False)
  target_compile_options(sokol_log PRIVATE -x objective-c)
  target_link_libraries(sokol_log PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_log
  EXPORT sokol_logTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_log.h DESTINATION include)

install(EXPORT sokol_logTargets
  FILE sokol_logTargets.cmake
  NAMESPACE sokol_log::
  DESTINATION lib/cmake/sokol_log
)
