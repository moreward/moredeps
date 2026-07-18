cmake_minimum_required(VERSION 3.25)
project(minigamepad LANGUAGES C)

add_library(minigamepad minigamepad.c)
target_compile_definitions(minigamepad PRIVATE MG_EXPORT)
target_include_directories(minigamepad PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/minigamepad>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(minigamepad PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(minigamepad PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if(APPLE)
  target_link_libraries(minigamepad PUBLIC
    "-framework IOKit"
    "-framework CoreFoundation"
  )
endif()


install(TARGETS minigamepad
  EXPORT minigamepadTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/minigamepad/minigamepad.h DESTINATION include)

install(EXPORT minigamepadTargets
  FILE minigamepadTargets.cmake
  NAMESPACE minigamepad::
  DESTINATION lib/cmake/minigamepad
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/minigamepad-config.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/minigamepadTargets.cmake\")\n"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/minigamepad-config.cmake"
  DESTINATION lib/cmake/minigamepad
)
