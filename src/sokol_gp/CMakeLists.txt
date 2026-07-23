cmake_minimum_required(VERSION 3.25)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(sokol_gp LANGUAGES C OBJC)
else()
  project(sokol_gp LANGUAGES C)
endif()

set(SOKOL_GP_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol_gp/thirdparty")
set(SOKOL_GP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sokol_gp")

# sokol_gp vendors a specific snapshot of the sokol headers in
# deps/sokol_gp/thirdparty. We build sokol_gp against those vendored headers so
# it remains compatible with the upstream sokol_gp code, even when the top-level
# deps/sokol submodule is newer. The vendored headers are installed into
# include/sokol_gp/ so that sokol_gp.h includes them from its own directory.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sokol_gp_compat")
file(COPY ${SOKOL_GP_VENDOR_DIR}/ DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sokol_gp_compat")
file(COPY "${SOKOL_GP_SRC_DIR}/sokol_gp.h" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sokol_gp_compat")

add_library(sokol_gp sokol_gp.c)
target_include_directories(sokol_gp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/sokol_gp_compat>
  $<BUILD_INTERFACE:${SOKOL_GP_SRC_DIR}>
  $<INSTALL_INTERFACE:include/sokol_gp>
)
set_target_properties(sokol_gp PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if(APPLE AND (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  target_compile_options(sokol_gp PRIVATE -x objective-c)
  target_link_libraries(sokol_gp PRIVATE
    "-framework AppKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework Cocoa"
    "-framework QuartzCore"
  )
endif()

install(TARGETS sokol_gp
  EXPORT sokol_gpTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sokol_gp_compat/"
  DESTINATION include/sokol_gp
  FILES_MATCHING PATTERN "*.h"
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../src/sokol_variant/sokol_gp_compat_note.txt"
  DESTINATION include/sokol_gp
  RENAME README.txt
)

install(EXPORT sokol_gpTargets
  FILE sokol_gpTargets.cmake
  NAMESPACE sokol_gp::
  DESTINATION lib/cmake/sokol_gp
)
