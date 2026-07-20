cmake_minimum_required(VERSION 3.25)
project(mfs-mtcc-example LANGUAGES C)

find_package(PhysFS REQUIRED)
find_package(mtcc REQUIRED)

add_executable(mfs-mtcc main.c)
target_link_libraries(mfs-mtcc PRIVATE PhysFS::PhysFS-static mtcc::mtcc)
target_include_directories(mfs-mtcc PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/mfs"
)

set_target_properties(mfs-mtcc PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

add_custom_target(mfs-mtcc-test
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
          zip -r "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" .
  COMMAND "$<TARGET_FILE:mfs-mtcc>" "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  DEPENDS mfs-mtcc
  COMMENT "Running sandboxed mtcc PhysFS example"
)
