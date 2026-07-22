# Top-level CMake driver for the moredeps repository.
# This file uses CMake's ExternalProject module to build each dependency in
# isolation, with the correct options, and install all artifacts into a single
# prefix under _out/<platform>.
#
# Usage:
#   cmake -S . -B _b/macos_arm64 -DCMAKE_TOOLCHAIN_FILE=toolchain/macos_arm64.cmake
#   cmake --build _b/macos_arm64
#
# Cross-compilation is handled by the toolchain files in toolchain/.

cmake_minimum_required(VERSION 3.25)
project(moredeps LANGUAGES C CXX)

include(ExternalProject)

# -----------------------------------------------------------------------------
# Global settings
# -----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Respect the build type if the caller provided one; otherwise default to Release.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# Platform detection
# -----------------------------------------------------------------------------
# Toolchain files in toolchain/ set MOREDEPS_PLATFORM. If it is not set (e.g.
# the caller did not use a toolchain file), try to derive a sensible default.
if(NOT DEFINED MOREDEPS_PLATFORM)
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" _sys_name)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _sys_proc)
  if(_sys_name STREQUAL "darwin")
    set(MOREDEPS_PLATFORM "macos_${_sys_proc}")
  elseif(_sys_name STREQUAL "linux")
    if(_sys_proc MATCHES "aarch64|arm64")
      set(MOREDEPS_PLATFORM "linux_arm64")
    else()
      set(MOREDEPS_PLATFORM "linux_x64")
    endif()
  elseif(_sys_name STREQUAL "windows")
    if(_sys_proc MATCHES "aarch64|arm64")
      set(MOREDEPS_PLATFORM "windows_arm64")
    else()
      set(MOREDEPS_PLATFORM "windows_x64")
    endif()
  elseif(_sys_name STREQUAL "emscripten")
    set(MOREDEPS_PLATFORM "wasm_emscripten")
  else()
    message(FATAL_ERROR "Unable to determine MOREDEPS_PLATFORM for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  unset(_sys_name)
  unset(_sys_proc)
endif()
set(MOREDEPS_PLATFORM "${MOREDEPS_PLATFORM}" CACHE STRING "moredeps target platform" FORCE)
message(STATUS "moredeps platform: ${MOREDEPS_PLATFORM}")

# -----------------------------------------------------------------------------
# Install / output prefix
# -----------------------------------------------------------------------------
# All dependency installs are staged into _out/<platform>. The build trees live
# in _b/<platform>/<dep> (the user passed -B _b/<platform>).
if(NOT CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/_out/${MOREDEPS_PLATFORM}" CACHE PATH "Install prefix" FORCE)
endif()
message(STATUS "moredeps install prefix: ${CMAKE_INSTALL_PREFIX}")

file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")

# -----------------------------------------------------------------------------
# Common CMake arguments passed to every dependency
# -----------------------------------------------------------------------------
set(MOREDEPS_COMMON_CMAKE_ARGS
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}"
  "-DBUILD_SHARED_LIBS=OFF"
  "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
  "-DCMAKE_C_STANDARD=11"
  "-DCMAKE_CXX_STANDARD=17"
  "-DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}"
  "-DCMAKE_FIND_ROOT_PATH=${CMAKE_INSTALL_PREFIX}"
  "-DCMAKE_FIND_NO_INSTALL_PREFIX=ON"
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
)

if(CMAKE_TOOLCHAIN_FILE)
  get_filename_component(_toolchain_abs "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE)
  list(APPEND MOREDEPS_COMMON_CMAKE_ARGS
    "-DCMAKE_TOOLCHAIN_FILE=${_toolchain_abs}"
  )
  unset(_toolchain_abs)
endif()

if(CMAKE_C_COMPILER)
  list(APPEND MOREDEPS_COMMON_CMAKE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
endif()
if(CMAKE_CXX_COMPILER)
  list(APPEND MOREDEPS_COMMON_CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
endif()

# Propagate compiler launchers (ccache/sccache) so sub-builds are cached too.
# Using launchers instead of shim directories keeps CMAKE_C_COMPILER pointing
# at the real compiler, which some configure scripts (e.g. TinyCC) require.
if(CMAKE_C_COMPILER_LAUNCHER)
  list(APPEND MOREDEPS_COMMON_CMAKE_ARGS "-DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}")
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER)
  list(APPEND MOREDEPS_COMMON_CMAKE_ARGS "-DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}")
endif()

# -----------------------------------------------------------------------------
# Helper for adding dependency subprojects
# -----------------------------------------------------------------------------
function(moredeps_add_dep name)
  set(options STATIC_ONLY)
  set(oneValueArgs SOURCE_DIR)
  set(multiValueArgs CMAKE_ARGS SHARED_CMAKE_ARGS DEPENDS BUILD_BYPRODUCTS INSTALL_COMMAND PATCH_COMMAND)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT ARG_SOURCE_DIR)
    set(ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/${name}")
  endif()

  if(ARG_INSTALL_COMMAND)
    set(INSTALL_COMMAND_ARG INSTALL_COMMAND ${ARG_INSTALL_COMMAND})
  endif()
  if(ARG_PATCH_COMMAND)
    set(PATCH_COMMAND_ARG PATCH_COMMAND ${ARG_PATCH_COMMAND})
  endif()

  # Static variant
  if(NOT MOREDEPS_BUILD_SHARED)
    ExternalProject_Add(
      ${name}
      SOURCE_DIR "${ARG_SOURCE_DIR}"
      BINARY_DIR "${CMAKE_BINARY_DIR}/${name}"
      INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
      CMAKE_ARGS
        ${MOREDEPS_COMMON_CMAKE_ARGS}
        ${ARG_CMAKE_ARGS}
      ${PATCH_COMMAND_ARG}
      ${INSTALL_COMMAND_ARG}
      DEPENDS ${ARG_DEPENDS}
      BUILD_BYPRODUCTS ${ARG_BUILD_BYPRODUCTS}
    )
  endif()

  # Shared variant -- auto-created for every dep that isn't STATIC_ONLY.
  # Transforms dependency chain: curl -> boringssl becomes
  # curl_shared -> boringssl_shared. 
  if(MOREDEPS_BUILD_SHARED AND NOT ARG_STATIC_ONLY)
    set(_shared_deps)
    foreach(_d IN LISTS ARG_DEPENDS)
      list(APPEND _shared_deps "${_d}_shared")
    endforeach()

    ExternalProject_Add(
      ${name}_shared
      SOURCE_DIR "${ARG_SOURCE_DIR}"
      BINARY_DIR "${CMAKE_BINARY_DIR}/${name}_shared"
      INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
      CMAKE_ARGS
        ${MOREDEPS_COMMON_CMAKE_ARGS}
        ${ARG_CMAKE_ARGS}
        "-DBUILD_SHARED_LIBS=ON"
        ${ARG_SHARED_CMAKE_ARGS}
      ${PATCH_COMMAND_ARG}
      ${INSTALL_COMMAND_ARG}
      DEPENDS ${_shared_deps}
    )
  endif()
endfunction()

# Helper for generating Sokol backend variants.
# Usage: moredeps_sokol_variant(gfx GLCORE)
# Generates a small wrapper source dir in the build tree and calls
# moredeps_add_dep for the variant library (e.g. sokol_gfx_glcore).
function(moredeps_sokol_variant component backend)
  string(TOLOWER "${backend}" _backend_lower)
  set(_variant_name "sokol_${component}_${_backend_lower}")
  set(_variant_src_dir "${CMAKE_CURRENT_BINARY_DIR}/sokol_variants/${_variant_name}")
  file(MAKE_DIRECTORY "${_variant_src_dir}")

  if(component STREQUAL "gp")
    set(SOKOL_GP_IMPL_DEFINE "#define SOKOL_GP_IMPL")
    set(SOKOL_COMPONENT_INCLUDE "#include \"sokol_gfx.h\"\n#include \"sokol_gp.h\"")
    # sokol_gp is built against the vendored sokol headers shipped in
    # deps/sokol_gp/thirdparty. Copy them into the build tree so the variant
    # is isolated from the top-level deps/sokol submodule version.
    set(SOKOL_SRC_DIR "${_variant_src_dir}/sokol_gp_compat")
    set(SOKOL_GP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sokol_gp")
    file(MAKE_DIRECTORY "${SOKOL_SRC_DIR}")
    file(COPY "${SOKOL_GP_SRC_DIR}/thirdparty/" DESTINATION "${SOKOL_SRC_DIR}")
    file(COPY "${SOKOL_GP_SRC_DIR}/sokol_gp.h" DESTINATION "${SOKOL_SRC_DIR}")
    set(SOKOL_GP_COMPAT_NOTE "${CMAKE_CURRENT_SOURCE_DIR}/src/sokol_variant/sokol_gp_compat_note.txt")
    set(_template_name "CMakeLists.gp.in")
  else()
    set(SOKOL_GP_IMPL_DEFINE "")
    if(component STREQUAL "glue")
      set(SOKOL_COMPONENT_INCLUDE "#include \"sokol_gfx.h\"\n#include \"sokol_app.h\"\n#include \"sokol_glue.h\"")
    else()
      set(SOKOL_COMPONENT_INCLUDE "#include \"sokol_${component}.h\"")
    endif()
    set(SOKOL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sokol")
    set(SOKOL_GP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sokol_gp")
    set(_template_name "CMakeLists.txt.in")
  endif()

  set(SOKOL_VARIANT_NAME "${_variant_name}")
  set(SOKOL_COMPONENT "${component}")
  set(SOKOL_BACKEND "${backend}")
  set(SOKOL_BACKEND_DEFINE "SOKOL_${backend}")
  if(APPLE AND backend STREQUAL "METAL")
    set(SOKOL_OBJC "OBJC")
  else()
    set(SOKOL_OBJC "")
  endif()

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sokol_variant/sokol_impl.c.in"
    "${_variant_src_dir}/sokol_impl.c"
    @ONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sokol_variant/${_template_name}"
    "${_variant_src_dir}/CMakeLists.txt"
    @ONLY
  )

  set(_depends "")
  if(backend STREQUAL "WGPU")
    set(_depends "dawn")
  endif()

  moredeps_add_dep(${_variant_name}
    SOURCE_DIR "${_variant_src_dir}"
    DEPENDS ${_depends}
  )
endfunction()

# -----------------------------------------------------------------------------
# Dependency definitions
# -----------------------------------------------------------------------------

# --- boringssl ---
# Single TLS backend for curl and libwebsockets. Installs OpenSSL::Crypto/
# OpenSSL::SSL targets into the common prefix.
# BoringSSL's assembly for Windows is not supported by the CMake ASM toolchain
# (NASM is required on x64, and ARM64 is not supported at all). Disable ASM on
# all Windows platforms. Emscripten also disables it.
if(EMSCRIPTEN OR WIN32)
  set(BORINGSSL_NO_ASM ON)
else()
  set(BORINGSSL_NO_ASM OFF)
endif()

moredeps_add_dep(boringssl
  CMAKE_ARGS
    "-DBUILD_TESTING=OFF"
    "-DFIPS=OFF"
    "-DOPENSSL_NO_ASM=${BORINGSSL_NO_ASM}"
)

# --- box3d ---
moredeps_add_dep(box3d
  CMAKE_ARGS
    "-DBOX3D_SAMPLES=OFF"
    "-DBOX3D_BENCHMARKS=OFF"
    "-DBOX3D_UNIT_TESTS=OFF"
    "-DBOX3D_DOCS=OFF"
    "-DBOX3D_BUILD_SHADERS=OFF"
    "-DBOX3D_PROFILE=OFF"
    "-DBOX3D_DISABLE_SIMD=OFF"
    "-DBOX3D_DOUBLE_PRECISION=OFF"
)

# --- cglm ---
moredeps_add_dep(cglm
  CMAKE_ARGS
    "-DCGLM_STATIC=ON"
    "-DCGLM_SHARED=OFF"
    "-DCGLM_USE_TEST=OFF"
    "-DCGLM_USE_C99=OFF"
  SHARED_CMAKE_ARGS
    "-DCGLM_STATIC=OFF"
    "-DCGLM_SHARED=ON"
)

# --- cimgui ---
# Upstream CMakeLists hard-codes SHARED. We wrap it to build a static lib.
moredeps_add_dep(cimgui
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cimgui"
)

# --- budouxc ---
# Upstream CMakeLists includes GNUInstallDirs after add_subdirectory(src), so
# CMAKE_INSTALL_LIBDIR is empty during install. We wrap it to avoid patching.
moredeps_add_dep(budouxc
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/budouxc"
)

# --- cJSON ---
# cJSON adds /Za on MSVC, which conflicts with the /std:c11 we pass globally.
if(WIN32)
  set(_CJSON_EXTRA_ARGS "-DENABLE_CUSTOM_COMPILER_FLAGS=OFF")
else()
  set(_CJSON_EXTRA_ARGS "")
endif()
moredeps_add_dep(cJSON
  CMAKE_ARGS
    "-DENABLE_CJSON_TEST=OFF"
    "-DENABLE_CJSON_UNINSTALL=OFF"
    "-DENABLE_CJSON_UTILS=OFF"
    "-DENABLE_LOCALES=ON"
    ${_CJSON_EXTRA_ARGS}
)

# --- curl ---
# Uses BoringSSL via the OpenSSL::SSL / OpenSSL::Crypto targets that BoringSSL
# installs into the common prefix.
moredeps_add_dep(curl
  DEPENDS boringssl
  CMAKE_ARGS
    "-DBUILD_CURL_EXE=OFF"
    "-DBUILD_STATIC_LIBS=ON"
    "-DBUILD_TESTING=OFF"
    "-DBUILD_EXAMPLES=OFF"
    "-DBUILD_LIBCURL_DOCS=OFF"
    "-DBUILD_MISC_DOCS=OFF"
    "-DENABLE_CURL_MANUAL=OFF"
    "-DCURL_ENABLE_SSL=ON"
    "-DCURL_USE_OPENSSL=ON"
    "-DCURL_DISABLE_LDAP=ON"
    "-DUSE_NGHTTP2=OFF"
    "-DUSE_NGTCP2=OFF"
    "-DUSE_QUICHE=OFF"
    "-DCURL_USE_LIBPSL=OFF"
    "-DCURL_USE_LIBIDN2=OFF"
    "-DCURL_USE_LIBSSH2=OFF"
    "-DCURL_USE_LIBSSH=OFF"
    "-DCURL_USE_GSSAPI=OFF"
    "-DCURL_USE_GSASL=OFF"
    "-DCURL_USE_LIBBACKTRACE=OFF"
    "-DCURL_USE_LIBUV=OFF"
    "-DCURL_BROTLI=OFF"
    "-DCURL_ZSTD=OFF"
    "-DCURL_CA_NATIVE=OFF"
    "-DCURL_CA_FALLBACK=OFF"
    "-DCURL_CA_BUNDLE=none"
    "-DCURL_CA_PATH=none"
    "-DENABLE_IPV6=ON"
    "-DENABLE_UNIX_SOCKETS=ON"
    "-DCURL_DISABLE_ALTSVC=ON"
    "-DCURL_DISABLE_SRP=ON"
    "-DCURL_DISABLE_COOKIES=OFF"
    "-DCURL_DISABLE_BASIC_AUTH=OFF"
    "-DCURL_DISABLE_BEARER_AUTH=OFF"
    "-DCURL_DISABLE_DIGEST_AUTH=OFF"
    "-DCURL_DISABLE_KERBEROS_AUTH=ON"
    "-DCURL_DISABLE_NEGOTIATE_AUTH=ON"
    "-DCURL_DISABLE_AWS=OFF"
    "-DCURL_DISABLE_DOH=OFF"
    "-DCURL_DISABLE_HSTS=OFF"
    "-DCURL_DISABLE_HTTP_AUTH=OFF"
    "-DCURL_DISABLE_IPFS=OFF"
    "-DCURL_DISABLE_MQTT=ON"
    "-DCURL_DISABLE_NTLM=ON"
    "-DCURL_DISABLE_PROXY=OFF"
    "-DCURL_DISABLE_RTSP=OFF"
    "-DCURL_DISABLE_SHUFFLE_DNS=OFF"
    "-DCURL_DISABLE_SMTP=OFF"
    "-DCURL_DISABLE_TELNET=ON"
    "-DCURL_DISABLE_TFTP=ON"
    "-DCURL_DISABLE_WEBSOCKETS=OFF"
  SHARED_CMAKE_ARGS
    "-DBUILD_STATIC_LIBS=OFF"
)

# --- dawn ---
# WebGPU-only build. Emscripten uses emdawnwebgpu (handled via dawn's own
# Emscripten options). Dependencies are pre-populated as git submodules in
# deps/dawn_third_party (they cannot be nested inside the deps/dawn submodule).
moredeps_add_dep(dawn
  PATCH_COMMAND
    ${CMAKE_COMMAND}
    -DSOURCE_DIR=<SOURCE_DIR>
    -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/dawn_export_deps.patch
    -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply_patch.cmake
  CMAKE_ARGS
    "-DDAWN_THIRD_PARTY_DIR=${CMAKE_CURRENT_SOURCE_DIR}/deps/dawn_third_party"
    "-DDAWN_EMDAWNWEBGPU_DIR=${CMAKE_CURRENT_SOURCE_DIR}/deps/dawn/third_party/emdawnwebgpu"
    "-DDAWN_ENABLE_INSTALL=$<IF:$<PLATFORM_ID:Emscripten>,OFF,ON>"
    "-DDAWN_BUILD_TESTS=OFF"
    "-DDAWN_BUILD_BENCHMARKS=OFF"
    "-DDAWN_BUILD_FUZZERS=OFF"
    "-DDAWN_BUILD_NODE_BINDINGS=OFF"
    "-DDAWN_BUILD_PROTOBUF=OFF"
    "-DDAWN_BUILD_SAMPLES=OFF"
    "-DDAWN_ENABLE_SWIFTSHADER=OFF"
    "-DDAWN_FETCH_DEPENDENCIES=OFF"
    "-DDAWN_USE_GLFW=OFF"
    "-DTINT_BUILD_TESTS=OFF"
    "-DTINT_BUILD_CMD_TOOLS=OFF"
    "-DTINT_BUILD_GLSL_VALIDATOR=OFF"
    "-DTINT_BUILD_WGSL_READER=ON"
    "-DTINT_BUILD_WGSL_WRITER=ON"
    "-DDAWN_BUILD_MONOLITHIC_LIBRARY=$<IF:$<PLATFORM_ID:Emscripten>,OFF,STATIC>"
    "-DDAWN_ENABLE_VULKAN=$<IF:$<PLATFORM_ID:Linux>,ON,OFF>"
    "-DDAWN_ENABLE_METAL=$<IF:$<PLATFORM_ID:Darwin>,ON,OFF>"
    "-DDAWN_USE_WAYLAND=OFF"
    "-DDAWN_ENABLE_DESKTOP_GL=OFF"
    "-DDAWN_ENABLE_OPENGLES=OFF"
  SHARED_CMAKE_ARGS
    "-DDAWN_BUILD_MONOLITHIC_LIBRARY=SHARED"
    "-DBUILD_SHARED_LIBS=OFF"
  INSTALL_COMMAND
    ${CMAKE_COMMAND} -DDAWN_BUILD_DIR=<BINARY_DIR>
                     -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                     -DEMSCRIPTEN=$<BOOL:$<PLATFORM_ID:Emscripten>>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_dawn.cmake
)

# --- enet ---
# UDP networking; not usable in a browser.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(enet
    CMAKE_ARGS
      "-DENET_STATIC=ON"
      "-DENET_SHARED=OFF"
      "-DENET_TEST=OFF"
      "-DENET_USE_MORE_PEERS=OFF"
      "-DENET_IPV4_ONLY=OFF"
    SHARED_CMAKE_ARGS
      "-DENET_STATIC=OFF"
      "-DENET_SHARED=ON"
  )
endif()

# --- flecs ---
moredeps_add_dep(flecs
  CMAKE_ARGS
    "-DFLECS_STATIC=ON"
    "-DFLECS_SHARED=OFF"
    "-DFLECS_TESTS=OFF"
    "-DFLECS_PIC=ON"
  SHARED_CMAKE_ARGS
    "-DFLECS_STATIC=OFF"
    "-DFLECS_SHARED=ON"
)

# --- freetype ---
# Keep PNG/Bzip2/Brotli disabled to reduce dependencies. HarfBuzz integration
# is enabled; FreeType is built before HarfBuzz.
moredeps_add_dep(freetype
  CMAKE_ARGS
    "-DFT_DISABLE_ZLIB=OFF"
    "-DFT_DISABLE_BZIP2=ON"
    "-DFT_DISABLE_PNG=ON"
    "-DFT_DISABLE_HARFBUZZ=OFF"
    "-DFT_DISABLE_BROTLI=ON"
    "-DFT_ENABLE_ERROR_STRINGS=ON"
)

# --- ggml ---
# Tensor library for machine learning inference.  CPU-only build with
# BLAS acceleration where available (Accelerate on macOS, OpenBLAS elsewhere).
# Supports Emscripten via WASM SIMD.
# ~2MB static lib, 290K LOC, MIT.
moredeps_add_dep(ggml
  PATCH_COMMAND
    ${CMAKE_COMMAND}
    -DSOURCE_DIR=<SOURCE_DIR>
    -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/ggml-math-lib.patch
    -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply_patch.cmake
  CMAKE_ARGS
    "-DGGML_STANDALONE=ON"
    "-DGGML_NATIVE=OFF"
    "-DGGML_CUDA=OFF"
    "-DGGML_METAL=OFF"
    "-DGGML_VULKAN=OFF"
    "-DGGML_OPENCL=OFF"
    "-DGGML_SYCL=OFF"
    "-DGGML_BACKEND_DL=OFF"
    "-DGGML_BUILD_TESTS=OFF"
    "-DGGML_BUILD_EXAMPLES=OFF"
  SHARED_CMAKE_ARGS
    "-DGGML_STANDALONE=ON"
    "-DGGML_NATIVE=OFF"
    "-DGGML_CUDA=OFF"
    "-DGGML_METAL=OFF"
    "-DGGML_VULKAN=OFF"
    "-DGGML_OPENCL=OFF"
    "-DGGML_SYCL=OFF"
    "-DGGML_BACKEND_DL=OFF"
    "-DGGML_BUILD_TESTS=OFF"
    "-DGGML_BUILD_EXAMPLES=OFF"
  )

# --- glfw ---
# GLFW is a desktop windowing library; it does not build for Emscripten.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(glfw
    CMAKE_ARGS
      "-DBUILD_SHARED_LIBS=OFF"
      "-DGLFW_BUILD_DOCS=OFF"
      "-DGLFW_INSTALL=ON"
      "-DGLFW_BUILD_WAYLAND=OFF"
  )
endif()

# --- harfbuzz ---
moredeps_add_dep(harfbuzz
  DEPENDS freetype
  CMAKE_ARGS
    "-DHB_HAVE_CAIRO=OFF"
    "-DHB_HAVE_FREETYPE=ON"
    "-DHB_HAVE_GRAPHITE2=OFF"
    "-DHB_HAVE_GLIB=OFF"
    "-DHB_HAVE_ICU=OFF"
    "-DHB_BUILD_UTILS=OFF"
    "-DHB_BUILD_SUBSET=ON"
    "-DHB_HAVE_GOBJECT=OFF"
    "-DHB_HAVE_INTROSPECTION=OFF"
)

# --- libwebsockets ---
# Uses BoringSSL from the common prefix. BSD sockets are not available in a browser.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(libwebsockets
    DEPENDS boringssl
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libwebsockets"
    CMAKE_ARGS
      "-DLWS_WITH_STATIC=ON"
      "-DLWS_WITH_SHARED=OFF"
      "-DLWS_WITHOUT_TESTAPPS=ON"
      "-DLWS_WITHOUT_TEST_SERVER=ON"
      "-DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON"
      "-DLWS_WITHOUT_TEST_PING=ON"
      "-DLWS_WITHOUT_TEST_CLIENT=ON"
      "-DLWS_WITH_MINIMAL_EXAMPLES=OFF"
      "-DLWS_WITH_SSL=ON"
      "-DLWS_WITH_BORINGSSL=ON"
      "-DLWS_OPENSSL_INCLUDE_DIRS=${CMAKE_INSTALL_PREFIX}/include"
      "-DDISABLE_WERROR=ON"
      "-DLWS_HAVE_RSA_SET0_KEY=ON"
      "-DLWS_HAVE_ECDSA_SIG_get0=ON"
      "-DLWS_HAVE_ECDSA_SIG_set0=ON"
      "-DLWS_HAVE_BN_bn2binpad=ON"
      "-DLWS_WITH_MBEDTLS=OFF"
      "-DLWS_WITH_WOLFSSL=OFF"
      "-DLWS_WITH_ZLIB=OFF"
      "-DLWS_WITH_LIBUV=OFF"
      "-DLWS_WITH_LIBEV=OFF"
      "-DLWS_WITH_LIBEVENT=OFF"
      "-DLWS_WITH_GL=OFF"
      "-DLWS_WITH_SMTP=OFF"
      "-DLWS_WITH_SPAWN=OFF"
      "-DLWS_WITH_FSMOUNT=OFF"
      "-DLWS_WITH_OTA=OFF"
      "-DLWS_WITH_LEJP=OFF"
      "-DLWS_WITH_LEJP_CONF=OFF"
      "-DLWS_WITH_LHP=OFF"
      "-DLWS_WITH_JSONRPC=OFF"
      "-DLWS_WITH_CONMON=OFF"
      "-DLWS_WITH_WOL=OFF"
      "-DLWS_WITH_CACHE_NSCOOKIEJAR=OFF"
      "-DLWS_WITH_NETLINK=OFF"
      "-DLWS_WITH_BINDTODEVICE=OFF"
      "-DLWS_WITH_LIBCAP=OFF"
      "-DLWS_WITH_SECURE_STREAMS=OFF"
      "-DLWS_WITH_HTTP_STREAM_COMPRESSION=OFF"
      "-DLWS_WITH_HTTP_BROTLI=OFF"
      "-DLWS_WITH_JOSE=OFF"
      "-DLWS_WITH_COSE=OFF"
      "-DLWS_WITH_GENCRYPTO=OFF"
      "-DLWS_WITH_SQLITE3=OFF"
      "-DLWS_WITH_STRUCT_JSON=OFF"
      "-DLWS_WITH_STRUCT_SQLITE3=OFF"
      "-DLWS_WITH_DRIVERS=OFF"
      "-DLWS_WITH_CBOR=OFF"
      "-DLWS_WITH_ALLOC_METADATA_LWS=OFF"
      "-DLWS_WITH_COMPRESSED_BACKTRACES=OFF"
      "-DLWS_WITH_DISKCACHE=OFF"
      "-DLWS_WITH_LWS_DSH=OFF"
      "-DLWS_WITH_PLUGINS_API=OFF"
      "-DLWS_WITH_EVLIB_PLUGINS=OFF"
      "-DLWS_WITH_GCOV=OFF"
      "-DLWS_WITH_ASAN=OFF"
      "-DLWS_WITH_FANALYZER=OFF"
      "-DLWS_WITH_NO_LOGS=OFF"
      "-DLWS_WITH_UDP=ON"
      "-DLWS_CLIENT_HTTP_PROXYING=ON"
      "-DLWS_WITH_FILE_OPS=ON"
      "-DLWS_WITH_CUSTOM_HEADERS=ON"
      "-DLWS_WITH_LWSAC=ON"
      "-DLWS_WITH_EXTERNAL_POLL=OFF"
      "-DLWS_WITH_TLS_JIT_TRUST=OFF"
      "-DLWS_WITH_SELFTESTS=OFF"
      "-DLWS_WITH_SYS_FAULT_INJECTION=OFF"
      "-DLWS_WITH_SYS_METRICS=OFF"
      "-DLWS_WITH_SYS_STATE=ON"
      "-DLWS_WITH_SYS_SMD=ON"
      "-DLWS_WITH_UPNG=ON"
      "-DLWS_WITH_GZINFLATE=ON"
      "-DLWS_WITH_JPEG=ON"
      "-DLWS_WITH_DLO=ON"
      "-DLWS_HTTP_HEADERS_ALL=OFF"
      "-DLWS_WITH_SUL_DEBUGGING=OFF"
      "-DLWS_WITH_WAKE_LOGGING=OFF"
      "-DLWS_WITH_MCUFONT_ENCODER=OFF"
      "-DLWS_WITH_COMPRESSED_BACKTRACES=OFF"
      "-DLWS_MSVC_STATIC_RUNTIME=OFF"
      "-DLWS_REPRODUCIBLE=ON"
      "-DLWS_WITH_EXPORT_LWSTARGETS=ON"
      "-DLWS_WITHOUT_EXTENSIONS=ON"
      "-DLWS_WITHOUT_DAEMONIZE=ON"
      "-DLWS_WITHOUT_BUILTIN_SHA1=OFF"
      "-DLWS_FALLBACK_GETHOSTBYNAME=OFF"
      "-DLWS_AVOID_SIGPIPE_IGN=OFF"
      "-DLWS_WITH_NETWORK=ON"
      "-DLWS_ROLE_H1=ON"
      "-DLWS_ROLE_WS=ON"
      "-DLWS_ROLE_MQTT=OFF"
      "-DLWS_ROLE_DBUS=OFF"
      "-DLWS_ROLE_RAW_PROXY=OFF"
      "-DLWS_ROLE_RAW_FILE=ON"
      "-DLWS_WITH_HTTP2=ON"
      "-DLWS_WITH_LWSWS=OFF"
      "-DLWS_WITH_CGI=OFF"
      "-DLWS_IPV6=ON"
      "-DLWS_UNIX_SOCK=ON"
      "-DLWS_WITH_PLUGINS=OFF"
      "-DLWS_WITH_PLUGINS_BUILTIN=OFF"
      "-DLWS_WITH_HTTP_PROXY=OFF"
      "-DLWS_WITH_ZIP_FOPS=OFF"
      "-DLWS_WITH_SOCKS5=OFF"
      "-DLWS_WITH_PEER_LIMITS=OFF"
      "-DLWS_WITH_RANGES=OFF"
      "-DLWS_WITH_THREADPOOL=OFF"
      "-DLWS_WITH_ACME=OFF"
      "-DLWS_WITH_HUBBUB=OFF"
      "-DLWS_WITH_ALSA=OFF"
      "-DLWS_WITH_GTK=OFF"
      "-DLWS_WITH_FTS=OFF"
      "-DLWS_WITH_SYS_ASYNC_DNS=OFF"
      "-DLWS_WITH_SYS_NTPCLIENT=OFF"
      "-DLWS_WITH_SYS_DHCP_CLIENT=OFF"
      "-DLWS_WITH_HTTP_BASIC_AUTH=ON"
      "-DLWS_WITH_HTTP_DIGEST_AUTH=ON"
      "-DLWS_WITH_HTTP_UNCOMMON_HEADERS=ON"
      "-DLWS_WITH_TLS_SESSIONS=ON"
      "-DLWS_SSL_CLIENT_USE_OS_CA_CERTS=ON"
      "-DLWS_TLS_LOG_PLAINTEXT_RX=OFF"
      "-DLWS_TLS_LOG_PLAINTEXT_TX=OFF"
      "-DLWS_WITHOUT_CLIENT=OFF"
      "-DLWS_WITHOUT_SERVER=OFF"
    SHARED_CMAKE_ARGS
      "-DLWS_WITH_STATIC=OFF"
      "-DLWS_WITH_SHARED=ON"
  )
endif()

# --- libunibreak ---
# Makefile-only upstream; wrapped to provide CMake install rules.
moredeps_add_dep(libunibreak
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/libunibreak"
)

# --- mimalloc ---
moredeps_add_dep(mimalloc
  CMAKE_ARGS
    "-DMI_BUILD_STATIC=ON"
    "-DMI_BUILD_SHARED=OFF"
    "-DMI_BUILD_OBJECT=OFF"
    "-DMI_BUILD_TESTS=OFF"
    "-DMI_OVERRIDE=OFF"
    "-DMI_SECURE=OFF"
    "-DMI_SECURE_FULL=OFF"
    "-DMI_GUARDED=OFF"
    "-DMI_OSX_INTERPOSE=ON"
    "-DMI_OSX_ZONE=ON"
    "-DMI_WIN_REDIRECT=ON"
    "-DMI_LOCAL_DYNAMIC_TLS=OFF"
    "-DMI_LIBC_MUSL=OFF"
    "-DMI_NO_THP=OFF"
    "-DMI_PADDING=OFF"
    "-DMI_SKIP_COLLECT_ON_EXIT=OFF"
    "-DMI_OPT_ARCH=OFF"
    "-DMI_OPT_SIMD=OFF"
    "-DMI_USE_CXX=OFF"
    "-DMI_DEBUG=OFF"
    "-DMI_DEBUG_INTERNAL=OFF"
    "-DMI_DEBUG_FULL=OFF"
    "-DMI_TRACK_VALGRIND=OFF"
    "-DMI_TRACK_ASAN=OFF"
    "-DMI_TRACK_ETW=OFF"
    "-DMI_SEE_ASM=OFF"
    "-DMI_INSTALL_TOPLEVEL=ON"
  SHARED_CMAKE_ARGS
    "-DMI_BUILD_STATIC=OFF"
    "-DMI_BUILD_SHARED=ON"
)

# --- pcre2 ---
moredeps_add_dep(pcre2
  CMAKE_ARGS
    "-DPCRE2_BUILD_PCRE2_8=ON"
    "-DPCRE2_BUILD_PCRE2_16=OFF"
    "-DPCRE2_BUILD_PCRE2_32=OFF"
    "-DPCRE2_BUILD_PCRE2GREP=OFF"
    "-DPCRE2_BUILD_TESTS=OFF"
    "-DPCRE2_SHOW_REPORT=OFF"
    "-DPCRE2_STATIC_PIC=ON"
    "-DPCRE2_SUPPORT_JIT=OFF"
    "-DPCRE2_SUPPORT_LIBBZ2=OFF"
    "-DPCRE2_SUPPORT_LIBZ=OFF"
    "-DPCRE2_SUPPORT_LIBEDIT=OFF"
    "-DPCRE2_SUPPORT_LIBREADLINE=OFF"
  SHARED_CMAKE_ARGS
    "-DPCRE2_BUILD_PCRE2_8=ON"
    "-DPCRE2_BUILD_PCRE2_16=OFF"
    "-DPCRE2_BUILD_PCRE2_32=OFF"
    "-DPCRE2_BUILD_PCRE2GREP=OFF"
    "-DPCRE2_BUILD_TESTS=OFF"
    "-DPCRE2_SHOW_REPORT=OFF"
    "-DPCRE2_SUPPORT_JIT=OFF"
    "-DPCRE2_SUPPORT_LIBBZ2=OFF"
    "-DPCRE2_SUPPORT_LIBZ=OFF"
    "-DPCRE2_SUPPORT_LIBEDIT=OFF"
    "-DPCRE2_SUPPORT_LIBREADLINE=OFF"
)

# --- miniaudio ---
moredeps_add_dep(miniaudio
  PATCH_COMMAND
    ${CMAKE_COMMAND}
    -DSOURCE_DIR=<SOURCE_DIR>
    -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/miniaudio_shared_link.patch
    -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply_patch.cmake
  CMAKE_ARGS
    "-DMINIAUDIO_BUILD_EXAMPLES=OFF"
    "-DMINIAUDIO_BUILD_TESTS=OFF"
    "-DMINIAUDIO_BUILD_TOOLS=OFF"
    "-DMINIAUDIO_INSTALL=ON"
)

# --- physfs ---
moredeps_add_dep(physfs
  CMAKE_ARGS
    "-DPHYSFS_BUILD_STATIC=ON"
    "-DPHYSFS_BUILD_SHARED=OFF"
    "-DPHYSFS_BUILD_TEST=OFF"
    "-DPHYSFS_BUILD_DOCS=OFF"
    "-DPHYSFS_ARCHIVE_ZIP=ON"
    "-DPHYSFS_ARCHIVE_7Z=ON"
    "-DPHYSFS_ARCHIVE_ISO9660=ON"
    "-DPHYSFS_ARCHIVE_GRP=OFF"
    "-DPHYSFS_ARCHIVE_WAD=OFF"
    "-DPHYSFS_ARCHIVE_HOG=OFF"
    "-DPHYSFS_ARCHIVE_MVL=OFF"
    "-DPHYSFS_ARCHIVE_QPAK=OFF"
    "-DPHYSFS_ARCHIVE_SLB=OFF"
    "-DPHYSFS_ARCHIVE_VDF=OFF"
  SHARED_CMAKE_ARGS
    "-DPHYSFS_BUILD_STATIC=OFF"
    "-DPHYSFS_BUILD_SHARED=ON"
)

# --- raylib ---
if(EMSCRIPTEN)
  moredeps_add_dep(raylib
    CMAKE_ARGS
      "-DBUILD_EXAMPLES=OFF"
      "-DBUILD_SHARED_LIBS=OFF"
      "-DPLATFORM=Web"
  )
else()
  moredeps_add_dep(raylib
    DEPENDS glfw
    CMAKE_ARGS
      "-DBUILD_EXAMPLES=OFF"
      "-DBUILD_SHARED_LIBS=OFF"
      "-DUSE_EXTERNAL_GLFW=ON"
      "-DOPENGL_VERSION=3.3"
  )
endif()

# --- mtcc ---
# TinyCC is Makefile-only and does not support Emscripten.
# TinyCC's Windows PE backend (tccpe.c) does not have ARM64 support, so skip
# it on Windows/ARM64 as well.
if(NOT EMSCRIPTEN AND NOT (WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64"))
  moredeps_add_dep(mtcc
    STATIC_ONLY
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/mtcc"
  )
endif()

# --- reproc ---
# Process spawning is not supported on the web.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(reproc
    PATCH_COMMAND
      ${CMAKE_COMMAND}
      -DSOURCE_DIR=<SOURCE_DIR>
      -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/reproc_export_deps.patch
      -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply_patch.cmake
    CMAKE_ARGS
      "-DREPROC_MULTITHREADED=ON"
  )
endif()

# --- sdl3 ---
moredeps_add_dep(sdl3
  CMAKE_ARGS
    "-DSDL_SHARED=OFF"
    "-DSDL_STATIC=ON"
    "-DSDL_TEST_LIBRARY=OFF"
    "-DSDL_TESTS=OFF"
    "-DSDL_EXAMPLES=OFF"
    "-DSDL_INSTALL=ON"
    "-DSDL_INSTALL_DOCS=OFF"
    "-DSDL_INSTALL_TESTS=OFF"
    "-DSDL_INSTALL_CPACK=OFF"
    "-DSDL_LEAN_AND_MEAN=ON"
    "-DSDL_RPATH=OFF"
    "-DSDL_FRAMEWORK=OFF"
    "-DSDL_DEPS_SHARED=OFF"
    "-DSDL_WERROR=OFF"
    "-DSDL_KMSDRM=OFF"
    "-DSDL_SNDIO=OFF"
    "-DSDL_OSS=OFF"
    "-DSDL_JACK=OFF"
    "-DSDL_WAYLAND_LIBDECOR=OFF"
    "-DSDL_WAYLAND_SHARED=OFF"
    "-DSDL_X11_SHARED=OFF"
    "-DSDL_COCOA_SHARED=OFF"
    "-DSDL_PULSEAUDIO_SHARED=OFF"
    "-DSDL_ALSA_SHARED=OFF"
    "-DSDL_PIPEWIRE_SHARED=OFF"
    "-DSDL_HIDAPI_LIBUSB_SHARED=OFF"
    "-DSDL_UNINSTALL=OFF"
  SHARED_CMAKE_ARGS
    "-DSDL_SHARED=ON"
    "-DSDL_STATIC=OFF"
)

# --- sdl3webgpu ---
# Upstream CMakeLists is missing cmake_minimum_required and links against
# targets that are built in separate ExternalProjects. We wrap it.
# On Emscripten we compile with -DWEBGPU_BACKEND_EMDAWNWEBGPU so that
# sdl3webgpu uses the emdawnwebgpu surface descriptor types.
moredeps_add_dep(sdl3webgpu
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3webgpu"
  DEPENDS sdl3 dawn
)

# --- SheenBidi ---
moredeps_add_dep(SheenBidi
  CMAKE_ARGS
    "-DBUILD_TESTING=OFF"
    "-DSB_CONFIG_UNITY=ON"
)

# --- skribidi ---
# Upstream fetches its own harfbuzz/SheenBidi/libunibreak/budouxc and sets
# global CMAKE_COMPILE_WARNING_AS_ERROR=ON. We instead build it from a wrapper
# that depends on the libraries already built by this project.
moredeps_add_dep(skribidi
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/skribidi"
  DEPENDS harfbuzz SheenBidi libunibreak budouxc
  CMAKE_ARGS
    "-DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}"
)

# --- sqlite-amalgamation ---
moredeps_add_dep(sqlite-amalgamation
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite-amalgamation"
)

# --- tinycsocket ---
# Upstream CMake builds the library but has no install target. We wrap it
# to add install rules without patching the submodule.
# BSD sockets are not available in a browser, so exclude on Emscripten.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(tinycsocket
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/tinycsocket"
    CMAKE_ARGS
      "-DTCS_ENABLE_TESTS=OFF"
      "-DTCS_ENABLE_EXAMPLES=OFF"
      "-DTCS_WARNINGS_AS_ERRORS=OFF"
      "-DTCS_GENERATE_COVERAGE=OFF"
  )
endif()

# --- tinycthread ---
# Minimal C11 threads API (thrd_create, mtx_lock, etc.) wrapping pthreads
# or Win32 threads.  Single .h + .c, ~500 lines, MIT.
# Excluded on Emscripten: requires pthreads.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(tinycthread
    CMAKE_ARGS
      "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
      "-DTINYCTHREAD_DISABLE_TESTS=ON"
      "-DTINYCTHREAD_INSTALL=ON"
    SHARED_CMAKE_ARGS
      "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
      "-DTINYCTHREAD_DISABLE_TESTS=ON"
      "-DTINYCTHREAD_INSTALL=ON"
  )
endif()

# --- tracy ---
moredeps_add_dep(tracy
  CMAKE_ARGS
    "-DTRACY_ENABLE=ON"
    "-DTRACY_Fortran=OFF"
    "-DTRACY_CLIENT_PYTHON=OFF"
    "-DTRACY_LTO=OFF"
    "-DTRACY_ON_DEMAND=OFF"
    "-DTRACY_CALLSTACK=OFF"
    "-DTRACY_NO_CALLSTACK=OFF"
    "-DTRACY_ONLY_LOCALHOST=OFF"
    "-DTRACY_NO_BROADCAST=OFF"
    "-DTRACY_ONLY_IPV4=OFF"
    "-DTRACY_NO_CODE_TRANSFER=OFF"
    "-DTRACY_NO_CONTEXT_SWITCH=OFF"
    "-DTRACY_NO_EXIT=OFF"
    "-DTRACY_NO_SAMPLING=OFF"
    "-DTRACY_NO_VERIFY=OFF"
    "-DTRACY_NO_VSYNC_CAPTURE=OFF"
    "-DTRACY_NO_FRAME_IMAGE=OFF"
    "-DTRACY_NO_SYSTEM_TRACING=OFF"
    "-DTRACY_PATCHABLE_NOPSLEDS=OFF"
    "-DTRACY_DELAYED_INIT=OFF"
    "-DTRACY_MANUAL_LIFETIME=OFF"
    "-DTRACY_FIBERS=OFF"
    "-DTRACY_NO_CRASH_HANDLER=OFF"
    "-DTRACY_TIMER_FALLBACK=OFF"
    "-DTRACY_LIBUNWIND_BACKTRACE=OFF"
    "-DTRACY_SYMBOL_OFFLINE_RESOLVE=OFF"
    "-DTRACY_LIBBACKTRACE_ELF_DYNLOAD_SUPPORT=OFF"
    "-DTRACY_DEBUGINFOD=OFF"
    "-DTRACY_IGNORE_MEMORY_FAULTS=OFF"
    "-DTRACY_VERBOSE=OFF"
    "-DTRACY_DEMANGLE=OFF"
    "-DTRACY_ROCPROF_CALIBRATION=OFF"
)

# --- utf8proc ---
moredeps_add_dep(utf8proc
  CMAKE_ARGS
    "-DUTF8PROC_INSTALL=ON"
    "-DUTF8PROC_ENABLE_TESTING=OFF"
    "-DLIB_FUZZING_ENGINE=OFF"
)

# --- zlib ---
moredeps_add_dep(zlib
  CMAKE_ARGS
    "-DZLIB_BUILD_STATIC=ON"
    "-DZLIB_BUILD_SHARED=OFF"
    "-DZLIB_BUILD_TESTING=OFF"
    "-DZLIB_INSTALL=ON"
    "-DZLIB_PREFIX=OFF"
  SHARED_CMAKE_ARGS
    "-DZLIB_BUILD_STATIC=OFF"
    "-DZLIB_BUILD_SHARED=ON"
)

# --- lz4 ---
moredeps_add_dep(lz4
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lz4/build/cmake"
  CMAKE_ARGS
    "-DLZ4_BUILD_CLI=OFF"
    "-DLZ4_POSITION_INDEPENDENT_LIB=ON"
    "-DBUILD_STATIC_LIBS=ON"
  SHARED_CMAKE_ARGS
    "-DBUILD_STATIC_LIBS=OFF"
)

# --- xxhash ---
moredeps_add_dep(xxhash
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/xxhash/cmake_unofficial"
  CMAKE_ARGS
    "-DXXHASH_BUILD_XXHSUM=OFF"
    "-DXXHASH_BUNDLED_MODE=OFF"
)

# --- zstd ---
moredeps_add_dep(zstd
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/zstd/build/cmake"
  CMAKE_ARGS
    "-DZSTD_BUILD_STATIC=ON"
    "-DZSTD_BUILD_SHARED=OFF"
    "-DZSTD_BUILD_COMPRESSION=ON"
    "-DZSTD_BUILD_DECOMPRESSION=ON"
    "-DZSTD_BUILD_DICTBUILDER=OFF"
    "-DZSTD_BUILD_DEPRECATED=OFF"
    "-DZSTD_BUILD_PROGRAMS=OFF"
    "-DZSTD_BUILD_CONTRIB=OFF"
    "-DZSTD_LEGACY_SUPPORT=OFF"
    "-DZSTD_USE_STATIC_RUNTIME=OFF"
    "-DZSTD_PROGRAMS_LINK_SHARED=OFF"
    "-DZSTD_ZLIB_SUPPORT=OFF"
    "-DZSTD_LZMA_SUPPORT=OFF"
    "-DZSTD_LZ4_SUPPORT=OFF"
    "-DZSTD_FRAMEWORK=OFF"
  SHARED_CMAKE_ARGS
    "-DZSTD_BUILD_STATIC=OFF"
    "-DZSTD_BUILD_SHARED=ON"
)

# -----------------------------------------------------------------------------
# Header-only / non-CMake dependency wrappers in src/
# -----------------------------------------------------------------------------
# Each wrapper is a small CMake project that builds a static library and
# installs it into the common prefix.

set(MOREDEPS_WRAPPER_NAMES
  cgltf
  FastNoiseLite
  microui
  minigamepad
  nanovg
  raudio
  sokol_app
  sokol_args
  sokol_audio
  sokol_fetch
  sokol_gfx
  sokol_glue
  sokol_gp
  sokol_log
  sokol_time
  stb_image
  stb_image_write
  stb_image_resize
  stb_truetype
  stb_rect_pack
  stb_ds
  ubench
  utest
)

# Wrappers that cannot be built as shared libraries.
set(MOREDEPS_WRAPPER_STATIC_ONLY
  sokol_app
  sokol_glue
)

foreach(_name IN LISTS MOREDEPS_WRAPPER_NAMES)
  set(_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/${_name}")
  if(NOT EXISTS "${_source_dir}/CMakeLists.txt")
    message(WARNING "Wrapper for ${_name} does not yet exist at ${_source_dir}; skipping.")
    continue()
  endif()

  # Wrappers can build static or shared depending on BUILD_SHARED_LIBS.
  if(_name IN_LIST MOREDEPS_WRAPPER_STATIC_ONLY)
    moredeps_add_dep(${_name}
      STATIC_ONLY
      SOURCE_DIR "${_source_dir}"
    )
  else()
    moredeps_add_dep(${_name}
      SOURCE_DIR "${_source_dir}"
    )
  endif()
endforeach()

# -----------------------------------------------------------------
# Sokol backend variants (Option C: build all sensible combinations)
# -----------------------------------------------------------------
set(_sokol_variants)
if(MOREDEPS_PLATFORM STREQUAL "macos_arm64")
  list(APPEND _sokol_variants
    gfx:GLCORE gfx:METAL gfx:WGPU
    app:GLCORE app:METAL
    glue:GLCORE glue:METAL
    gp:GLCORE gp:METAL
  )
elseif(MOREDEPS_PLATFORM STREQUAL "linux_x64" OR MOREDEPS_PLATFORM STREQUAL "linux_arm64")
  list(APPEND _sokol_variants
    gfx:GLCORE gfx:GLES3 gfx:WGPU
    app:GLCORE app:GLES3 app:WGPU
    glue:GLCORE glue:GLES3 glue:WGPU
    gp:GLCORE gp:GLES3
  )
elseif(MOREDEPS_PLATFORM MATCHES "windows_(x64|arm64)")
  list(APPEND _sokol_variants
    gfx:GLCORE gfx:D3D11 gfx:WGPU
    app:GLCORE app:D3D11 app:WGPU
    glue:GLCORE glue:D3D11 glue:WGPU
    gp:GLCORE gp:D3D11
  )
elseif(MOREDEPS_PLATFORM STREQUAL "wasm_emscripten")
  list(APPEND _sokol_variants
    gfx:GLES3 gfx:WGPU
    app:GLES3 app:WGPU
    glue:GLES3 glue:WGPU
    gp:GLES3
  )
endif()

foreach(_spec IN LISTS _sokol_variants)
  string(REPLACE ":" ";" _parts "${_spec}")
  list(GET _parts 0 _component)
  list(GET _parts 1 _backend)
  moredeps_sokol_variant("${_component}" "${_backend}")
endforeach()

# --- lua ---
# Lua is Makefile-only. We wrap it in src/lua/ to build it via CMake.
moredeps_add_dep(lua
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/lua"
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "moredeps configured. Build with: cmake --build ${CMAKE_BINARY_DIR}")
message(STATUS "Artifacts will be staged to: ${CMAKE_INSTALL_PREFIX}")
