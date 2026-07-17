cmake_minimum_required(VERSION 3.25)
project(libwebsockets_wrapper LANGUAGES C)

# Copy the upstream libwebsockets source into the build tree so we can patch it
# without dirtying the submodule in deps/.
set(LWS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/libwebsockets")
set(LWS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/libwebsockets")

file(COPY "${LWS_SRC_DIR}/" DESTINATION "${LWS_BUILD_DIR}")
# The copied .git gitlink points back to the parent's submodule repo; remove it
# so git apply treats the copy as a plain directory.
file(REMOVE "${LWS_BUILD_DIR}/.git")

set(LWS_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/../../patches/libwebsockets_disable_wx.patch")
if(EXISTS "${LWS_PATCH}")
  execute_process(
    COMMAND git apply --ignore-whitespace "${LWS_PATCH}"
    WORKING_DIRECTORY "${LWS_BUILD_DIR}"
    RESULT_VARIABLE _patch_result
  )
  if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to apply libwebsockets patch ${LWS_PATCH}")
  endif()
endif()

# Point libwebsockets at the BoringSSL installed by this project. We set the
# library list here (rather than on the top-level CMAKE_ARGS) so the two paths
# survive as a single CMake list instead of being split into separate command-
# line arguments by ExternalProject.
set(LWS_OPENSSL_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
if(BUILD_SHARED_LIBS)
  if(WIN32)
    set(LWS_OPENSSL_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/ssl.lib;${CMAKE_INSTALL_PREFIX}/lib/crypto.lib")
  elseif(APPLE)
    set(LWS_OPENSSL_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/libssl.dylib;${CMAKE_INSTALL_PREFIX}/lib/libcrypto.dylib")
  else()
    set(LWS_OPENSSL_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/libssl.so;${CMAKE_INSTALL_PREFIX}/lib/libcrypto.so")
  endif()
else()
  if(WIN32)
    set(LWS_OPENSSL_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/ssl.lib;${CMAKE_INSTALL_PREFIX}/lib/crypto.lib")
  else()
    set(LWS_OPENSSL_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/libssl.a;${CMAKE_INSTALL_PREFIX}/lib/libcrypto.a")
  endif()
endif()

add_subdirectory("${LWS_BUILD_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lws_build")
