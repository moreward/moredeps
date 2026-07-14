# moredeps Build Infrastructure — Working Log

**Started:** 2026-07-12  
**Goal:** Implement the production-grade static-library build system described in `docs/build_plan.md`.

---

## 2026-07-14 — CI green on all platforms; caching, packaging, and site overhauled

Full CI pipeline (`build.yml` → `release` → `deploy-site`) is now green for all 6 targets. Warm-cache timings: linux ~7 min, macOS ~7.5 min, wasm ~10 min, windows_x64 ~16 min, windows_arm64 ~23 min (cold: 30–70 min).

### Caching fixes

- **Windows sccache was never active**: the workflow installed sccache and saved/restored its directory, but never set `CMAKE_C(XX)_COMPILER_LAUNCHER=sccache`, so every Windows build compiled cold. Fixed for both Windows jobs.
- **Windows arm64 used the emulated x64 cross-compiler**: host detection relied on `PROCESSOR_ARCHITECTURE`, but git-bash runs x64-emulated on ARM runners and reports `AMD64`. Now uses `RUNNER_ARCH` → native `vcvarsall arm64` toolset (fallback to `x64_arm64` if unavailable). boringssl alone went 48 min → cached.
- Removed the broken `deps/` submodule cache (commit `21c379d`); fresh `git submodule update --init` (~3 min) is reliable.

### Packaging fixes (`ci_package.py`)

- **Dep commit SHAs were the repo SHA** (`git rev-parse HEAD` inside an empty submodule dir walks up to the superproject) — hence double-SHA artifact names. Now uses `git ls-tree` on the superproject; works with `submodules: false`.
- **Windows zips were header-only for several deps**: MSVC library names (`SDL3-static.lib`, `physfs-static.lib`, `zs.lib`, `utf8proc_static.lib`, `websockets_static.lib`, `zstd_static.lib`, `libunibreak.lib`) didn't match `DEP_LIBRARY_NAMES`. Lib matching now tries stems with and without the `lib` prefix plus explicit variants. sdl3/libunibreak now ship Windows builds.
- **lua was missing everywhere** (dir `lua-5.5.0` vs key `lua`); deps are now enumerated from the curated list (drops junk rows `dawn_third_party`, `imgui`), with `DIR_ALIAS` for the dir mapping.
- **Five deps shipped no headers on any platform** (boringssl `include/openssl/`, freetype `include/freetype2/`, skribidi `skb_*.h`, budouxc `budoux.h`, libunibreak `linebreak.h` etc.) — header matcher now covers those.
- dawn on wasm marked excluded (browser provides WebGPU; the headers-only zip was misleading).
- Manifest entries now include `repo_url` (from `.gitmodules`).
- Verified end-to-end: 268 built zips, 0 without libs, 0 without headers.

### Download site

- **CORS fix:** browsers can't fetch release assets cross-origin (302 to `release-assets.githubusercontent.com` without CORS headers). The manifest is now served same-origin from Pages: new `deploy-site` job redeploys the site with the fresh manifest after each release; `pages.yml` also fetches it on docs pushes.
- Dep names link to `<upstream repo>/tree/<pinned commit>`.
- Older `build-<sha>` releases show a plain asset list.
- Multi-download replaced: scripted multi-downloads are silently browser-blocked, so the site now lists `curl -fLO <url>` commands for the selection in a copyable textarea, with per-dependency (row) and per-platform (column) select-all checkboxes.

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

## 2026-07-12 (later) — Emscripten validated and macOS missing deps completed

### Emscripten (wasm32) full-build results

**Command:**
```bash
./scripts/build_all.sh wasm_emscripten
```

**Result:** Build completed successfully. 60 static libraries installed in `_out/wasm_emscripten/lib/`. Notable exclusions on this target:
- `glfw` — no desktop windowing on the web.
- `mtcc` — target-specific C/ASM cannot compile to WASM.
- `sdl3webgpu` — `WGPUSurfaceDescriptorFromCanvasHTMLSelector` is incompatible with the current emdawnwebgpu headers.
- `lua` — not wired for Emscripten yet.
- `libwebgpu_dawn.a` — not produced; instead `emdawnwebgpu` headers/JS files are staged by `scripts/install_dawn.cmake`.

Fixes applied during Emscripten validation:
- `src/ubench/ubench.c` includes `emscripten/html5.h` before `ubench.h` to provide `emscripten_performance_now`.
- `OPENSSL_NO_ASM=ON` for BoringSSL on Emscripten to avoid `-Wa,-g` assembler errors.
- `CMAKE_C_STANDARD=11` and `CMAKE_CXX_STANDARD=17` added to common CMake args to fix cJSON C99 errors.
- `CMAKE_FIND_ROOT_PATH` set to `CMAKE_INSTALL_PREFIX` so cross-builds can find installed deps.

