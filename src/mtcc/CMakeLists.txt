cmake_minimum_required(VERSION 3.25)
project(mtcc LANGUAGES C)

set(MTCC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/mtcc")
set(MTCC_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/mtcc_build")

file(MAKE_DIRECTORY ${MTCC_BUILD_DIR})

# Run mtcc's configure script in the build directory.
execute_process(
  COMMAND ${MTCC_SRC_DIR}/configure --cc=${CMAKE_C_COMPILER} --ar=${CMAKE_AR}
  WORKING_DIRECTORY ${MTCC_BUILD_DIR}
  RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR "mtcc configure failed")
endif()

# Build libtcc.a using mtcc's Makefile.
add_custom_command(
  OUTPUT ${MTCC_BUILD_DIR}/libtcc.a
  COMMAND ${CMAKE_MAKE_PROGRAM} -C ${MTCC_BUILD_DIR} libtcc.a
  DEPENDS ${MTCC_SRC_DIR}/Makefile ${MTCC_SRC_DIR}/libtcc.c
  COMMENT "Building libtcc.a"
)

add_custom_target(mtcc_build ALL DEPENDS ${MTCC_BUILD_DIR}/libtcc.a)

# Install the library and headers.
install(FILES ${MTCC_BUILD_DIR}/libtcc.a TYPE LIB)
install(FILES
  ${MTCC_SRC_DIR}/libtcc.h
  ${MTCC_SRC_DIR}/tcc.h
  DESTINATION include
)
