cmake_minimum_required(VERSION 3.25)
project(mtcc LANGUAGES C)

set(MTCC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/mtcc")
set(MTCC_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/mtcc_build")

file(MAKE_DIRECTORY ${MTCC_BUILD_DIR})

if(WIN32)
  # mtcc on Windows must be built with MSVC (cl/lib). The upstream source ships a
  # win32/ directory and both x86_64 and arm64 code generators.
  #
  # We copy the source into the build tree and run a custom batch there.
  # This keeps the submodule clean.
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64|i[3-6]86|X86")
    set(MTCC_ARCH "x86_64")
    set(MTCC_TCC_TARGET "-DTCC_TARGET_X86_64")
    set(MTCC_MFLAG "-m64")
    # x86_64 uses win32/lib/*.c for crt files and ../lib/*.S for alloca
    set(MTCC_LIBSRCS
      libtcc.c tccpp.c tccgen.c tccdbg.c tccelf.c tccasm.c tccrun.c
      x86_64-gen.c x86_64-link.c i386-asm.c tccpe.c
    )
    set(MTCC_RT_WIN32
      win32/lib/crt1.c win32/lib/crt1w.c win32/lib/wincrt1.c win32/lib/wincrt1w.c
      win32/lib/dllcrt1.c win32/lib/dllmain.c win32/lib/chkstk.S
    )
    set(MTCC_RT_TOP
      lib/libtcc1.c lib/alloca.S lib/alloca-bt.S lib/stdatomic.c lib/atomic.S lib/builtin.c
    )
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64|AARCH64")
    set(MTCC_ARCH "arm64")
    set(MTCC_TCC_TARGET "-DTCC_TARGET_ARM64")
    set(MTCC_MFLAG "-marm64")
    # arm64 uses arm64-* code generators; no i386-asm.c needed
    set(MTCC_LIBSRCS
      libtcc.c tccpp.c tccgen.c tccdbg.c tccelf.c tccasm.c tccrun.c
      arm64-gen.c arm64-link.c arm64-asm.c tccpe.c
    )
    # ARM64 Windows runtime: no chkstk.S (Windows ARM64 has built-in stack probing)
    set(MTCC_RT_WIN32
      win32/lib/crt1.c win32/lib/crt1w.c win32/lib/wincrt1.c win32/lib/wincrt1w.c
      win32/lib/dllcrt1.c win32/lib/dllmain.c
    )
    set(MTCC_RT_TOP
      lib/libtcc1.c lib/alloca.S lib/alloca-bt.S lib/stdatomic.c lib/atomic.S lib/builtin.c
    )
  else()
    message(FATAL_ERROR "mtcc Windows wrapper: unsupported architecture ${CMAKE_SYSTEM_PROCESSOR}")
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
    COMMENT "Building mtcc for Windows ${MTCC_ARCH}"
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
  # Keep mtcc's MinGW-style standard headers away from the shared include root
  # so they don't shadow MSVC system headers for other dependencies.
  install(DIRECTORY ${MTCC_BUILD_DIR}/include/ DESTINATION include/mtcc)
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
