# CMake Build Options & Proposed Defaults

This document collects the build options exposed by the CMake-based dependencies (and notes for non-CMake ones) so we can decide on production defaults.  The guiding principle is:

> **Library only. No tests, no examples, no docs, no tools unless they are required to produce the library.**

These defaults will be encoded in the top-level `CMakeLists.txt` and the per-dependency wrappers in `src/<dep>/`.

---

## Global CMake defaults

Set at the top level and inherited by all subdirectories:

| Option | Value | Reason |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` | We produce release artifacts. |
| `BUILD_SHARED_LIBS` | `OFF` | Static libraries by default. Shared libraries are deferred to a later phase. |
| `CMAKE_POSITION_INDEPENDENT_CODE` | `ON` | Safe for static libs and required if we later mix in shared libs. |
| `CMAKE_INSTALL_PREFIX` | `_out/<platform>` | Staged output directory. |
| `CMAKE_C_STANDARD` | `11` | C99/C11 features (e.g., cJSON `long long`) required. |
| `CMAKE_CXX_STANDARD` | `17` | Matches modern C++ deps (Dawn, abseil, etc.). |
| `CMAKE_PREFIX_PATH` | `_out/<platform>` | Allows cross-compilation sub-builds to find installed deps. |
| `CMAKE_FIND_ROOT_PATH` | `_out/<platform>` | Same as above for cross-compilation. |

---

## Per-dependency option defaults

### `box3d`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BOX3D_SAMPLES` | `ON` | `OFF` | No examples. |
| `BOX3D_BENCHMARKS` | `OFF` | `OFF` | Keep off. |
| `BOX3D_UNIT_TESTS` | `ON` | `OFF` | No tests. |
| `BOX3D_DOCS` | `OFF` | `OFF` | Keep off. |
| `BOX3D_BUILD_SHADERS` | `OFF` | `OFF` | Sokol-shdc dependency; keep off. |
| `BOX3D_DOUBLE_PRECISION` | `OFF` | `OFF` | Keep float unless needed. |
| `BOX3D_DISABLE_SIMD` | `OFF` | `OFF` | Use SIMD on supported platforms. |
| `BOX3D_PROFILE` | `OFF` | `OFF` | Tracy integration; off by default. |

### `cglm`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `CGLM_STATIC` | `OFF` (top-level) | `ON` | Force static library. |
| `CGLM_SHARED` | `ON` | `OFF` | No shared lib in this phase. |
| `CGLM_USE_TEST` | `OFF` | `OFF` | No tests. |
| `CGLM_USE_C99` | `OFF` | `OFF` | Keep default. |

### `cimgui`

`deps/cimgui/CMakeLists.txt` is hard-coded to build a `SHARED` library.  We will therefore **not** call `add_subdirectory(deps/cimgui)` directly; instead we will create a `src/cimgui/` wrapper that compiles `cimgui.cpp` and the ImGui sources as a static library.  This avoids patching the upstream repo.

| Wrapper setting | Proposed value | Notes |
|---|---|---|
| Library type | `STATIC` | Required for our static-artifact goal. |
| `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` | `1` | Matches upstream. |
| `IMGUI_IMPL_API` | `extern "C"` | Matches upstream. |

### `cJSON`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | `OFF` | Global override. |
| `ENABLE_CJSON_TEST` | `ON` | `OFF` | No tests. |
| `ENABLE_CJSON_UNINSTALL` | `ON` | `OFF` | Unnecessary in a deps build. |
| `ENABLE_CJSON_UTILS` | `OFF` | `OFF` | Keep off unless required. |
| `ENABLE_LOCALES` | `ON` | `ON` | Keep default. |

### `boringssl`

BoringSSL replaces OpenSSL as the single TLS backend for `curl` and `libwebsockets`.

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_SHARED_LIBS` | `OFF` | `OFF` | Static libraries. |
| `BUILD_TESTING` | `ON` | `OFF` | No tests. |
| `FIPS` | `OFF` | `OFF` | Keep off. |
| `INSTALL_ENABLED` | `1` | `1` | Keep install enabled for `crypto`/`ssl` targets and headers. |

BoringSSL does not expose an option to disable the `bssl` command-line tool, so it will be built but **not** included in the packaged artifact. Only `libcrypto.a`, `libssl.a`, and the public headers are packaged.

### `curl`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_CURL_EXE` | `ON` | `OFF` | No executable. |
| `BUILD_STATIC_LIBS` | `OFF` | `ON` | We want static libcurl. |
| `BUILD_TESTING` | `ON` | `OFF` | No tests. |
| `BUILD_EXAMPLES` | `ON` | `OFF` | No examples. |
| `BUILD_LIBCURL_DOCS` | `ON` | `OFF` | No docs. |
| `BUILD_MISC_DOCS` | `ON` | `OFF` | No docs. |
| `ENABLE_CURL_MANUAL` | `ON` | `OFF` | No docs. |
| `CURL_ENABLE_SSL` | `ON` | `ON` | Keep SSL. |
| `CURL_USE_BORINGSSL` | `OFF` | `ON` | Use BoringSSL on all platforms. |
| `CURL_DISABLE_LDAP` | `ON` | `ON` | Keep off. |
| `CURL_USE_LIBPSL` | `ON` | `OFF` | Avoid libpsl system dependency. |
| `CURL_USE_LIBSSH2` | `ON` | `OFF` | Avoid libssh2 system dependency. |
| `CURL_BROTLI` | `OFF` | `OFF` | Keep off. |
| `CURL_ZSTD` | `OFF` | `OFF` | Keep off. |
| `USE_NGHTTP2` | `ON` | `OFF` | HTTP/2 deferred; avoids nghttp2 dependency. |
| `USE_NGTCP2` | `OFF` | `OFF` | HTTP/3; keep off. |
| `USE_QUICHE` | `OFF` | `OFF` | HTTP/3; keep off. |
| `USE_LIBIDN2` | `ON` | `OFF` | Avoid libidn2 system dependency. |
| `CURL_DISABLE_LDAP` | `ON` | `ON` | Keep off. |

