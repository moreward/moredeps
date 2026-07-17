cmake_minimum_required(VERSION 3.25)
if(APPLE AND False)
  project(sokol_args LANGUAGES C OBJC)
else()
  project(sokol_args LANGUAGES C)
endif()

add_library(sokol_args sokol_args.c)
target_include_directories(sokol_args PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_args PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_args PUBLIC SOKOL_DLL)
endif()



if(APPLE AND False)
  target_compile_options(sokol_args PRIVATE -x objective-c)
  target_link_libraries(sokol_args PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_args
  EXPORT sokol_argsTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_args.h DESTINATION include)

install(EXPORT sokol_argsTargets
  FILE sokol_argsTargets.cmake
  NAMESPACE sokol_args::
  DESTINATION lib/cmake/sokol_args
)
