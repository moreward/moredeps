cmake_minimum_required(VERSION 3.25)
project(sdl3webgpu LANGUAGES C)

set(SDL3WEBGPU_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sdl3webgpu")
set(SDL3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/sdl3/include")
set(SDL3WEBGPU_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/upstream")

# On Emscripten the upstream source uses the old WebGPU string API where
# selector/label were const char*. The emdawnwebgpu headers use WGPUStringView.
# Copy the source to the build tree and patch it so the submodule stays clean.
file(MAKE_DIRECTORY "${SDL3WEBGPU_BUILD_DIR}")
file(COPY "${SDL3WEBGPU_SRC_DIR}/sdl3webgpu.c" DESTINATION "${SDL3WEBGPU_BUILD_DIR}")
file(COPY "${SDL3WEBGPU_SRC_DIR}/sdl3webgpu.h" DESTINATION "${SDL3WEBGPU_BUILD_DIR}")
if(EMSCRIPTEN)
  find_program(PATCH_EXECUTABLE patch REQUIRED)
  execute_process(
    COMMAND "${PATCH_EXECUTABLE}" -p1 -i "${CMAKE_CURRENT_SOURCE_DIR}/../../patches/sdl3webgpu_emdawn_stringview.patch"
    WORKING_DIRECTORY "${SDL3WEBGPU_BUILD_DIR}"
    RESULT_VARIABLE _patch_result
  )
  if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to patch sdl3webgpu.c for Emscripten")
  endif()
endif()

add_library(sdl3webgpu STATIC ${SDL3WEBGPU_BUILD_DIR}/sdl3webgpu.c)
target_include_directories(sdl3webgpu PUBLIC
  $<BUILD_INTERFACE:${SDL3WEBGPU_BUILD_DIR}>
  $<BUILD_INTERFACE:${SDL3_SRC_DIR}>
  $<BUILD_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(sdl3webgpu PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if(APPLE)
  target_compile_options(sdl3webgpu PRIVATE -x objective-c)
  target_link_libraries(sdl3webgpu PRIVATE
    "-framework CoreVideo"
    "-framework IOKit"
    "-framework QuartzCore"
    "-framework Cocoa"
  )
elseif(EMSCRIPTEN)
  target_compile_definitions(sdl3webgpu PRIVATE WEBGPU_BACKEND_EMDAWNWEBGPU)
endif()

install(TARGETS sdl3webgpu
  EXPORT sdl3webgpuTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(FILES ${SDL3WEBGPU_BUILD_DIR}/sdl3webgpu.h DESTINATION include)

install(EXPORT sdl3webgpuTargets
  FILE sdl3webgpuTargets.cmake
  NAMESPACE sdl3webgpu::
  DESTINATION lib/cmake/sdl3webgpu
)
