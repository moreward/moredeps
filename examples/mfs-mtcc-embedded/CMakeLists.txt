cmake_minimum_required(VERSION 3.25)
project(mfs-mtcc-embedded-example LANGUAGES C)

find_package(PhysFS REQUIRED)
find_package(mtcc REQUIRED)

add_executable(mfs-mtcc-embedded main.c)
target_link_libraries(mfs-mtcc-embedded PRIVATE PhysFS::PhysFS-static mtcc::mtcc)
target_include_directories(mfs-mtcc-embedded PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/mfs"
)

set_target_properties(mfs-mtcc-embedded PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

# Optional: a helper target that creates the example archive.
# The archive must contain:
#   jit/main.c          - the source to compile
#   runtime/libtcc1.a   - the mtcc runtime library
#   runtime/include/    - mtcc builtin headers
#
# To build the archive, provide the mtcc runtime directory via
# -DMTCC_RUNTIME_DIR=<path> (defaults to /usr/local/lib/tcc).
set(MTCC_RUNTIME_DIR "/usr/local/lib/tcc" CACHE PATH "Directory containing libtcc1.a and include/")

add_custom_target(mfs-mtcc-embedded-zip
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/runtime"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/runtime/include"
  COMMAND ${CMAKE_COMMAND} -E copy
          "${MTCC_RUNTIME_DIR}/libtcc1.a"
          "${CMAKE_CURRENT_BINARY_DIR}/runtime/libtcc1.a"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${MTCC_RUNTIME_DIR}/include"
          "${CMAKE_CURRENT_BINARY_DIR}/runtime/include"
  COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_BINARY_DIR}"
          zip -r "scripts.zip" runtime
  COMMAND ${CMAKE_COMMAND} -E chdir
          "${CMAKE_CURRENT_SOURCE_DIR}/jit"
          zip -r "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip" main.c
  DEPENDS mfs-mtcc-embedded
  COMMENT "Creating self-contained mtcc PhysFS archive"
)

add_custom_target(mfs-mtcc-embedded-test
  COMMAND "$<TARGET_FILE:mfs-mtcc-embedded>" "${CMAKE_CURRENT_BINARY_DIR}/scripts.zip"
  DEPENDS mfs-mtcc-embedded-zip
  COMMENT "Running self-contained mtcc PhysFS example"
)
