cmake_minimum_required(VERSION 3.25)
project(vma LANGUAGES CXX)

add_library(vma vma.cpp)
target_include_directories(vma PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../deps/vma/include>
  $<INSTALL_INTERFACE:include>
)

# Vulkan headers are a system dependency. On macOS they live under
# /opt/homebrew (ARM) or /usr/local (Intel). On Linux they are typically
# under /usr/include. On Windows they come from the Vulkan SDK.
# We add common system include roots; the compile will error clearly if
# vulkan/vulkan.h is still not found.
if(APPLE)
  target_include_directories(vma PUBLIC /opt/homebrew/include /usr/local/include)
endif()

set_target_properties(vma PROPERTIES
  CXX_STANDARD 17
  POSITION_INDEPENDENT_CODE ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

install(TARGETS vma
  EXPORT vmaTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/vma/include/vk_mem_alloc.h DESTINATION include)
install(EXPORT vmaTargets
  FILE vmaTargets.cmake
  NAMESPACE vma::
  DESTINATION lib/cmake/vma
)
# Also install a standard Config file so find_package(vma) works.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vmaConfig.cmake"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/vmaTargets.cmake\")\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vmaConfig.cmake"
  DESTINATION lib/cmake/vma)
