cmake_minimum_required(VERSION 3.25)
project(mfs-lua-example LANGUAGES C)

find_package(PhysFS REQUIRED)
find_package(lua REQUIRED)
find_package(mimalloc REQUIRED)
find_package(PCRE2 REQUIRED)
find_package(utf8proc REQUIRED)

add_executable(mfs-lua main.c mfs_lua.c)
target_link_libraries(mfs-lua PRIVATE
  PhysFS::PhysFS-static
  lua::lua
  mimalloc
  PCRE2::PCRE2
  utf8proc::utf8proc
)
target_include_directories(mfs-lua PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/mfs"
)

set_target_properties(mfs-lua PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

# A simple test target that creates a zip archive from scripts/ and runs the example.
add_custom_target(mfs-lua-test
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/write_dir"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/write_dir"
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
          zip -r "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" .
  COMMAND "$<TARGET_FILE:mfs-lua>" "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" "${CMAKE_CURRENT_BINARY_DIR}/write_dir"
  DEPENDS mfs-lua
  COMMENT "Running sandboxed Lua PhysFS example"
)