**TLS backend:** BoringSSL (`deps/boringssl`) is the single TLS backend on all platforms. This replaces the previous platform-native approach and removes the OpenSSL dependency.

### `dawn`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `DAWN_ENABLE_INSTALL` | `OFF` | `ON` on native, `OFF` on Emscripten | Emscripten produces no installable static library. |
| `DAWN_BUILD_BENCHMARKS` | `OFF` | `OFF` | No benchmarks. |
| `DAWN_BUILD_FUZZERS` | `OFF` | `OFF` | No fuzzers. |
| `DAWN_BUILD_NODE_BINDINGS` | `OFF` | `OFF` | No Node.js bindings. |
| `DAWN_BUILD_PROTOBUF` | `ON` | `OFF` | Only needed for some tools; disable for WebGPU-only build. |
| `DAWN_ENABLE_SWIFTSHADER` | `OFF` | `OFF` | Keep off. |
| `DAWN_FETCH_DEPENDENCIES` | `ON` | `OFF` | Pre-populate third-party deps as git submodules in `deps/dawn_third_party/` and set `DAWN_THIRD_PARTY_DIR`. Avoids the broken Python fetch script. |
| `TINT_BUILD_TESTS` | `ON` | `OFF` | No tests. |
| `TINT_BUILD_CMD_TOOLS` | `ON` | `OFF` | No tools. |
| `TINT_BUILD_GLSL_VALIDATOR` | `ON` | `OFF` | Only needed for tool output. |
| `DAWN_BUILD_MONOLITHIC_LIBRARY` | `STATIC` | `STATIC` on native, `OFF` on Emscripten | Static monolithic library on desktop; none on Emscripten. |
| `DAWN_ENABLE_VULKAN` | varies | `ON` on Linux, `OFF` otherwise | Only needed on Linux. |
| `DAWN_ENABLE_METAL` | varies | `ON` on macOS, `OFF` otherwise | Only needed on macOS. |

**Emscripten note:** On `wasm_emscripten`, Dawn does not produce a static library. Instead, `scripts/install_dawn.cmake` stages the `emdawnwebgpu` headers (`webgpu/webgpu.h`, `dawn/dawn_version.h`) and JavaScript files (`library_webgpu_*.js`) into `_out/wasm_emscripten/`.

### `enet`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `ENET_STATIC` | `ON` | `ON` | Static lib. |
| `ENET_SHARED` | `OFF` | `OFF` | No shared. |
| `ENET_TEST` | `ON` | `OFF` | No tests. |
| `ENET_USE_MORE_PEERS` | `OFF` | `OFF` | Keep default unless needed. |
| `ENET_IPV4_ONLY` | `OFF` | `OFF` | Keep IPv6. |

**Excluded on `wasm_emscripten`** (no UDP sockets).

### `flecs`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `FLECS_STATIC` | `ON` | `ON` | Static lib. |
| `FLECS_SHARED` | `ON` | `OFF` | No shared. |
| `FLECS_TESTS` | `OFF` | `OFF` | No tests. |
| `FLECS_PIC` | `ON` | `ON` | Keep default. |

### `freetype`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `FT_DISABLE_ZLIB` | `OFF` | `OFF` | Use zlib for compressed fonts. |
| `FT_DISABLE_BZIP2` | `OFF` | `ON` | Disable bzip2 to reduce deps. |
| `FT_DISABLE_PNG` | `OFF` | `ON` | Disable PNG bitmap support unless needed. |
| `FT_DISABLE_HARFBUZZ` | `OFF` | `OFF` | **Enable** HarfBuzz interdependency for improved auto-hinting. |
| `FT_DISABLE_BROTLI` | `OFF` | `ON` | Disable WOFF2 support to reduce deps. |
| `FT_ENABLE_ERROR_STRINGS` | `OFF` | `ON` | Useful error descriptions. |

### `glfw`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_SHARED_LIBS` | `OFF` | `OFF` | Static. |
| `GLFW_BUILD_DOCS` | `ON` | `OFF` | No docs. |
| `GLFW_INSTALL` | `ON` | `ON` | Keep install target. |

**Excluded on `wasm_emscripten`** (web apps use SDL3 / emscripten HTML5 APIs).

### `harfbuzz`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `HB_HAVE_CAIRO` | `OFF` | `OFF` | No Cairo. |
| `HB_HAVE_FREETYPE` | `OFF` | `ON` | **Enable** FreeType interop for improved auto-hinting. FreeType must be built first. |
| `HB_HAVE_GRAPHITE2` | `OFF` | `OFF` | Keep off. |
| `HB_HAVE_GLIB` | `OFF` | `OFF` | Keep off. |
| `HB_HAVE_ICU` | `OFF` | `OFF` | Keep off. |
| `HB_HAVE_CORETEXT` | `ON` on macOS | `ON` on macOS | Native text shaping on macOS. |
| `HB_HAVE_UNISCRIBE` | `OFF` | `OFF` | Keep off unless needed on Windows. |
| `HB_HAVE_GDI` | `OFF` | `OFF` | Keep off. |
| `HB_HAVE_DIRECTWRITE` | `OFF` | `OFF` | Keep off. |
| `HB_BUILD_UTILS` | `OFF` | `OFF` | No utils. |
| `HB_BUILD_SUBSET` | `ON` | `ON` | Keep subset library. |
| `HB_HAVE_GOBJECT` | `OFF` | `OFF` | Keep off. |
| `HB_HAVE_INTROSPECTION` | `OFF` | `OFF` | Keep off. |

### `libwebsockets`

