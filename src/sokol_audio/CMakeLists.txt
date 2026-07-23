cmake_minimum_required(VERSION 3.25)
# On iOS 26+ SDK, CoreAudio headers pull in Foundation (ObjC).
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(sokol_audio LANGUAGES C OBJC)
else()
  project(sokol_audio LANGUAGES C)
endif()

add_library(sokol_audio sokol_audio.c)
target_include_directories(sokol_audio PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sokol_audio PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  target_compile_definitions(sokol_audio PUBLIC SOKOL_DLL)
endif()

if(APPLE AND (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_source_files_properties(sokol_audio.c PROPERTIES COMPILE_FLAGS "-x objective-c")
  endif()
  target_link_libraries(sokol_audio PUBLIC
    "-framework AudioToolbox"
    "-framework CoreAudio"
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
