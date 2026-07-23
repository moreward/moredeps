cmake_minimum_required(VERSION 3.25)
project(lua LANGUAGES C)

set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/lua-5.5.0/src")

set(LUA_LIB_SOURCES
  ${LUA_SRC_DIR}/lapi.c
  ${LUA_SRC_DIR}/lauxlib.c
  ${LUA_SRC_DIR}/lbaselib.c
  ${LUA_SRC_DIR}/lcode.c
  ${LUA_SRC_DIR}/lcorolib.c
  ${LUA_SRC_DIR}/lctype.c
  ${LUA_SRC_DIR}/ldblib.c
  ${LUA_SRC_DIR}/ldebug.c
  ${LUA_SRC_DIR}/ldo.c
  ${LUA_SRC_DIR}/ldump.c
  ${LUA_SRC_DIR}/lfunc.c
  ${LUA_SRC_DIR}/lgc.c
  ${LUA_SRC_DIR}/linit.c
  ${LUA_SRC_DIR}/liolib.c
  ${LUA_SRC_DIR}/llex.c
  ${LUA_SRC_DIR}/lmathlib.c
  ${LUA_SRC_DIR}/lmem.c
  ${LUA_SRC_DIR}/loadlib.c
  ${LUA_SRC_DIR}/lobject.c
  ${LUA_SRC_DIR}/lopcodes.c
  ${LUA_SRC_DIR}/loslib.c
  ${LUA_SRC_DIR}/lparser.c
  ${LUA_SRC_DIR}/lstate.c
  ${LUA_SRC_DIR}/lstring.c
  ${LUA_SRC_DIR}/lstrlib.c
  ${LUA_SRC_DIR}/ltable.c
  ${LUA_SRC_DIR}/ltablib.c
  ${LUA_SRC_DIR}/ltm.c
  ${LUA_SRC_DIR}/lundump.c
  ${LUA_SRC_DIR}/lutf8lib.c
  ${LUA_SRC_DIR}/lvm.c
  ${LUA_SRC_DIR}/lzio.c
)

# On iOS, system() is not available.  Drop loslib.c (os.execute, os.exit,
# os.clock, etc.) from the build.  Embedded/mobile Lua users typically
# don't need the os library.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_filtered_sources)
  foreach(_src IN LISTS LUA_LIB_SOURCES)
    get_filename_component(_src_name "${_src}" NAME)
    if(NOT _src_name STREQUAL "loslib.c")
      list(APPEND _filtered_sources "${_src}")
    endif()
  endforeach()
  set(LUA_LIB_SOURCES "${_filtered_sources}")
  unset(_filtered_sources)
  unset(_src_name)
endif()

add_library(lua ${LUA_LIB_SOURCES})
target_include_directories(lua PUBLIC
  $<BUILD_INTERFACE:${LUA_SRC_DIR}>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(lua PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

# Shared-library DLL export on Windows.
# LUA_BUILD_AS_DLL (public)  → consumers see __declspec(dllimport)
# LUA_CORE         (private) → building lua sees __declspec(dllexport)
if(BUILD_SHARED_LIBS AND WIN32)
  target_compile_definitions(lua PUBLIC LUA_BUILD_AS_DLL)
  target_compile_definitions(lua PRIVATE LUA_CORE)
endif()

if(APPLE)
  target_compile_definitions(lua PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
  target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
elseif(WIN32)
  target_compile_definitions(lua PRIVATE LUA_USE_WINDOWS)
endif()

install(TARGETS lua
  EXPORT luaTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES
  ${LUA_SRC_DIR}/lua.h
  ${LUA_SRC_DIR}/luaconf.h
  ${LUA_SRC_DIR}/lualib.h
  ${LUA_SRC_DIR}/lauxlib.h
  ${LUA_SRC_DIR}/lua.hpp
  DESTINATION include
)
install(EXPORT luaTargets
  FILE luaTargets.cmake
  NAMESPACE lua::
  DESTINATION lib/cmake/lua
)
