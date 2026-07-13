# moredeps Build Infrastructure — Working Log

**Started:** 2026-07-12  
**Goal:** Implement the production-grade static-library build system described in `docs/build_plan.md`.

---

## 2026-07-12 — Phase 2 implementation

### What was done before this session
- Plan and build-options documents were reviewed and approved.
- `openssl` submodule was replaced with `boringssl`.
- All doc changes were pushed to `main` by the user.

### This session's work
- Created this working log.
- Created `toolchain/` files for `macos_arm64`, `linux_x64`, `linux_arm64`, `windows_x64`, `windows_arm64`, and `wasm_emscripten`.
- Created top-level `CMakeLists.txt` using `ExternalProject_Add` to build each dependency in isolation and stage installs to `_out/<platform>`.
- Created `scripts/build_all.sh` to configure and build a chosen platform (or all platforms).
- Created `src/<dep>/` CMake wrappers for header-only / non-CMake dependencies.
- Validated the architecture on `macos_arm64` by successfully building many deps.

### macOS arm64 full-build results

**Command:**
```bash
cmake -S . -B _b/macos_arm64 -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain/macos_arm64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build _b/macos_arm64 --parallel 2 -- --keep-going
```

**Successfully installed libraries (`_out/macos_arm64/lib/`):**
`libbox3d.a`, `libbudouxc.a`, `libcglm.a`, `libcgltf.a`, `libcjson.a`, `libcrypto.a`, `libcurl.a`, `libenet.a`, `libFastNoiseLite.a`, `libflecs_static.a`, `libfontstash.a`, `libfreetype.a`, `libglfw3.a`, `libharfbuzz-subset.a`, `libharfbuzz.a`, `liblibunibreak.a`, `liblua.a`, `liblz4.a`, `libmicroui.a`, `libmimalloc.a`, `libminiaudio_channel_combiner_node.a`, `libminiaudio_channel_separator_node.a`, `libminiaudio_libvorbis.a`, `libminiaudio_ltrim_node.a`, `libminiaudio_reverb_node.a`, `libminiaudio_vocoder_node.a`, `libminiaudio.a`, `libminigamepad.a`, `libnanovg.a`, `libphysfs.a`, `libraudio.a`, `libreproc.a`, `libSDL3.a`, `libSheenBidi.a`, `libskribidi.a`, `libsokol_app.a`, `libsokol_args.a`, `libsokol_audio.a`, `libsokol_fetch.a`, `libsokol_gfx.a`, `libsokol_log.a`, `libsokol_time.a`, `libsqlite3.a`, `libssl.a`, `libstb_ds.a`, `libstb_image_resize.a`, `libstb_image_write.a`, `libstb_image.a`, `libstb_rect_pack.a`, `libstb_truetype.a`, `libTracyClient.a`, `libubench.a`, `libutest.a`, `libutf8proc.a`, `libxxhash.a`, `libz.a`, `libzstd.a`.

**Failing targets (5):**
1. **dawn** — Python 3.14 incompatibility in `tools/fetch_dawn_dependencies.py` (`NameError: name 'Str' is not defined`).
2. **libwebsockets** — BoringSSL API incompatibility: libwebsockets' OpenSSL code uses `RSA` internal fields (`ctx->rsa->p`, `ctx->rsa->q`) that BoringSSL keeps opaque.
3. **tinycsocket** — macOS POSIX include-order issues (`INADDR_LOOPBACK`, `IP_ADD_MEMBERSHIP`, `IFF_UP`, `struct ip_mreq`, `u_short`).
4. **sokol_glue** — API mismatch with current `sokol` submodule (`SAPP_PIXELFORMAT_*`, `SG_PIXELFORMAT_*` constants not defined).
5. **sokol_gp** — API mismatch with current `sokol` submodule (`sg_shader_desc.images`, `image_sampler_pairs`, `sg_buffer_desc.type`, `SG_USAGE_STREAM`, etc.).

### Notes / decisions made during implementation
- `curl` options were tuned to avoid pulling in system libraries (`libpsl`, `libssh2`, `brotli`, `zstd`) and to disable HTTP/2/QUIC for now.
- Lua's native Makefile was wrapped in `src/lua/CMakeLists.txt` instead of trying to use CMake directly in the upstream source tree.
- `sokol_app` and `sokol_gfx` are compiled as Objective-C on macOS with `SOKOL_METAL`.
- `libwebsockets` now passes `LWS_OPENSSL_INCLUDE_DIRS` and `LWS_OPENSSL_LIBRARIES` pointing to the BoringSSL install prefix, and `DISABLE_WERROR=ON`.

### Blockers / issues
1. **Dawn:** needs a Python version compatible with its fetch script (not 3.14) or a different dependency-fetch strategy.
2. **libwebsockets + BoringSSL:** needs either a libwebsockets/BoringSSL revision pair that is compatible, or a patch to libwebsockets to avoid OpenSSL `RSA` internal access.
3. **tinycsocket:** needs upstream fix for macOS POSIX includes or platform exclusion.
4. **sokol_glue / sokol_gp:** need `sokol` and `sokol_gp` pinned to mutually compatible revisions.

### Next steps
- Decide on pinning/compat strategy for Dawn, libwebsockets/BoringSSL, tinycsocket, and sokol family.
- Validate `wasm_emscripten` build.
- Validate Linux and Windows builds on separate VMs.
- Pin floating submodules to concrete tags/commits.
- Commit current implementation to a feature branch.