`libwebsockets` has many options.  We will build a minimal client/server library with SSL.

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `LWS_WITH_STATIC` | `ON` | `ON` | Static lib. |
| `LWS_WITH_SHARED` | `ON` | `OFF` | No shared. |
| `LWS_WITHOUT_TESTAPPS` | `OFF` | `ON` | No test apps. |
| `LWS_WITHOUT_TEST_SERVER` | `OFF` | `ON` | No test server. |
| `LWS_WITHOUT_TEST_SERVER_EXTPOLL` | `OFF` | `ON` | No test server. |
| `LWS_WITHOUT_TEST_PING` | `OFF` | `ON` | No test app. |
| `LWS_WITHOUT_TEST_CLIENT` | `OFF` | `ON` | No test app. |
| `LWS_WITH_MINIMAL_EXAMPLES` | `OFF`/`ON` | `OFF` | No examples. |
| `LWS_WITH_SSL` | `ON` | `ON` | Keep SSL. |
| `LWS_WITH_BORINGSSL` | `OFF` | `ON` | Use BoringSSL on all platforms. |
| `LWS_WITH_MBEDTLS` | `OFF` | `OFF` | Do not use mbedTLS. |
| `LWS_WITH_WOLFSSL` | `OFF` | `OFF` | Do not use wolfSSL. |
| `LWS_WITH_ZLIB` | `OFF` | `OFF` | Keep off unless compression extensions needed. |
| `LWS_WITH_LIBUV` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_LIBEV` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_LIBEVENT` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_GL` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SMTP` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SPAWN` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_FSMOUNT` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_OTA` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_LEJP` | `ON` | `OFF` | Optional; disable if not needed. |
| `LWS_WITH_LHP` | `ON` | `OFF` | Optional HTML5 parser. |
| `LWS_WITH_JSONRPC` | `ON` | `OFF` | Optional. |
| `LWS_WITH_CONMON` | `ON` | `OFF` | Optional. |
| `LWS_WITH_WOL` | `ON` | `OFF` | Optional. |
| `LWS_WITH_CACHE_NSCOOKIEJAR` | `ON`/`OFF` | `OFF` | Optional. |
| `LWS_WITH_NETLINK` | `ON` | `OFF` | Linux-only; disable if not needed. |
| `LWS_WITH_BINDTODEVICE` | `ON` | `OFF` | Linux-only. |
| `LWS_WITH_LIBCAP` | `ON` | `OFF` | Linux-only. |
| `LWS_WITH_SECURE_STREAMS` | `ON` | `OFF` | Disable to reduce size unless needed. |
| `LWS_WITH_HTTP_STREAM_COMPRESSION` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_HTTP_BROTLI` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_JOSE` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_COSE` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_GENCRYPTO` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SQLITE3` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_STRUCT_JSON` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_STRUCT_SQLITE3` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_DRIVERS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_CBOR` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_ALLOC_METADATA_LWS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_COMPRESSED_BACKTRACES` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_DISKCACHE` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_LWS_DSH` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_PLUGINS_API` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_EVLIB_PLUGINS` | `ON`/`OFF` | `OFF` | Keep off. |
| `LWS_WITH_GCOV` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_ASAN` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_FANALYZER` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_NO_LOGS` | `OFF` | `OFF` | Keep logs. |
| `LWS_WITH_UDP` | `ON` | `ON` | Keep for network. |
| `LWS_WITH_CLIENT_HTTP_PROXYING` | `ON` | `ON` | Keep proxy support. |
| `LWS_WITH_FILE_OPS` | `ON` | `ON` | Keep file ops. |
| `LWS_WITH_CUSTOM_HEADERS` | `ON` | `ON` | Keep custom headers. |
| `LWS_WITH_LWSAC` | `ON` | `ON` | Keep chunk allocator. |
| `LWS_WITH_EXTERNAL_POLL` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_TLS_JIT_TRUST` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SELFTESTS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_FAULT_INJECTION` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_METRICS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_STATE` | `ON` | `ON` | Keep. |
| `LWS_WITH_SYS_SMD` | `ON` | `ON` | Keep. |
| `LWS_WITH_UPNG` | `ON` | `ON` | Keep PNG stream decoder. |
| `LWS_WITH_GZINFLATE` | `ON` | `ON` | Keep gzip inflator. |
| `LWS_WITH_JPEG` | `ON` | `ON` | Keep JPEG decoder. |
| `LWS_WITH_DLO` | `ON` | `ON` | Keep display-list objects. |
| `LWS_HTTP_HEADERS_ALL` | `OFF` | `OFF` | Keep header reduction. |
| `LWS_WITH_SUL_DEBUGGING` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_WAKE_LOGGING` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_MCUFONT_ENCODER` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_COMPRESSED_BACKTRACES` | `OFF` | `OFF` | Keep off. |
| `LWS_MSVC_STATIC_RUNTIME` | `OFF` | `OFF` | Keep dynamic CRT. |
| `LWS_REPRODUCIBLE` | `ON` | `ON` | Good to keep. |
| `LWS_WITH_EXPORT_LWSTARGETS` | `ON` | `ON` | Keep. |
| `LWS_WITHOUT_EXTENSIONS` | `ON` | `ON` | Keep off (extensions disabled). |
| `LWS_WITHOUT_DAEMONIZE` | `ON` | `ON` | Keep. |
| `LWS_WITHOUT_BUILTIN_SHA1` | `OFF` | `OFF` | Keep. |
| `LWS_FALLBACK_GETHOSTBYNAME` | `OFF` | `OFF` | Keep. |
| `LWS_AVOID_SIGPIPE_IGN` | `OFF` | `OFF` | Keep. |
| `LWS_WITH_NETWORK` | `ON` | `ON` | Keep. |
| `LWS_ROLE_H1` | `ON` | `ON` | Keep HTTP/1. |
| `LWS_ROLE_WS` | `ON` | `ON` | Keep WebSockets. |
| `LWS_ROLE_MQTT` | `OFF` | `OFF` | Keep off. |
| `LWS_ROLE_DBUS` | `OFF` | `OFF` | Keep off. |
| `LWS_ROLE_RAW_PROXY` | `OFF` | `OFF` | Keep off. |
| `LWS_ROLE_RAW_FILE` | `ON` | `ON` | Keep raw files. |
| `LWS_WITH_HTTP2` | `ON` | `ON` | Keep HTTP/2. |
| `LWS_WITH_LWSWS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_CGI` | `OFF` | `OFF` | Keep off. |
| `LWS_IPV6` | `OFF` | `ON` | Enable IPv6. |
| `LWS_UNIX_SOCK` | `ON` | `ON` | Keep on Unix. |
| `LWS_WITH_PLUGINS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_PLUGINS_BUILTIN` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_HTTP_PROXY` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_ZIP_FOPS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SOCKS5` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_PEER_LIMITS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_RANGES` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_THREADPOOL` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_ACME` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_HUBBUB` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_ALSA` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_GTK` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_FTS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_ASYNC_DNS` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_NTPCLIENT` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_SYS_DHCP_CLIENT` | `OFF` | `OFF` | Keep off. |
| `LWS_WITH_HTTP_BASIC_AUTH` | `ON` | `ON` | Keep. |
| `LWS_WITH_HTTP_DIGEST_AUTH` | `ON` | `ON` | Keep. |
| `LWS_WITH_HTTP_UNCOMMON_HEADERS` | `ON` | `ON` | Keep. |
| `LWS_WITH_TLS_SESSIONS` | `ON` | `ON` | Keep. |
| `LWS_SSL_CLIENT_USE_OS_CA_CERTS` | `ON` | `ON` | Keep. |
| `LWS_TLS_LOG_PLAINTEXT_RX` | `OFF` | `OFF` | Keep off. |
| `LWS_TLS_LOG_PLAINTEXT_TX` | `OFF` | `OFF` | Keep off. |
| `LWS_WITHOUT_CLIENT` | `OFF` | `OFF` | Keep client. |
| `LWS_WITHOUT_SERVER` | `OFF` | `OFF` | Keep server. |

**BoringSSL compatibility:** `libwebsockets`'s `CHECK_FUNCTION_EXISTS` feature detection misses several BoringSSL accessor APIs, so the following CMake cache variables are forced to `1`:
- `LWS_HAVE_RSA_SET0_KEY`
- `LWS_HAVE_ECDSA_SIG_get0`
- `LWS_HAVE_ECDSA_SIG_set0`
- `LWS_HAVE_BN_bn2binpad`

Additionally, `LWS_OPENSSL_LIBRARIES` and `LWS_OPENSSL_INCLUDE_DIRS` are pointed at the installed BoringSSL prefix, and `DISABLE_WERROR=ON` is set to avoid BoringSSL-related warnings being treated as errors.

**Windows note:** `CMakeLists.txt` selects `ssl.lib`/`crypto.lib` for `LWS_OPENSSL_LIBRARIES` on `WIN32`; Unix builds use `libssl.a`/`libcrypto.a`.

**Excluded on `wasm_emscripten`**.

### `mimalloc`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `MI_BUILD_STATIC` | `ON` | `ON` | Static lib. |
| `MI_BUILD_SHARED` | `ON` | `OFF` | No shared. |
| `MI_BUILD_OBJECT` | `ON` | `OFF` | No object lib artifact. |
| `MI_BUILD_TESTS` | `ON` | `OFF` | No tests. |
| `MI_OVERRIDE` | `ON` | `ON` | Override malloc; standard for mimalloc. |
| `MI_SECURE` | `OFF` | `OFF` | Keep off. |
| `MI_SECURE_FULL` | `OFF` | `OFF` | Keep off. |
| `MI_GUARDED` | `OFF` | `OFF` | Keep off. |
| `MI_OSX_INTERPOSE` | `ON` | `ON` | Keep on macOS. |
| `MI_OSX_ZONE` | `ON` | `ON` | Keep on macOS. |
| `MI_WIN_REDIRECT` | `ON` | `ON` | Keep on Windows. |
| `MI_LOCAL_DYNAMIC_TLS` | `OFF` | `OFF` | Keep off. |
| `MI_LIBC_MUSL` | `OFF` | `OFF` | Keep off. |
| `MI_NO_THP` | `OFF` | `OFF` | Keep off. |
| `MI_PADDING` | `OFF` | `OFF` | Keep off. |
| `MI_SKIP_COLLECT_ON_EXIT` | `OFF` | `OFF` | Keep off. |
| `MI_OPT_ARCH` | `OFF` | `OFF` | Keep off for portable builds. |
| `MI_OPT_SIMD` | `OFF` | `OFF` | Keep off. |
| `MI_USE_CXX` | `OFF` | `OFF` | Keep C. |
| `MI_DEBUG*` | `OFF` | `OFF` | Keep off. |
| `MI_TRACK_*` | `OFF` | `OFF` | Keep off. |
| `MI_SEE_ASM` | `OFF` | `OFF` | Keep off. |
| `MI_INSTALL_TOPLEVEL` | `OFF` | `ON` | Install into prefix root for cleaner artifact. |

### `miniaudio`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `MINIAUDIO_BUILD_EXAMPLES` | `OFF` | `OFF` | No examples. |
| `MINIAUDIO_BUILD_TESTS` | `OFF` | `OFF` | No tests. |
| `MINIAUDIO_BUILD_TOOLS` | `OFF` | `OFF` | No tools. |
| `MINIAUDIO_INSTALL` | `ON` | `ON` | Keep install. |
| `MINIAUDIO_ENABLE_ONLY_SPECIFIC_BACKENDS` | `OFF` | `OFF` | Keep default backend selection. |
| `MINIAUDIO_NO_DECODING` | `OFF` | `OFF` | Keep decoding. |
| `MINIAUDIO_NO_ENCODING` | `OFF` | `OFF` | Keep encoding. |
| `MINIAUDIO_NO_DEVICEIO` | `OFF` | `OFF` | Keep device I/O. |
| `MINIAUDIO_NO_RESOURCE_MANAGER` | `OFF` | `OFF` | Keep resource manager. |
| `MINIAUDIO_NO_NODE_GRAPH` | `OFF` | `OFF` | Keep node graph. |
| `MINIAUDIO_NO_ENGINE` | `OFF` | `OFF` | Keep engine. |
| `MINIAUDIO_NO_GENERATION` | `OFF` | `OFF` | Keep generation. |
| `MINIAUDIO_NO_THREADING` | `OFF` | `OFF` | Keep threading. |
| `MINIAUDIO_NO_RUNTIME_LINKING` | `OFF` | `OFF` | Keep runtime linking. |
| `MINIAUDIO_NO_SSE2` / `AVX2` / `NEON` | `OFF` | `OFF` | Keep SIMD on supported platforms. |
| `MINIAUDIO_NO_WAV` / `FLAC` / `MP3` | `OFF` | `OFF` | Keep decoders. |
| `MINIAUDIO_NO_LIBVORBIS` / `LIBOPUS` | `OFF` | `OFF` | Keep unless size matters. |
| Backends (WASAPI, CoreAudio, ALSA, PulseAudio, etc.) | `OFF` (enable flags) | platform defaults | Use default backend auto-selection. On Emscripten, ensure WebAudio is used. |

### `physfs`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `PHYSFS_BUILD_STATIC` | `TRUE` | `TRUE` | Static lib. |
| `PHYSFS_BUILD_SHARED` | `TRUE` | `FALSE` | No shared. |
| `PHYSFS_BUILD_TEST` | `TRUE` | `FALSE` | No test. |
| `PHYSFS_BUILD_DOCS` | `TRUE` | `FALSE` | No docs. |
| `PHYSFS_DISABLE_INSTALL` | `OFF` | `OFF` | Keep install. |
| `PHYSFS_ARCHIVE_ZIP` | `TRUE` | `TRUE` | Keep ZIP. |
| `PHYSFS_ARCHIVE_7Z` | `TRUE` | `TRUE` | Keep 7z. |
| `PHYSFS_ARCHIVE_ISO9660` | `TRUE` | `TRUE` | Keep ISO. |
| `PHYSFS_ARCHIVE_GRP` | `TRUE` | `FALSE` | Build Engine GRP — disable. |
| `PHYSFS_ARCHIVE_WAD` | `TRUE` | `FALSE` | Doom WAD — disable. |
| `PHYSFS_ARCHIVE_HOG` | `TRUE` | `FALSE` | Descent HOG — disable. |
| `PHYSFS_ARCHIVE_MVL` | `TRUE` | `FALSE` | Descent MVL — disable. |
| `PHYSFS_ARCHIVE_QPAK` | `TRUE` | `FALSE` | Quake QPAK — disable. |
| `PHYSFS_ARCHIVE_SLB` | `TRUE` | `FALSE` | I-War SLB — disable. |
| `PHYSFS_ARCHIVE_VDF` | `TRUE` | `FALSE` | Gothic VDF — disable. |

### `reproc`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `REPROC_MULTITHREADED` | `ON` | `ON` | Keep multithreaded. |

**Excluded on `wasm_emscripten`** (no process spawning).

### `sdl3`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `SDL_SHARED` | `ON` | `OFF` | No shared. |
| `SDL_STATIC` | `ON` | `ON` | Static lib. |
| `SDL_TEST_LIBRARY` | `ON` | `OFF` | No test library. |
| `SDL_TESTS` | `OFF` | `OFF` | No tests. |
| `SDL_EXAMPLES` | `OFF` | `OFF` | No examples. |
| `SDL_INSTALL` | `ON` | `ON` | Keep install. |
| `SDL_INSTALL_DOCS` | `ON` | `OFF` | No docs. |
| `SDL_INSTALL_TESTS` | `OFF` | `OFF` | No tests. |
| `SDL_INSTALL_CPACK` | `OFF` | `OFF` | Keep off. |
| `SDL_LEAN_AND_MEAN` | `ON` | `ON` | Reduce size. |
| `SDL_OFFSCREEN` | `ON` | `ON` | Keep offscreen driver. |
| `SDL_OPENGL` | platform | platform | Keep where supported. |
| `SDL_OPENGLES` | platform | platform | Keep where supported. |
| `SDL_VULKAN` | platform | platform | Keep where supported. |
| `SDL_METAL` | `ON` on macOS | `ON` on macOS | Keep on macOS. |
| `SDL_DIRECTX` | `ON` on Windows | `ON` on Windows | Keep on Windows. |
| `SDL_RENDER_D3D` / `D3D11` / `D3D12` | `ON` on Windows | `ON` on Windows | Keep on Windows. |
| `SDL_RENDER_METAL` | `ON` on macOS | `ON` on macOS | Keep on macOS. |
| `SDL_RENDER_VULKAN` | `ON` | `ON` | Keep. |
| `SDL_RENDER_GPU` | `ON` | `ON` | Keep new GPU API. |
| `SDL_X11` | `ON` on Linux | `ON` on Linux | Keep on Linux. |
| `SDL_WAYLAND` | `ON` on Linux | `ON` on Linux | Keep on Linux. |
| `SDL_KMSDRM` | `ON` on Linux | `OFF` | Optional; disable for cleaner build. |
| `SDL_PULSEAUDIO` | `ON` on Linux | `ON` | Keep. |
| `SDL_ALSA` | `ON` on Linux | `ON` | Keep. |
| `SDL_PIPEWIRE` | `ON` on Linux | `ON` | Keep. |
| `SDL_SNDIO` | `ON` on Linux | `OFF` | Optional; disable. |
| `SDL_OSS` | `ON` on Linux | `OFF` | Optional; disable. |
| `SDL_JACK` | `ON` on Linux | `OFF` | Optional; disable. |
| `SDL_WASAPI` | `ON` on Windows | `ON` | Keep on Windows. |
| `SDL_COCOA` | `ON` on macOS | `ON` | Keep on macOS. |
| `SDL_LIBUDEV` | `ON` | `ON` | Keep on Linux. |
| `SDL_HIDAPI` | `ON` | `ON` | Keep HIDAPI. |
| `SDL_HIDAPI_JOYSTICK` | `ON` | `ON` | Keep. |
| `SDL_HIDAPI_LIBUSB` | `ON` | `ON` | Keep on Linux. |
| `SDL_HIDAPI_LIBUSB_SHARED` | `ON` | `OFF` | Static link. |
| `SDL_DBUS` | `ON` on Linux | `ON` | Keep on Linux. |
| `SDL_IBUS` | `ON` on Linux | `ON` | Keep on Linux. |
| `SDL_LIBC` | `ON` | `ON` | Keep libc support. |
| `SDL_GCC_ATOMICS` | auto | auto | Keep. |
| `SDL_ASSEMBLY` | `ON` | `ON` | Keep SIMD assembly. |
| `SDL_SSE` / `SSE2` / `SSE3` / `SSE4_1` / `SSE4_2` / `AVX` / `AVX2` / `AVX512F` | `ON` on x86 | `ON` on x86 | Keep on x86. |
| `SDL_ARMNEON` / `ALTIVEC` / `LSX` / `LASX` | `ON` on arm/ppc/loongarch | `ON` | Keep on supported archs. |
| `SDL_MMX` | `ON` on x86 | `ON` | Keep on x86. |
| `SDL_RPATH` | `ON` | `OFF` | No RPATH for static artifacts. |
| `SDL_FRAMEWORK` | `OFF` | `OFF` | No macOS framework. |
| `SDL_RELOCATABLE` | `ON` | `ON` | Keep. |
| `SDL_LIBICONV` | `OFF` | `OFF` | Keep off. |
| `SDL_SYSTEM_ICONV` | `ON` | `ON` | Keep. |
| `SDL_LIBTHAI` | `OFF` | `OFF` | Keep off. |
| `SDL_FRIBIDI` | `OFF` | `OFF` | Keep off. |
| `SDL_OPENVR` | `OFF` | `OFF` | Keep off. |
| `SDL_VIRTUAL_JOYSTICK` | `OFF` | `OFF` | Keep off. |
| `SDL_DUMMYCAMERA` | `ON` | `ON` | Keep dummy camera. |
| `SDL_DUMMYVIDEO` | `ON` | `ON` | Keep dummy video. |
| `SDL_DISKAUDIO` | `ON` | `ON` | Keep disk audio. |
| `SDL_DUMMYAUDIO` | `ON` | `ON` | Keep dummy audio. |
| `SDL_PTHREADS` | `ON` on Unix | `ON` | Keep. |
| `SDL_PTHREADS_SEM` | `ON` on Unix | `ON` | Keep. |
| `SDL_CLOCK_GETTIME` | `ON` | `ON` | Keep. |
| `SDL_DLOPEN_NOTES` | `ON` | `ON` | Keep. |
| `SDL_DEPS_SHARED` | `ON` | `OFF` | Prefer static system deps. |
| `SDL_WERROR` | `OFF` | `OFF` | Keep off. |
| `SDL_ASAN` | `OFF` | `OFF` | Keep off. |
| `SDL_CCACHE` | `OFF` | `OFF` | Keep off. |
| `SDL_CLANG_TIDY` | `OFF` | `OFF` | Keep off. |
| `SDL_PRESEED` | `OFF` | `OFF` | Keep off. |
| `SDL_VIVANTE` | `OFF` | `OFF` | Keep off. |
| `SDL_ROCKCHIP` | `OFF` | `OFF` | Keep off. |
| `SDL_RPI` | `OFF` | `OFF` | Keep off. |
| `SDL_VIDEO_VITA_PIB` / `PVR` | `OFF` | `OFF` | Keep off. |
| `SDL_WAYLAND_SHARED` / `X11_SHARED` / etc. | `ON` | `OFF` | Static system deps. |
| `SDL_WAYLAND_LIBDECOR` | `ON` | `OFF` | Optional; disable to reduce deps. |
| `SDL_PULSEAUDIO_SHARED` / `ALSA_SHARED` / `PIPEWIRE_SHARED` | `ON` | `OFF` | Static link. |
| `SDL_COCOA_SHARED` / etc. | `ON` | `OFF` | Static link. |
| `SDL_TESTS_LINK_SHARED` | `OFF` | `OFF` | N/A (tests off). |
| `SDL_EXAMPLES_LINK_SHARED` | `OFF` | `OFF` | N/A (examples off). |
| `SDL_UNINSTALL` | `ON` | `OFF` | No uninstall target. |
| `SDL_REVISION` | `` | `` | Use upstream default. |
| `SDL_VENDOR_INFO` | `` | `` | Use upstream default. |

**Emscripten:** SDL3 supports Emscripten. We will enable the Emscripten video/audio backends and disable platform-specific ones (X11, Wayland, DirectX, etc.).

### `sdl3webgpu`

This is a small CMake library.  We will use the upstream `CMakeLists.txt` with `BUILD_SHARED_LIBS=OFF` and no tests/examples.

### `skribidi`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `SKRIBIDI_EXAMPLE` | `ON` | `OFF` | No example. |
| `SKRIBIDI_UNIT_TESTS` | `ON` | `OFF` | No tests. |
| `ENABLE_ASAN` | `OFF` | `OFF` | Keep off. |

### `sqlite-amalgamation`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_SHELL` | `OFF` | `OFF` | No shell executable. |
| `BUILD_SHELL_STATIC` | `ON` | `OFF` | N/A. |
| `SQLITE_ENABLE_MATH_FUNCTIONS` | `ON` | `ON` | Keep math functions. |
| `SQLITE_RECOMMENDED_OPTIONS` | `ON` | `ON` | Keep recommended options. |
| `SQLITE_OMIT_DECLTYPE` | `ON` | `ON` | Keep. |
| `SQLITE_OMIT_JSON` | `OFF` | `OFF` | Keep JSON support. |
| `SQLITE_ENABLE_FTS5` | `OFF` | `OFF` | Optional; keep off unless needed. |
| `SQLITE_ENABLE_RTREE` | `OFF` | `OFF` | Optional; keep off unless needed. |
| `SQLITE_ENABLE_STAT4` | `OFF` | `OFF` | Optional. |
| `SQLITE_ENABLE_GEOPOLY` | `OFF` | `OFF` | Optional. |
| `SQLITE_ENABLE_RBU` | `OFF` | `OFF` | Optional. |
| `SQLITE_ENABLE_DBSTAT_VTAB` | `OFF` | `OFF` | Optional. |
| `SQLITE_ENABLE_COLUMN_METADATA` | `OFF` | `OFF` | Optional. |
| `SQLITE_ENABLE_ICU` | `OFF` | `OFF` | Optional. |
| `SQLITE_USE_URI` | `OFF` | `OFF` | Optional. |
| `BUILD_WITH_XPSDK` | `OFF` | `OFF` | Keep off. |

