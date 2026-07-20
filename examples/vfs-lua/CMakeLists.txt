cmake_minimum_required(VERSION 3.25)
project(vfs-lua-example LANGUAGES C)

find_package(PhysFS REQUIRED)
find_package(lua REQUIRED)

add_executable(vfs-lua main.c)
target_link_libraries(vfs-lua PRIVATE PhysFS::PhysFS-static lua::lua)
target_include_directories(vfs-lua PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/vfs"
)

set_target_properties(vfs-lua PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

# A simple test target that creates a zip archive from scripts/ and runs the example.
add_custom_target(vfs-lua-test
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
          zip -r "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" .
  COMMAND "$<TARGET_FILE:vfs-lua>" "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  DEPENDS vfs-lua
  COMMENT "Running sandboxed Lua PhysFS example"
)
