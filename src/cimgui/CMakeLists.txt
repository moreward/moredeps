cmake_minimum_required(VERSION 3.25)
project(cimgui LANGUAGES CXX)

set(CIMGUI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/cimgui")
set(IMGUI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/imgui")

set(CIMGUI_SOURCES
  ${CIMGUI_SRC_DIR}/cimgui.cpp
  ${IMGUI_SRC_DIR}/imgui.cpp
  ${IMGUI_SRC_DIR}/imgui_demo.cpp
  ${IMGUI_SRC_DIR}/imgui_draw.cpp
  ${IMGUI_SRC_DIR}/imgui_widgets.cpp
)
# Newer ImGui versions include imgui_tables.cpp; include it if present.
if(EXISTS ${IMGUI_SRC_DIR}/imgui_tables.cpp)
  list(APPEND CIMGUI_SOURCES ${IMGUI_SRC_DIR}/imgui_tables.cpp)
endif()

add_library(cimgui ${CIMGUI_SOURCES})
target_include_directories(cimgui PUBLIC
  $<BUILD_INTERFACE:${CIMGUI_SRC_DIR}>
  $<BUILD_INTERFACE:${IMGUI_SRC_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(cimgui PUBLIC
  IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
)
if(WIN32)
  # cimgui.h defaults to __declspec(dllexport) on Windows. For a static library
  # this is unnecessary and the multi-token IMGUI_IMPL_API definition does not
  # survive CMake's compile-definition handling, so keep the macros simple.
  
  target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_API=)
else()
  target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_API=extern "C")
endif()
set_target_properties(cimgui PROPERTIES
  CXX_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)
set_target_properties(cimgui PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(WIN32 AND BUILD_SHARED_LIBS)
  # cimgui.h defaults to __declspec(dllexport) on Windows. For consumers of the
  # shared library we want the declarations to be plain extern so they link
  # against the import library; WINDOWS_EXPORT_ALL_SYMBOLS handles the actual
  # exports from the DLL itself.
  target_compile_definitions(cimgui PUBLIC CIMGUI_NO_EXPORT)
endif()

install(TARGETS cimgui
  EXPORT cimguiTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES
  ${CIMGUI_SRC_DIR}/cimgui.h
  DESTINATION include
)

install(EXPORT cimguiTargets
  FILE cimguiTargets.cmake
  NAMESPACE cimgui::
  DESTINATION lib/cmake/cimgui
)
