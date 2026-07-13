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

# -----------------------------------------------------------------------------
# Helper for adding dependency subprojects
# -----------------------------------------------------------------------------
function(moredeps_add_dep name)
  set(options "")
  set(oneValueArgs SOURCE_DIR)
  set(multiValueArgs CMAKE_ARGS DEPENDS BUILD_BYPRODUCTS INSTALL_COMMAND PATCH_COMMAND)
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
endfunction()

# -----------------------------------------------------------------------------
# Dependency definitions
# -----------------------------------------------------------------------------

# --- boringssl ---
# Single TLS backend for curl and libwebsockets. Installs OpenSSL::Crypto/
# OpenSSL::SSL targets into the common prefix.
moredeps_add_dep(boringssl
  CMAKE_ARGS
    "-DBUILD_TESTING=OFF"
    "-DFIPS=OFF"
    "-DOPENSSL_NO_ASM=$<IF:$<PLATFORM_ID:Emscripten>,ON,OFF>"
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
)

# --- cimgui ---
# Upstream CMakeLists hard-codes SHARED. We wrap it to build a static lib.
moredeps_add_dep(cimgui
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cimgui"
)

# --- cJSON ---
moredeps_add_dep(cJSON
  CMAKE_ARGS
    "-DENABLE_CJSON_TEST=OFF"
    "-DENABLE_CJSON_UNINSTALL=OFF"
    "-DENABLE_CJSON_UTILS=OFF"
    "-DENABLE_LOCALES=ON"
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
)

# --- dawn ---
# WebGPU-only build. Emscripten uses emdawnwebgpu (handled via dawn's own
# Emscripten options). Dependencies are pre-populated as git submodules in
# deps/dawn_third_party (they cannot be nested inside the deps/dawn submodule).
moredeps_add_dep(dawn
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
    "-DDAWN_ENABLE_DESKTOP_GL=OFF"
    "-DDAWN_ENABLE_OPENGLES=OFF"
  INSTALL_COMMAND
    ${CMAKE_COMMAND} -DDAWN_BUILD_DIR=<BINARY_DIR>
                     -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                     -DEMSCRIPTEN=$<BOOL:$<PLATFORM_ID:Emscripten>>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_dawn.cmake
)

# --- enet ---
moredeps_add_dep(enet
  CMAKE_ARGS
    "-DENET_STATIC=ON"
    "-DENET_SHARED=OFF"
    "-DENET_TEST=OFF"
    "-DENET_USE_MORE_PEERS=OFF"
    "-DENET_IPV4_ONLY=OFF"
)

