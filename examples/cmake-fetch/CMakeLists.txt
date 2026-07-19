# examples/cmake-fetch/CMakeLists.txt
#
# A minimal CMake example that downloads moredeps.json and selected packages
# at configure time, then adds the unpacked static libraries to the link line.
# It relies on a tiny Python helper (moredeps_fetch.py) for manifest parsing.
#
# Usage:
#   mkdir build && cd build
#   cmake .. -DMOREDEPS_RELEASE=latest -DMOREDEPS_DEPS="sokol;glfw;cglm"
#
# The deps are unpacked inside the build tree under _deps/moredeps/.

cmake_minimum_required(VERSION 3.25)
project(moredeps_consumer LANGUAGES C CXX)

set(MOREDEPS_RELEASE "latest" CACHE STRING "moredeps release tag")
set(MOREDEPS_DEPS "sokol;glfw;cglm" CACHE STRING "Semicolon-separated moredeps dependency names")
set(MOREDEPS_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/moredeps" CACHE PATH "Where to unpack moredeps")

set(FETCH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/moredeps_fetch.py")

file(MAKE_DIRECTORY "${MOREDEPS_DIR}")

# Pick the host platform for include/library paths.
if(APPLE)
    set(MOREDEPS_PLATFORM "macos_arm64")
elseif(WIN32)
    set(MOREDEPS_PLATFORM "windows_x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(MOREDEPS_PLATFORM "linux_arm64")
else()
    set(MOREDEPS_PLATFORM "linux_x64")
endif()

# Download and unpack selected deps using the Python helper.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        python3 "${FETCH_SCRIPT}"
            --release "${MOREDEPS_RELEASE}"
            --platform "${MOREDEPS_PLATFORM}"
            --deps ${MOREDEPS_DEPS}
            --output "${MOREDEPS_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

# Collect include and link directories from the unpacked tree.
file(GLOB dep_dirs RELATIVE "${MOREDEPS_DIR}" "${MOREDEPS_DIR}/*")
foreach(dep_dir IN LISTS dep_dirs)
    set(dep_include "${MOREDEPS_DIR}/${dep_dir}/static/${MOREDEPS_PLATFORM}/include")
    set(dep_lib "${MOREDEPS_DIR}/${dep_dir}/static/${MOREDEPS_PLATFORM}/lib")

    if(EXISTS "${dep_include}")
        include_directories("${dep_include}")
    endif()

    if(EXISTS "${dep_lib}")
        link_directories("${dep_lib}")
    endif()

    message(STATUS "moredeps: added ${dep_dir} (${MOREDEPS_PLATFORM})")
endforeach()

add_executable(demo main.c)
# Add actual link libraries here based on the deps you selected, e.g.:
# target_link_libraries(demo PRIVATE sokol_app_glcore sokol_gfx_glcore glfw3 cglm)
