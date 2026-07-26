cmake_minimum_required(VERSION 3.25)
project(dcimgui LANGUAGES CXX)

set(DEAR_BINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/dear_bindings")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/imgui")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(PY_DEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/py_deps")
file(MAKE_DIRECTORY "${PY_DEPS_DIR}")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Dear Bindings needs the Python PLY package.  Make sure it is available for the
# exact interpreter CMake selected, without relying on a system/site install that
# may be missing or externally-managed (e.g. Homebrew Python on macOS).
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -c "import ply"
  RESULT_VARIABLE _PLY_AVAILABLE
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT _PLY_AVAILABLE EQUAL 0)
  message(STATUS "PLY not found for ${Python3_EXECUTABLE}; installing locally to ${PY_DEPS_DIR}")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -m pip install --quiet --target "${PY_DEPS_DIR}" ply
    RESULT_VARIABLE _PLY_INSTALL_RC
  )
  if(NOT _PLY_INSTALL_RC EQUAL 0)
    message(FATAL_ERROR "Failed to install Python PLY package for ${Python3_EXECUTABLE}. "
      "Install it manually (e.g. 'python3 -m pip install --user ply' or 'pip install --target <dir> ply').")
  endif()
endif()

# Preserve any existing PYTHONPATH and prepend the local PLY directory.
if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
  set(_PYTHONPATH_VALUE "${PY_DEPS_DIR}${CMAKE_HOST_PATH_SEP}$ENV{PYTHONPATH}")
else()
  set(_PYTHONPATH_VALUE "${PY_DEPS_DIR}")
endif()

set(DCIMGUI_H "${GENERATED_DIR}/dcimgui.h")
set(DCIMGUI_CPP "${GENERATED_DIR}/dcimgui.cpp")
set(DCIMGUI_JSON "${GENERATED_DIR}/dcimgui.json")

add_custom_command(
  OUTPUT "${DCIMGUI_H}" "${DCIMGUI_CPP}" "${DCIMGUI_JSON}"
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${_PYTHONPATH_VALUE}"
          "${Python3_EXECUTABLE}" "${DEAR_BINDINGS_DIR}/dear_bindings.py"
          -o "${GENERATED_DIR}/dcimgui"
          "${IMGUI_DIR}/imgui.h"
  DEPENDS "${IMGUI_DIR}/imgui.h"
          "${DEAR_BINDINGS_DIR}/dear_bindings.py"
  COMMENT "Generating Dear Bindings C API for Dear ImGui"
)

set(DCIMGUI_SOURCES
  "${DCIMGUI_CPP}"
  "${IMGUI_DIR}/imgui.cpp"
  "${IMGUI_DIR}/imgui_demo.cpp"
  "${IMGUI_DIR}/imgui_draw.cpp"
  "${IMGUI_DIR}/imgui_tables.cpp"
  "${IMGUI_DIR}/imgui_widgets.cpp"
)

add_library(dcimgui ${DCIMGUI_SOURCES})
target_include_directories(dcimgui PUBLIC
  $<BUILD_INTERFACE:${GENERATED_DIR}>
  $<BUILD_INTERFACE:${IMGUI_DIR}>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(dcimgui PROPERTIES
  CXX_STANDARD 17
  POSITION_INDEPENDENT_CODE ON
)
if(WIN32)
  set_target_properties(dcimgui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

install(TARGETS dcimgui
  EXPORT dcimguiTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES
  "${DCIMGUI_H}"
  "${DCIMGUI_JSON}"
  "${IMGUI_DIR}/imconfig.h"
  DESTINATION include
)

install(EXPORT dcimguiTargets
  FILE dcimguiTargets.cmake
  NAMESPACE dcimgui::
  DESTINATION lib/cmake/dcimgui
)