### `tinycsocket`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `TCS_ENABLE_TESTS` | `OFF` | `OFF` | No tests. |
| `TCS_ENABLE_EXAMPLES` | `OFF` | `OFF` | No examples. |
| `TCS_WARNINGS_AS_ERRORS` | `OFF` | `OFF` | Keep off. |
| `TCS_GENERATE_COVERAGE` | `OFF` | `OFF` | Keep off. |

**Excluded on `wasm_emscripten`**.

### `tracy`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `TRACY_ENABLE` | `ON` | `ON` | Keep profiling enabled. |
| `TRACY_ON_DEMAND` | `OFF` | `OFF` | Keep off. |
| `TRACY_Fortran` | `OFF` | `OFF` | No Fortran. |
| `TRACY_PYTHON` / `TRACY_CLIENT_PYTHON` | `OFF` | `OFF` | No Python client. |
| `TRACY_LTO` | `OFF` | `OFF` | Keep off. |
| `TRACY_CALLSTACK` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_CALLSTACK` | `OFF` | `OFF` | Keep off. |
| `TRACY_ONLY_LOCALHOST` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_BROADCAST` | `OFF` | `OFF` | Keep off. |
| `TRACY_ONLY_IPV4` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_CODE_TRANSFER` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_CONTEXT_SWITCH` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_EXIT` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_SAMPLING` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_VERIFY` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_VSYNC_CAPTURE` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_FRAME_IMAGE` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_SYSTEM_TRACING` | `OFF` | `OFF` | Keep off. |
| `TRACY_PATCHABLE_NOPSLEDS` | `OFF` | `OFF` | Keep off. |
| `TRACY_DELAYED_INIT` | `OFF` | `OFF` | Keep off. |
| `TRACY_MANUAL_LIFETIME` | `OFF` | `OFF` | Keep off. |
| `TRACY_FIBERS` | `OFF` | `OFF` | Keep off. |
| `TRACY_NO_CRASH_HANDLER` | `OFF` | `OFF` | Keep off. |
| `TRACY_TIMER_FALLBACK` | `OFF` | `OFF` | Keep off. |
| `TRACY_LIBUNWIND_BACKTRACE` | `OFF` | `OFF` | Keep off. |
| `TRACY_SYMBOL_OFFLINE_RESOLVE` | `OFF` | `OFF` | Keep off. |
| `TRACY_LIBBACKTRACE_ELF_DYNLOAD_SUPPORT` | `OFF` | `OFF` | Keep off. |
| `TRACY_DEBUGINFOD` | `OFF` | `OFF` | Keep off. |
| `TRACY_IGNORE_MEMORY_FAULTS` | `OFF` | `OFF` | Keep off. |
| `TRACY_VERBOSE` | `OFF` | `OFF` | Keep off. |
| `TRACY_DEMANGLE` | `OFF` | `OFF` | Keep off. |
| `TRACY_ROCPROF_CALIBRATION` | `OFF` | `OFF` | Keep off. |