### macOS arm64 — missing deps completed

Added and validated:
- `cimgui` — wrapper in `src/cimgui/` builds a static library from the cimgui/ImGui sources.
- `raylib` — built via `ExternalProject_Add` with `BUILD_EXAMPLES=OFF`, `BUILD_SHARED_LIBS=OFF`; on Emscripten uses `PLATFORM=Web`.
- `mtcc` — wrapper in `src/mtcc/` runs `./configure` and `make libtcc.a` in the build tree.

The macOS arm64 build now produces 66 static libraries.

### Clean submodule handling

- `skribidi` — upstream sets `CMAKE_COMPILE_WARNING_AS_ERROR ON` globally and would dirty the submodule during build. We now build it from a copy of the source in the build tree (`_b/<platform>/skribidi_src_copy`) and apply a Python patch only to the copy, leaving the submodule clean.
- `tinycsocket` — upstream writes generated version headers back into the source tree. We now copy the source into the build tree in `src/tinycsocket/` before invoking the upstream build, leaving the submodule clean.
- `deps/skribidi` and `deps/tinycsocket` remain unmodified after a full build.

### Deferred

- `ghostty` — still cannot be constrained to emit only `libghostty.a`. The default `zig build` install step tries to copy the macOS app bundle even with `-Dapp-runtime=none`. Deferred until upstream supports a library-only build.

### Updated docs

- `docs/build_plan.md` updated to reflect the implemented `ExternalProject_Add` super-build design, the `deps/dawn_third_party/` layout, and the new/updated wrappers.
- `docs/build_options.md` updated with actual defaults (Dawn `DAWN_FETCH_DEPENDENCIES=OFF`, curl options, libwebsockets BoringSSL feature-detection flags, etc.).
- `README.md` status updated to show both macOS arm64 and Emscripten as validated.

### Next steps
- Validate `linux_x64` and `linux_arm64` builds.
- Validate `windows_x64` and `windows_arm64` builds; fix the `libwebsockets` BoringSSL library-name issue for MSVC (`.lib` vs `.a`).
- Commit and push the current state to origin.

---

## 2026-07-12 (latest) — Skribidi deps pinned, Emscripten exclusions, build scripts

### Skribidi reproducibility fix

- Added `deps/SheenBidi`, `deps/libunibreak`, `deps/budouxc`, and `deps/imgui` as top-level submodules, all pinned to the commits expected by the current `skribidi`/`cimgui` revisions.
- Updated `deps/harfbuzz` to `11.0.0` (matching what `skribidi` upstream expects).
- Created `src/libunibreak/` and `src/budouxc/` wrappers to build these deps with correct CMake install rules.
- Replaced the `skribidi` build-tree-copy + Python patch approach with a clean `src/skribidi/` wrapper that depends on `harfbuzz`, `SheenBidi`, `libunibreak`, and `budouxc`.
- Removed `scripts/patch_skribidi.py` since it is no longer needed.
- Result: `skribidi` no longer fetches anything at build time; it is fully air-gap compatible.

### cimgui / imgui

- `cimgui` wrapper now uses the top-level `deps/imgui` submodule instead of the nested `cimgui/imgui` submodule.
- `README.md` updated to note that `--recursive` is no longer required.

### Emscripten exclusions

- Gated `enet`, `libwebsockets`, `reproc`, and `tinycsocket` on `if(NOT EMSCRIPTEN)` in `CMakeLists.txt` because they rely on platform APIs not available in the browser.
- Documented these exclusions in `docs/build_plan.md` and `docs/build_options.md`.
- Verified the full `wasm_emscripten` clean build still succeeds; artifacts now exclude the four network/process libraries.

### raylib on Emscripten

- Validated that `raylib` builds with `PLATFORM=Web` on Emscripten and that a downstream test program links successfully with `-sUSE_GLFW=3`.
- Documented the `-sUSE_GLFW=3` requirement in `docs/build_options.md`.

### Build scripts

- Created `scripts/clean_all.sh` to remove `_b/` and `_out/`.
- Created `scripts/validate_dev_env.sh` to check for required tools per platform and print actionable guidance when something is missing.
- Updated `scripts/build_all.sh` to call `validate_dev_env.sh` and to select the right CMake generator on Windows (Ninja, JOM, or NMake Makefiles).

### Linux cross-compilation

- Updated `toolchain/linux_arm64.cmake` to auto-detect the cross compiler's sysroot via `gcc -print-sysroot` and set `CMAKE_SYSROOT` when a real sysroot is reported.

### Documentation

- Added `docs/build_plan.md` section 3.6 describing Windows build machine requirements (Visual Studio 2022, Ninja, etc.).
- Added `budouxc`, `SheenBidi`, `libunibreak`, and `raylib` sections to `docs/build_options.md`.
- Updated `skribidi`, `curl`, and `tinycsocket` notes in `docs/build_options.md`.
- Updated `README.md` to list the new scripts and the updated submodule layout.

