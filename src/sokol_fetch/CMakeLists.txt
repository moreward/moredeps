cmake_minimum_required(VERSION 3.25)
if(APPLE AND False)
  project(sokol_fetch LANGUAGES C OBJC)
else()
  project(sokol_fetch LANGUAGES C)
endif()

add_library(sokol_fetch sokol_fetch.c)
target_include_directories(sokol_fetch PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_fetch PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_fetch PUBLIC SOKOL_DLL)
endif()



if(APPLE AND False)
  target_compile_options(sokol_fetch PRIVATE -x objective-c)
  target_link_libraries(sokol_fetch PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_fetch
  EXPORT sokol_fetchTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol/sokol_fetch.h DESTINATION include)

install(EXPORT sokol_fetchTargets
  FILE sokol_fetchTargets.cmake
  NAMESPACE sokol_fetch::
  DESTINATION lib/cmake/sokol_fetch
)