We will only build the **Tracy client** library (`TracyClient`), not the capture/profiler/import tools.

### `utf8proc`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `UTF8PROC_INSTALL` | `On` | `On` | Keep install. |
| `UTF8PROC_ENABLE_TESTING` | `Off` | `Off` | No tests. |
| `LIB_FUZZING_ENGINE` | `Off` | `Off` | No fuzzing. |

### `zlib`

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `ZLIB_BUILD_STATIC` | `ON` | `ON` | Static lib. |
| `ZLIB_BUILD_SHARED` | `ON` | `OFF` | No shared. |
| `ZLIB_BUILD_TESTING` | `ON` | `OFF` | No tests. |
| `ZLIB_INSTALL` | `ON` | `ON` | Keep install. |
| `ZLIB_PREFIX` | `OFF` | `OFF` | Keep off. |

We will not build `contrib/minizip` unless requested later.

### `lz4` (CMake wrapper in `deps/lz4/build/cmake/`)

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `LZ4_BUILD_CLI` | `ON` | `OFF` | No executable. |
| `LZ4_POSITION_INDEPENDENT_LIB` | `ON` | `ON` | Keep PIC. |
| `BUILD_SHARED_LIBS` | `ON` | `OFF` | Static. |
| `BUILD_STATIC_LIBS` | `OFF` | `ON` | Static lib. |

