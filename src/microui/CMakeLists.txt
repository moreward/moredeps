cmake_minimum_required(VERSION 3.25)
project(microui LANGUAGES C)

add_library(microui
  ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/microui/src/microui.c
)
target_include_directories(microui PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/microui/src>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(microui PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
set_target_properties(microui PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

install(TARGETS microui EXPORT microuiTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/microui/src/microui.h DESTINATION include)
install(EXPORT microuiTargets FILE microuiTargets.cmake NAMESPACE microui:: DESTINATION lib/cmake/microui)
