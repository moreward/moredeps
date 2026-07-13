cmake_minimum_required(VERSION 3.25)
project(tinycsocket LANGUAGES C)

# Pull in the upstream build rules.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../deps/tinycsocket ${CMAKE_CURRENT_BINARY_DIR}/upstream)

# Upstream has no install rules, so add minimal ones here.
install(TARGETS tinycsocket
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/tinycsocket/include/tinycsocket.h DESTINATION include)