### Validation

- Full clean `macos_arm64` build succeeded (67 static libraries).
- Full clean `wasm_emscripten` build succeeded (56 static libraries, excluding the newly gated network/process deps).
- All submodules remained clean after the builds.

- Discovered that `deps/sokol_gp/thirdparty/` vendored `sokol` headers at commit `ce91d77` (Dec 2024) while the top-level `deps/sokol` submodule was at a July 2026 commit.
- Pinned `deps/sokol` to `ce91d77f93917bfbf772146c7e2538d71c0113a4` so that `sokol_gfx`, `sokol_app`, `sokol_glue`, and `sokol_gp` use the same `sokol` ABI.
- Updated `src/sokol_gp/CMakeLists.txt` to prefer the top-level `deps/sokol` includes over the vendored copies.
- Verified the sokol-related targets still build on macOS arm64 after the pin.
- Validate `linux_x64` / `linux_arm64` on a Linux host.
- Validate `windows_x64` / `windows_arm64` on the arm64 Windows VM.

- Added `src/ghostty/CMakeLists.txt` wrapper:
  - Copies `deps/ghostty` to the build tree to keep the submodule clean.
  - Runs `zig build -Doptimize=ReleaseFast -Dapp-runtime=none -Demit-macos-app=false -Demit-xcframework=true`.
  - Extracts `libghostty.a` from the resulting xcframework and installs it along with `include/ghostty.h` and `include/ghostty/`.
- Added `moredeps_add_dep(ghostty ...)` gated to `MOREDEPS_PLATFORM == "macos_arm64"`.
- Full clean macOS arm64 rebuild succeeded; now produces 67 static libraries.
- Updated `docs/build_plan.md`, `docs/build_options.md`, `README.md`, and `docs/work_log.md` to reflect the new ghostty status.
- Fixed `docs/build_options.md` libwebsockets option name: `LWS_CLIENT_HTTP_PROXYING` (not `LWS_WITH_CLIENT_HTTP_PROXYING`).

## 2026-07-13 — Emscripten: re-enable lua and sdl3webgpu

- Removed the `wasm_emscripten` exclusion for `lua` in `CMakeLists.txt`; `liblua.a` now builds for Emscripten.
- Removed the `wasm_emscripten` exclusion for `sdl3webgpu` and added `WEBGPU_BACKEND_EMDAWNWEBGPU` compile definition.
- Added `patches/sdl3webgpu_emdawn_stringview.patch` and updated `src/sdl3webgpu/CMakeLists.txt` to copy the source to the build tree and apply the patch on Emscripten, so the submodule stays clean.
- The patch updates `WGPUSurfaceDescriptorFromCanvasHTMLSelector.selector` and `.label` to use `WGPUStringView`.
- Verified full `wasm_emscripten` rebuild succeeds with 58 static libraries.
- Verified `sdl3webgpu` still builds on `macos_arm64`.
- Updated `docs/build_plan.md` and `docs/build_options.md`.

## 2026-07-13 (later) — Sokol backend variants

- Implemented Option C for Sokol backend variants using `src/sokol_variant/` templates and a `moredeps_sokol_variant()` helper.
- Per-platform variants:
  - macOS: `*_glcore`, `*_metal`
  - Linux: `*_glcore`, `*_gles3`
  - Windows: `*_glcore`, `*_gles3`, `*_d3d11`
  - Emscripten: `*_gles3`
- Default `sokol_app`/`sokol_gfx`/`sokol_glue`/`sokol_gp` targets still use the platform-specific backend.
- **WGPU variants deferred:** the pinned `sokol` (matching `sokol_gp`) uses an older `webgpu.h` API that is incompatible with the current `dawn` submodule.
- Verified full `macos_arm64` build (75 static libraries) and full `wasm_emscripten` build (62 static libraries) with the new variants.

## 2026-07-13 (latest) — Update sokol to latest, enable WGPU variants, isolate sokol_gp

- Updated `deps/sokol` to the latest commit (`3743ea6`, 2026-07-08) so that `sokol_gfx`/`sokol_app`/`sokol_glue` match the current `deps/dawn` `webgpu.h` API.
- Kept `deps/sokol_gp` at `f6c9639` (it does not compile against latest sokol).
- Built `sokol_gp` against the vendored sokol headers in `deps/sokol_gp/thirdparty`; headers are now installed to `include/sokol_gp/`.
- Added `src/sokol_variant/sokol_gp_compat_note.txt` and install it as `include/sokol_gp/README.txt` to warn that `sokol_gp` must not be mixed with top-level `sokol_*` libraries.
- Re-enabled WGPU variants:
  - `sokol_gfx_wgpu` on all platforms
  - `sokol_app_wgpu` and `sokol_glue_wgpu` only on Emscripten (sokol_app does not support WGPU on desktop)
