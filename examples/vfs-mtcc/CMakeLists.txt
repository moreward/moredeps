cmake_minimum_required(VERSION 3.25)
project(vfs-mtcc-example LANGUAGES C)

find_package(PhysFS REQUIRED)
find_package(mtcc REQUIRED)

add_executable(vfs-mtcc main.c)
target_link_libraries(vfs-mtcc PRIVATE PhysFS::PhysFS-static mtcc::mtcc)
target_include_directories(vfs-mtcc PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/vfs"
)

set_target_properties(vfs-mtcc PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

add_custom_target(vfs-mtcc-test
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
          zip -r "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" .
  COMMAND "$<TARGET_FILE:vfs-mtcc>" "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  DEPENDS vfs-mtcc
  COMMENT "Running sandboxed mtcc PhysFS example"
)
