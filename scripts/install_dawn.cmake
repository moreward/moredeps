# Custom install script for Dawn.
# Dawn's upstream CMake install target only exists when DAWN_ENABLE_INSTALL is
# ON, which is disabled on Emscripten because there is no monolithic library to
# export. This script stages the artifacts that are produced on each platform.

if(NOT DAWN_BUILD_DIR)
  message(FATAL_ERROR "DAWN_BUILD_DIR not set")
endif()
if(NOT CMAKE_INSTALL_PREFIX)
  message(FATAL_ERROR "CMAKE_INSTALL_PREFIX not set")
endif()

file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/include")

if(EMSCRIPTEN)
  # Emscripten build produces emdawnwebgpu headers and JS library fragments.
  set(EMDAWN_INCLUDE_DIR "${DAWN_BUILD_DIR}/gen/src/emdawnwebgpu/include")
  set(EMDAWN_LIB_DIR "${DAWN_BUILD_DIR}/gen/src/emdawnwebgpu")

  if(EXISTS "${EMDAWN_INCLUDE_DIR}")
    file(INSTALL "${EMDAWN_INCLUDE_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
  endif()

  file(GLOB EMDAWN_JS_FILES "${EMDAWN_LIB_DIR}/*.js" "${EMDAWN_LIB_DIR}/*.json")
  if(EMDAWN_JS_FILES)
    file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/share/emdawnwebgpu")
    foreach(_f IN LISTS EMDAWN_JS_FILES)
      file(INSTALL "${_f}" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/emdawnwebgpu")
    endforeach()
  endif()

  # Also install the generated dawn/utility headers if present.
  if(EXISTS "${DAWN_BUILD_DIR}/gen/include")
    file(INSTALL "${DAWN_BUILD_DIR}/gen/include/" DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
  endif()
else()
  # Non-Emscripten: use the upstream install target.
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${DAWN_BUILD_DIR}" --target install
    RESULT_VARIABLE _result
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Dawn install step failed")
  endif()
endif()
