cmake_minimum_required(VERSION 3.25)
project(tinycsocket LANGUAGES C)

set(TINYCSOCKET_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/tinycsocket")
set(TINYCSOCKET_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/upstream")

# Upstream's CMakeLists writes generated headers back into the source tree.
# Copy the source into the build tree so the submodule stays clean.
file(REMOVE_RECURSE ${TINYCSOCKET_BUILD_DIR})
file(COPY ${TINYCSOCKET_SRC_DIR}/ DESTINATION ${TINYCSOCKET_BUILD_DIR})

# Force the upstream build to honor BUILD_SHARED_LIBS without patching the submodule.
set(_TCS_CMAKE "${TINYCSOCKET_BUILD_DIR}/CMakeLists.txt")
file(READ "${_TCS_CMAKE}" _tcs_cmake_text)
string(REPLACE "add_library(tinycsocket STATIC" "add_library(tinycsocket" _tcs_cmake_text "${_tcs_cmake_text}")
file(WRITE "${_TCS_CMAKE}" "${_tcs_cmake_text}")

add_subdirectory(${TINYCSOCKET_BUILD_DIR} ${TINYCSOCKET_BUILD_DIR})
set_target_properties(tinycsocket PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Upstream has no install rules, so add minimal ones here.
install(TARGETS tinycsocket
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${TINYCSOCKET_BUILD_DIR}/include/tinycsocket.h DESTINATION include)
