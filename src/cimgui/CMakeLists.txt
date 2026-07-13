cmake_minimum_required(VERSION 3.25)
project(cimgui LANGUAGES CXX)

set(CIMGUI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/cimgui")

set(CIMGUI_SOURCES
  ${CIMGUI_SRC_DIR}/cimgui.cpp
  ${CIMGUI_SRC_DIR}/imgui/imgui.cpp
  ${CIMGUI_SRC_DIR}/imgui/imgui_demo.cpp
  ${CIMGUI_SRC_DIR}/imgui/imgui_draw.cpp
  ${CIMGUI_SRC_DIR}/imgui/imgui_widgets.cpp
)
# Newer ImGui versions include imgui_tables.cpp; include it if present.
if(EXISTS ${CIMGUI_SRC_DIR}/imgui/imgui_tables.cpp)
  list(APPEND CIMGUI_SOURCES ${CIMGUI_SRC_DIR}/imgui/imgui_tables.cpp)
endif()

add_library(cimgui STATIC ${CIMGUI_SOURCES})
target_include_directories(cimgui PUBLIC
  $<BUILD_INTERFACE:${CIMGUI_SRC_DIR}>
  $<BUILD_INTERFACE:${CIMGUI_SRC_DIR}/imgui>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(cimgui PUBLIC
  IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
)
if(WIN32)
  target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_API=extern "C" __declspec\(dllexport\))
else()
  target_compile_definitions(cimgui PRIVATE IMGUI_IMPL_API=extern "C")
endif()
set_target_properties(cimgui PROPERTIES
  CXX_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

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
