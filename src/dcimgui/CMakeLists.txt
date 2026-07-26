cmake_minimum_required(VERSION 3.25)
project(dcimgui LANGUAGES CXX)

set(DEAR_BINDINGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/dear_bindings")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/imgui")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DCIMGUI_H "${GENERATED_DIR}/dcimgui.h")
set(DCIMGUI_CPP "${GENERATED_DIR}/dcimgui.cpp")
set(DCIMGUI_JSON "${GENERATED_DIR}/dcimgui.json")

add_custom_command(
  OUTPUT "${DCIMGUI_H}" "${DCIMGUI_CPP}" "${DCIMGUI_JSON}"
  COMMAND "${Python3_EXECUTABLE}" "${DEAR_BINDINGS_DIR}/dear_bindings.py"
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
  DESTINATION include
)

install(EXPORT dcimguiTargets
  FILE dcimguiTargets.cmake
  NAMESPACE dcimgui::
  DESTINATION lib/cmake/dcimgui
)
