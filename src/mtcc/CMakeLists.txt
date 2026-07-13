cmake_minimum_required(VERSION 3.25)
project(mtcc LANGUAGES C)

set(MTCC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/mtcc")
set(MTCC_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/mtcc_build")

file(MAKE_DIRECTORY ${MTCC_BUILD_DIR})

if(WIN32)
  # mtcc on Windows must be built with MSVC (cl/lib). The upstream source ships a
  # win32/ directory and x86_64 code generators, so we copy the source into the
  # build tree and run a custom batch there. This keeps the submodule clean.
  #
  # ARM64 Windows is not supported yet because the batch would need to use the
  # arm64-* source files and a different TCC_TARGET_* define.
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64|i[3-6]86")
    message(FATAL_ERROR "mtcc Windows wrapper currently only supports x86/x64, not ${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  file(COPY ${MTCC_SRC_DIR}/ DESTINATION ${MTCC_BUILD_DIR})
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/build_mtcc_windows.bat.in"
    "${MTCC_BUILD_DIR}/win32/build_mtcc_windows.bat"
    @ONLY
  )

  add_custom_command(
    OUTPUT
      ${MTCC_BUILD_DIR}/win32/libtcc.lib
      ${MTCC_BUILD_DIR}/lib/libtcc1.a
    COMMAND cmd /c "${MTCC_BUILD_DIR}/win32/build_mtcc_windows.bat"
    WORKING_DIRECTORY ${MTCC_BUILD_DIR}
    DEPENDS
      "${MTCC_BUILD_DIR}/win32/build_mtcc_windows.bat"
      ${MTCC_BUILD_DIR}/libtcc.c
    COMMENT "Building mtcc for Windows x64"
  )

  add_custom_target(mtcc_build ALL DEPENDS
    ${MTCC_BUILD_DIR}/win32/libtcc.lib
    ${MTCC_BUILD_DIR}/lib/libtcc1.a
  )

  install(FILES ${MTCC_BUILD_DIR}/win32/libtcc.lib TYPE LIB)
  install(FILES ${MTCC_BUILD_DIR}/lib/libtcc1.a TYPE LIB)
  install(FILES
    ${MTCC_BUILD_DIR}/libtcc.h
    ${MTCC_BUILD_DIR}/tcc.h
    DESTINATION include
  )
  install(DIRECTORY ${MTCC_BUILD_DIR}/include/ DESTINATION include)
else()
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
endif()