We will create a wrapper in `src/lz4/` or use `add_subdirectory(deps/lz4/build/cmake)` with `EXCLUDE_FROM_ALL`.

### `xxhash` (CMake wrapper in `deps/xxhash/cmake_unofficial/`)

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | `OFF` | Static. |
| `XXHASH_BUILD_XXHSUM` | `ON` | `OFF` | No executable. |
| `XXHASH_BUNDLED_MODE` | `OFF` | `OFF` | Keep off for installable artifact. |

We will create a wrapper in `src/xxhash/` or use `add_subdirectory(deps/xxhash/cmake_unofficial)`.

### `zstd` (CMake wrapper in `deps/zstd/build/cmake/`)

| Option | Upstream default | Proposed default | Notes |
|---|---|---|---|
| `ZSTD_BUILD_STATIC` | `ON` | `ON` | Static lib. |
| `ZSTD_BUILD_SHARED` | `ON` | `OFF` | No shared. |
| `ZSTD_BUILD_COMPRESSION` | `ON` | `ON` | Keep compression. |
| `ZSTD_BUILD_DECOMPRESSION` | `ON` | `ON` | Keep decompression. |
| `ZSTD_BUILD_DICTBUILDER` | `ON` | `OFF` | Optional; disable unless dict building needed. |
| `ZSTD_BUILD_DEPRECATED` | `OFF` | `OFF` | Keep off. |
| `ZSTD_BUILD_PROGRAMS` | `ON` | `OFF` | No executables. |
| `ZSTD_BUILD_CONTRIB` | `OFF` | `OFF` | Keep off. |
| `ZSTD_LEGACY_SUPPORT` | `ON` | `OFF` | Disable legacy format support for smaller build. |
| `ZSTD_LEGACY_LEVEL` | `5` | `N/A` | N/A if legacy off. |
| `ZSTD_USE_STATIC_RUNTIME` | `OFF` | `OFF` | Keep dynamic CRT on Windows. |
| `ZSTD_PROGRAMS_LINK_SHARED` | `OFF` | `OFF` | N/A. |
| `ZSTD_ZLIB_SUPPORT` | `OFF` | `OFF` | Keep off. |
| `ZSTD_LZMA_SUPPORT` | `OFF` | `OFF` | Keep off. |
| `ZSTD_LZ4_SUPPORT` | `OFF` | `OFF` | Keep off. |
| `ZSTD_FRAMEWORK` | `OFF` | `OFF` | No Apple framework. |

