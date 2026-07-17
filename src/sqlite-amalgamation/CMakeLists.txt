cmake_minimum_required(VERSION 3.25)
project(sqlite-amalgamation LANGUAGES C)

set(SQLITE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sqlite-amalgamation")

add_library(sqlite3 ${SQLITE_SRC_DIR}/sqlite3.c)

target_compile_definitions(sqlite3 PRIVATE
  SQLITE_ENABLE_MATH_FUNCTIONS=1
  SQLITE_RECOMMENDED_OPTIONS=1
  SQLITE_OMIT_DECLTYPE=1
)

# Public API headers
set(SQLITE_PUBLIC_HEADERS
  ${SQLITE_SRC_DIR}/sqlite3.h
  ${SQLITE_SRC_DIR}/sqlite3ext.h
)

target_include_directories(sqlite3 PUBLIC
  $<BUILD_INTERFACE:${SQLITE_SRC_DIR}>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(sqlite3 PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
  WINDOWS_EXPORT_ALL_SYMBOLS ON
  PUBLIC_HEADER "${SQLITE_PUBLIC_HEADERS}"
)

install(TARGETS sqlite3
  EXPORT sqlite3Targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include
)

install(EXPORT sqlite3Targets
  FILE sqlite3Targets.cmake
  NAMESPACE sqlite3::
  DESTINATION lib/cmake/sqlite3
)
