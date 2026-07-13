cmake_minimum_required(VERSION 3.25)
project(raudio LANGUAGES C)

add_library(raudio STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/raudio/src/raudio.c
)
target_include_directories(raudio PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/raudio/src>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(raudio PRIVATE
  RAUDIO_STANDALONE
  SUPPORT_MODULE_RAUDIO
)
set_target_properties(raudio PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
install(TARGETS raudio EXPORT raudioTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/raudio/src/raudio.h DESTINATION include)
install(EXPORT raudioTargets FILE raudioTargets.cmake NAMESPACE raudio:: DESTINATION lib/cmake/raudio)