- `sokol_gp_wgpu` remains unavailable because the vendored sokol headers predate the current Dawn API.
- Verified full `macos_arm64` build (76 static libraries) and full `wasm_emscripten` build (65 static libraries).
- Updated `docs/build_plan.md`, `docs/build_options.md`, and `docs/work_log.md`.

### Next steps
- Apply any further user comments before moving to Linux/Windows validation.
- Validate `linux_x64` / `linux_arm64` on a Linux host.
- Validate `windows_x64` / `windows_arm64` on the arm64 Windows VM.
- Re-enable `sokol_gp_wgpu` once upstream `sokol_gp` updates its vendored sokol headers, or patch `sokol_gp` to match current `sokol`/`dawn`.
- Validate `linux_x64` / `linux_arm64` on a Linux host.
- Validate `windows_x64` / `windows_arm64` on the arm64 Windows VM.
- Re-enable WGPU Sokol variants once Sokol/Sokol_GP/Dawn API versions align.

## 2026-07-13 (latest) — Linux arm64 build green

- Tested `linux_arm64` build on the `ubudev` VM (aarch64 Linux).
- Fixed GLFW Wayland requirement by adding `-DGLFW_BUILD_WAYLAND=OFF`.
- Reduced Dawn Linux deps by adding `-DDAWN_USE_WAYLAND=OFF` (still requires X11).
- Installed required system dev packages on the VM:
  - `libx11-dev`, `libx11-xcb-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libxext-dev`
  - `libxss-dev`, `libxtst-dev`, `libxkbcommon-dev`
  - `libasound2-dev`, `libgl1-mesa-dev`, `libvulkan-dev`
- Full `linux_arm64` build succeeded with **76 static libraries**.
- Updated `scripts/validate_dev_env.sh` and `docs/build_plan.md` with Linux package requirements.

---

## 2026-07-13 — Windows x64 and arm64 builds validated

### Windows x64

- Built on an arm64 Windows VM using the x64 Native Tools Command Prompt.
- Added `scripts/setup_vcvars.sh` to auto-detect Visual Studio via `vswhere.exe` and run `vcvarsall.bat` for the correct architecture, so builds work from any shell (cmd, PowerShell, Git Bash).
- Fixed cJSON `/Za` conflict with MSVC by setting `ENABLE_CUSTOM_COMPILER_FLAGS=OFF` on Windows.
- Fixed `fontstash` and `minigamepad` wrappers to include `windows.h` with `NOMINMAX` and `WIN32_LEAN_AND_MEAN` to avoid macro pollution.
- Fixed `cimgui` `IMGUI_IMPL_API` definition for MSVC (multi-token `extern "C" __declspec(dllexport)` does not survive CMake's compile-definition handling); now uses `CIMGUI_NO_EXPORT` and `IMGUI_IMPL_API=` for static builds.
- Fixed libwebsockets `/WX` (warnings-as-errors) on MSVC by creating `src/libwebsockets/` wrapper that copies upstream source and applies `patches/libwebsockets_disable_wx.patch`.
- Full `windows_x64` build succeeded.

### Windows arm64

- Built on the same arm64 Windows VM using the arm64 Native Tools Command Prompt (cross-compiled from x64 host also works via `setup_vcvars.sh`).
- **BoringSSL:** disabled ASM on Windows ARM64 (`OPENSSL_NO_ASM=ON`) because MSVC ARM64 does not support BoringSSL's assembly files.
- **mtcc:** excluded on Windows ARM64 because TinyCC's PE backend (`tccpe.c`) does not support the ARM64 architecture. The `CMakeLists.txt` now gates mtcc with `NOT (WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")`.
- **Sokol variants:** removed `gfx:GLES3` and `gp:GLES3` from the Windows variant list (OpenGL ES is not typically used on Windows desktop).
- Full `windows_arm64` build succeeded (all deps except mtcc).

### Updated docs

- Updated `README.md` with a "Quick start" section showing `git submodule update --init --jobs 4` (explicitly noting **do not use `--recursive`**).
- Updated status table in `README.md` and `docs/build_plan.md` to show all validated platforms.
- Added Windows-specific notes to `docs/build_options.md` for BoringSSL and cJSON.
- Updated `docs/build_plan.md` Windows section to describe the auto-detected MSVC environment via `setup_vcvars.sh`.

### Next steps
- Validate `linux_x64` on a Linux host.
- Set up CI (GitHub Actions) with caching and incremental builds.
- Implement the manifest JSON + GitHub Releases + GitHub Pages workflow described in `docs/build_plan.md` section 5.
