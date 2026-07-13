cmake_minimum_required(VERSION 3.25)
project(tinycsocket LANGUAGES C)

set(TINYCSOCKET_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/tinycsocket")
set(TINYCSOCKET_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/upstream")

# Upstream's CMakeLists writes generated headers back into the source tree.
# Copy the source into the build tree so the submodule stays clean.
file(REMOVE_RECURSE ${TINYCSOCKET_BUILD_DIR})
file(COPY ${TINYCSOCKET_SRC_DIR}/ DESTINATION ${TINYCSOCKET_BUILD_DIR})

add_subdirectory(${TINYCSOCKET_BUILD_DIR} ${TINYCSOCKET_BUILD_DIR})

# Upstream has no install rules, so add minimal ones here.
install(TARGETS tinycsocket
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${TINYCSOCKET_BUILD_DIR}/include/tinycsocket.h DESTINATION include)