We will create a wrapper in `src/zstd/` or use `add_subdirectory(deps/zstd/build/cmake)` with `EXCLUDE_FROM_ALL`.

---

## Header-only / non-CMake dependencies (wrapper defaults)

These dependencies do not have native CMake builds (or the native build is unsuitable).  We will create minimal `src/<dep>/CMakeLists.txt` wrappers that compile the implementation files into a static library.  Their "options" are mostly the `#define IMPLEMENTATION` macros.

| Dep | Implementation macro(s) | Notes |
|---|---|---|
| `cgltf` | `CGLTF_IMPLEMENTATION` | glTF loader. |
| `cimgui` | N/A (wrapper compiles `cimgui.cpp` + ImGui sources) | Upstream hard-codes `SHARED`; wrapper builds static. |
| `FastNoiseLite` | `FNL_IMPLEMENTATION` | C noise library. |
| `fontstash` | `FONTSTASH_IMPLEMENTATION` | Font rasterization. |
| `microui` | `MUI_IMPLEMENTATION` | Single-source UI. |
| `miniaudio` (fallback) | `MINIAUDIO_IMPLEMENTATION` | Only if we use the wrapper instead of upstream CMake. |
| `minigamepad` | `MGP_IMPLEMENTATION` | Gamepad abstraction. |
| `mtcc` | N/A (Makefile wrapper) | `src/mtcc/CMakeLists.txt` runs configure + make. |
| `nanovg` | `NANOVG_IMPLEMENTATION` | 2D vector graphics. |
| `raudio` | `RAUDIO_IMPLEMENTATION` | Audio library. |
| `sokol` | `SOKOL_IMPL` plus per-module macros | **Per-module static libraries:** `sokol_app`, `sokol_args`, `sokol_audio`, `sokol_fetch`, `sokol_gfx`, `sokol_glue`, `sokol_log`, `sokol_time`. `sokol_app`/`sokol_gfx`/`sokol_glue` use Metal on macOS. |
| `sokol_gp` | `SOKOL_GP_IMPL` | Built against the Sokol headers in `deps/sokol_gp/thirdparty`. |
| `stb` | `STB_IMAGE_IMPLEMENTATION`, `STB_TRUETYPE_IMPLEMENTATION`, etc. | **Per-module static libraries:** `stb_image`, `stb_image_write`, `stb_image_resize`, `stb_truetype`, `stb_rect_pack`, `stb_ds`. |
| `tinycsocket` | N/A (CMake wrapper) | Upstream writes into its source tree; wrapper copies to build tree first. |
| `ubench` | `UBENCH_IMPLEMENTATION` | On Emscripten the wrapper includes `emscripten/html5.h` before `ubench.h`. |
| `utest` | `UTEST_IMPLEMENTATION` | Unit testing. |

