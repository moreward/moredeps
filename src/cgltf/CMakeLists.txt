cmake_minimum_required(VERSION 3.25)
project(cgltf LANGUAGES C)

add_library(cgltf cgltf.c)
target_include_directories(cgltf PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/cgltf>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(cgltf PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(cgltf PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)


install(TARGETS cgltf
  EXPORT cgltfTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/cgltf/cgltf.h DESTINATION include)

install(EXPORT cgltfTargets
  FILE cgltfTargets.cmake
  NAMESPACE cgltf::
  DESTINATION lib/cmake/cgltf
)
