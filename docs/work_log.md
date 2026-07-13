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
cmake --build _b/macos_arm64 --parallel 2
```

**Result:** All targets build successfully. Installed static libraries in `_out/macos_arm64/lib/`:
`libbox3d.a`, `libbudouxc.a`, `libcglm.a`, `libcgltf.a`, `libcjson.a`, `libcrypto.a`, `libcurl.a`, `libenet.a`, `libFastNoiseLite.a`, `libflecs_static.a`, `libfontstash.a`, `libfreetype.a`, `libglfw3.a`, `libharfbuzz-subset.a`, `libharfbuzz.a`, `liblibunibreak.a`, `liblua.a`, `liblz4.a`, `libmicroui.a`, `libmimalloc.a`, `libminiaudio_channel_combiner_node.a`, `libminiaudio_channel_separator_node.a`, `libminiaudio_libvorbis.a`, `libminiaudio_ltrim_node.a`, `libminiaudio_reverb_node.a`, `libminiaudio_vocoder_node.a`, `libminiaudio.a`, `libminigamepad.a`, `libnanovg.a`, `libphysfs.a`, `libraudio.a`, `libreproc.a`, `libSDL3.a`, `libsdl3webgpu.a`, `libSheenBidi.a`, `libskribidi.a`, `libsokol_app.a`, `libsokol_args.a`, `libsokol_audio.a`, `libsokol_fetch.a`, `libsokol_gfx.a`, `libsokol_glue.a`, `libsokol_gp.a`, `libsokol_log.a`, `libsokol_time.a`, `libsqlite3.a`, `libssl.a`, `libstb_ds.a`, `libstb_image_resize.a`, `libstb_image_write.a`, `libstb_image.a`, `libstb_rect_pack.a`, `libstb_truetype.a`, `libtinycsocket.a`, `libTracyClient.a`, `libubench.a`, `libutest.a`, `libutf8proc.a`, `libwebgpu_dawn.a`, `libwebsockets.a`, `libxxhash.a`, `libz.a`, `libzstd.a`.

### Notes / decisions made during implementation
- `curl` options were tuned to avoid pulling in system libraries (`libpsl`, `libssh2`, `brotli`, `zstd`) and to disable HTTP/2/QUIC for now.
- Lua's native Makefile was wrapped in `src/lua/CMakeLists.txt` instead of trying to use CMake directly in the upstream source tree.
- `sokol_app`, `sokol_gfx`, and `sokol_glue` are compiled as Objective-C on macOS with `SOKOL_METAL`.
- `sokol_gp` is built against the vendored Sokol headers shipped in `deps/sokol_gp/thirdparty` to avoid a version mismatch with the top-level `sokol` submodule.
- `libwebsockets` was made to build against BoringSSL by forcing CMake feature-detection flags that BoringSSL's `CHECK_FUNCTION_EXISTS` misses (`LWS_HAVE_RSA_SET0_KEY`, `LWS_HAVE_ECDSA_SIG_get0`, `LWS_HAVE_ECDSA_SIG_set0`, `LWS_HAVE_BN_bn2binpad`).
- `tinycsocket` was updated to the latest master revision (which adds macOS support) and wrapped in `src/tinycsocket/` to add install rules that upstream lacks.
- `sdl3webgpu` was updated to the latest master revision and wrapped in `src/sdl3webgpu/` to add a `cmake_minimum_required` and avoid linking against non-existent targets in the isolated ExternalProject build.
- **Dawn:** after discovering that Python 3.11 did not fix `fetch_dawn_dependencies.py` (the script is missing a `Str` mock class), we switched to the submodule approach. Because Git cannot nest submodules inside `deps/dawn`, Dawn's third-party dependencies live in `deps/dawn_third_party/` and are wired to Dawn via `DAWN_THIRD_PARTY_DIR`. `DAWN_FETCH_DEPENDENCIES=OFF` disables the network-dependent fetch script. Vulkan is enabled only on Linux; Metal is enabled only on macOS.

### Blockers / issues
- None currently on `macos_arm64`; all targets build and install.

### Next steps
- Pin all floating submodules to concrete tags/commits (tinycsocket, sdl3webgpu, sokol, sokol_gp, and any others still on floating branches).
- Validate `wasm_emscripten` build.
- Validate Linux and Windows builds on separate VMs.
- Update `docs/build_plan.md` and `docs/build_options.md` to reflect the Dawn submodule layout and the BoringSSL/libwebsockets feature-detection workaround.
- Commit and push the current state.

---

## 2026-07-12 (later still) — Submodules pinned

- Removed all `branch = ...` entries from `.gitmodules`. Submodules now stay at their recorded SHA and will not drift when `git submodule update --remote` is run.
- All submodule commits match the revisions validated by the macos_arm64 build.
- `tinycsocket` submodule was cleaned of build-generated files in its source tree before the pin.

### Next steps
- Full review of the build system and docs.
- Validate `wasm_emscripten` build.
- Validate Linux and Windows builds on separate VMs.
- Push to origin.