# --- flecs ---
moredeps_add_dep(flecs
  CMAKE_ARGS
    "-DFLECS_STATIC=ON"
    "-DFLECS_SHARED=OFF"
    "-DFLECS_TESTS=OFF"
    "-DFLECS_PIC=ON"
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

# --- ghostty ---
# Zig build. On macOS it produces libghostty.a inside an xcframework. We copy
# the source to the build tree, run zig build, and install the library + header.
if(MOREDEPS_PLATFORM STREQUAL "macos_arm64")
  moredeps_add_dep(ghostty
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ghostty"
  )
endif()

# --- glfw ---
# GLFW is a desktop windowing library; it does not build for Emscripten.
if(NOT EMSCRIPTEN)
  moredeps_add_dep(glfw
    CMAKE_ARGS
      "-DBUILD_SHARED_LIBS=OFF"
      "-DGLFW_BUILD_DOCS=OFF"
      "-DGLFW_INSTALL=ON"
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
# Uses BoringSSL from the common prefix.
if(WIN32)
  set(_LWS_SSL_LIBS "${CMAKE_INSTALL_PREFIX}/lib/ssl.lib;${CMAKE_INSTALL_PREFIX}/lib/crypto.lib")
else()
  set(_LWS_SSL_LIBS "${CMAKE_INSTALL_PREFIX}/lib/libssl.a;${CMAKE_INSTALL_PREFIX}/lib/libcrypto.a")
endif()
moredeps_add_dep(libwebsockets
  DEPENDS boringssl
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
    "-DLWS_OPENSSL_LIBRARIES=${_LWS_SSL_LIBS}"
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
)

# --- mimalloc ---
moredeps_add_dep(mimalloc
  CMAKE_ARGS
    "-DMI_BUILD_STATIC=ON"
    "-DMI_BUILD_SHARED=OFF"
    "-DMI_BUILD_OBJECT=OFF"
    "-DMI_BUILD_TESTS=OFF"
    "-DMI_OVERRIDE=ON"
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
)

# --- miniaudio ---
moredeps_add_dep(miniaudio
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
if(NOT EMSCRIPTEN)
  moredeps_add_dep(mtcc
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/mtcc"
  )
endif()

# --- reproc ---
moredeps_add_dep(reproc
  CMAKE_ARGS
    "-DREPROC_MULTITHREADED=ON"
)

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
)

# --- sdl3webgpu ---
# Upstream CMakeLists is missing cmake_minimum_required and links against
# targets that are built in separate ExternalProjects. We wrap it.
# Excluded on Emscripten: the current sdl3webgpu and emdawnwebgpu versions use
# incompatible WebGPU surface descriptor types (WGPUSurfaceDescriptorFromCanvasHTMLSelector
# was removed/renamed in the upstream WebGPU header).
if(NOT EMSCRIPTEN)
  moredeps_add_dep(sdl3webgpu
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl3webgpu"
    DEPENDS sdl3 dawn
  )
endif()

# --- skribidi ---
# Upstream CMakeLists fetches its own harfbuzz/SheenBidi/libunibreak/budouxc
# and sets CMAKE_COMPILE_WARNING_AS_ERROR globally. We build it from a copy
# of the source in the build tree so the submodule stays clean, and patch the
# warning-as-error setting in the copy only.
set(_SKRIBIDI_SRC_COPY "${CMAKE_BINARY_DIR}/skribidi_src_copy")
set(_SKRIBIDI_PATCH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch_skribidi.py")
ExternalProject_Add(
  skribidi
  DOWNLOAD_COMMAND
    ${CMAKE_COMMAND} -E rm -rf ${_SKRIBIDI_SRC_COPY}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/skribidi
      ${_SKRIBIDI_SRC_COPY}
  SOURCE_DIR ${_SKRIBIDI_SRC_COPY}
  BINARY_DIR "${CMAKE_BINARY_DIR}/skribidi"
  INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
  PATCH_COMMAND
    python3 ${_SKRIBIDI_PATCH_SCRIPT} ${_SKRIBIDI_SRC_COPY}
  CMAKE_ARGS
    ${MOREDEPS_COMMON_CMAKE_ARGS}
    "-DSKRIBIDI_EXAMPLE=OFF"
    "-DSKRIBIDI_UNIT_TESTS=OFF"
    "-DENABLE_ASAN=OFF"
)

# --- sqlite-amalgamation ---
moredeps_add_dep(sqlite-amalgamation
  CMAKE_ARGS
    "-DBUILD_SHELL=OFF"
    "-DSQLITE_ENABLE_MATH_FUNCTIONS=ON"
    "-DSQLITE_RECOMMENDED_OPTIONS=ON"
    "-DSQLITE_OMIT_DECLTYPE=ON"
    "-DSQLITE_OMIT_JSON=OFF"
    "-DSQLITE_ENABLE_FTS5=OFF"
    "-DSQLITE_ENABLE_RTREE=OFF"
    "-DSQLITE_ENABLE_STAT4=OFF"
    "-DSQLITE_ENABLE_GEOPOLY=OFF"
    "-DSQLITE_ENABLE_RBU=OFF"
    "-DSQLITE_ENABLE_DBSTAT_VTAB=OFF"
    "-DSQLITE_ENABLE_COLUMN_METADATA=OFF"
    "-DSQLITE_ENABLE_ICU=OFF"
    "-DSQLITE_USE_URI=OFF"
    "-DBUILD_WITH_XPSDK=OFF"
)

# --- tinycsocket ---
# Upstream CMake builds the library but has no install target. We wrap it
# to add install rules without patching the submodule.
moredeps_add_dep(tinycsocket
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/tinycsocket"
  CMAKE_ARGS
    "-DTCS_ENABLE_TESTS=OFF"
    "-DTCS_ENABLE_EXAMPLES=OFF"
    "-DTCS_WARNINGS_AS_ERRORS=OFF"
    "-DTCS_GENERATE_COVERAGE=OFF"
)

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
)

# --- lz4 ---
moredeps_add_dep(lz4
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/lz4/build/cmake"
  CMAKE_ARGS
    "-DLZ4_BUILD_CLI=OFF"
    "-DLZ4_POSITION_INDEPENDENT_LIB=ON"
    "-DBUILD_STATIC_LIBS=ON"
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
)

# -----------------------------------------------------------------------------
# Header-only / non-CMake dependency wrappers in src/
# -----------------------------------------------------------------------------
# Each wrapper is a small CMake project that builds a static library and
# installs it into the common prefix.

set(MOREDEPS_WRAPPER_NAMES
  cgltf
  FastNoiseLite
  fontstash
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

foreach(_name IN LISTS MOREDEPS_WRAPPER_NAMES)
  set(_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/${_name}")
  if(NOT EXISTS "${_source_dir}/CMakeLists.txt")
    message(WARNING "Wrapper for ${_name} does not yet exist at ${_source_dir}; skipping.")
    continue()
  endif()

  ExternalProject_Add(
    ${_name}
    SOURCE_DIR "${_source_dir}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/${_name}"
    INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
    CMAKE_ARGS
      ${MOREDEPS_COMMON_CMAKE_ARGS}
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/${_name}/${CMAKE_STATIC_LIBRARY_PREFIX}${_name}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
endforeach()

# --- lua ---
# Lua is Makefile-only. We wrap it in src/lua/ to build it via CMake.
if(NOT MOREDEPS_PLATFORM STREQUAL "wasm_emscripten")
  moredeps_add_dep(lua
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/lua"
  )
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "moredeps configured. Build with: cmake --build ${CMAKE_BINARY_DIR}")
message(STATUS "Artifacts will be staged to: ${CMAKE_INSTALL_PREFIX}")