## Non-CMake / native build dependencies

| Dep | Build system | Wrapper / notes |
|---|---|---|
| `lua-5.5.0` | Makefile | `src/lua/CMakeLists.txt` drives the upstream `make` rules and installs `liblua.a` + headers. |
| `mtcc` | Makefile | `src/mtcc/CMakeLists.txt` runs `./configure` and `make libtcc.a` in the build tree. **Excluded on `wasm_emscripten`**. |

---

## Open decisions / resolved items

1. **HarfBuzz ↔ FreeType integration:** **Enabled** `HB_HAVE_FREETYPE=ON` and `FT_DISABLE_HARFBUZZ=OFF`. FreeType is built before HarfBuzz.
2. **Dawn Emscripten path:** Resolved. `DAWN_ENABLE_INSTALL=OFF` and `DAWN_BUILD_MONOLITHIC_LIBRARY=OFF` on Emscripten; `scripts/install_dawn.cmake` stages the `emdawnwebgpu` artifacts.
3. **Sokol / STB module granularity:** Resolved. Each module is a separate static library (`sokol_app`, `sokol_gfx`, `stb_image`, etc.).
4. **TLS backend:** Resolved. BoringSSL is used everywhere for `curl` and `libwebsockets`.
5. **Submodules:** All floating submodules are pinned by removing `branch = ...` from `.gitmodules` and committing the resolved submodule commits. `cimgui/imgui` is a nested submodule that must be initialized with `git submodule update --init --recursive`.
6. **Remaining platforms:** `linux_x64`, `linux_arm64`, `windows_x64`, and `windows_arm64` toolchains are present but not yet validated locally.

