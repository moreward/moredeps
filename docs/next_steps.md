# Next Steps

## 1. Mobile targets (priority)

Build and test static libraries for iOS and Android. Already sketched in
`docs/mobile_plan.md`.

### 1a. iOS (arm64 + simulator arm64 / x64)

- macOS host only (Xcode required).
- Toolchain: `toolchain/ios_arm64.cmake` + `toolchain/ios_simulator.cmake`
  using `CMAKE_SYSTEM_NAME=iOS` and `CMAKE_OSX_SYSROOT`.
- CI: `macos-15` runner with Xcode pre-installed.
- Exclusions: deps that require desktop OS APIs (same set as macOS or
  slightly larger — no GLFW, no Vulkan, etc.).

### 1b. Android (arm64 + x64)

- Host: Linux or macOS with Android NDK installed.
- Toolchain: `toolchain/android_arm64.cmake` and
  `toolchain/android_x64.cmake` using `CMAKE_SYSTEM_NAME=Android` and
  `CMAKE_ANDROID_NDK`.
- CI: `ubuntu-24.04-arm` runner + NDK download/setup step.
- Exclusions: similar to WASM (no desktop GL, no process APIs for some deps);
  `reproc`, `tinycsocket`, `libwebsockets` may need evaluation.
- Android adds `liblog`, `libandroid`, `libEGL`, `libGLESv3` as implicit
  system libs.

### 1c. Work required

- [ ] Write toolchain files (`ios_arm64.cmake`, `ios_simulator_arm64.cmake`,
      `android_arm64.cmake`, `android_x64.cmake`).
- [ ] Audit deps for mobile compatibility (build each dep, note failures).
- [ ] Add `EXCLUDED` entries to `ci_package.py` for deps that can't build.
- [ ] Add CI jobs to `.github/workflows/build.yml` for each new platform.
- [ ] Add test toolchain support in `tests/run_tests.py` (cross-compiled
      executables can't be run natively — tests will be build-only).
- [ ] Document mobile-specific link requirements (system libs, frameworks).

---

## 2. Finish test snippets (11 remaining)

Deps without test snippets: `boringssl`, `cimgui`, `dawn`, `imgui`, `tracy`,
`libwebsockets`, `nanovg`, `raylib`, `utest`, `sdl3webgpu`, `stb_image_resize`.

Each needs a `tests/snippets/<dep>/test.c` (minimal API call) and optional
`config.json` for extra link libs / frameworks / skip flags.

- [ ] `stb_image_resize` — simplest, C, one function call
- [ ] `utest` — C, macro-based, just run a passing test
- [ ] `imgui` — C++, create context + destroy
- [ ] `cimgui` — C, wrap imgui create/destroy
- [ ] `tracy` — C++, set thread name
- [ ] `nanovg` — C, create GL context then destroy
- [ ] `boringssl` — C, call `SHA256` on empty input
- [ ] `libwebsockets` — C, create context + destroy
- [ ] `dawn` — C++, create instance
- [ ] `raylib` — C, init window + close
- [ ] `sdl3webgpu` — C, get SDL + WebGPU, no window

---

## 3. Windows fixes

- [ ] Fix `zlibstatic.lib` not installed into `_out/windows_x64/lib/` —
      blocks freetype/harfbuzz/skribidi static tests.
- [ ] Fix curl static on Windows (`__imp_curl_version_info` — import lib
      picked up instead of static lib).
- [ ] Re-enable freetype/harfbuzz/skribidi static tests on Windows.

---

## 4. New dependencies (future)

| Dep | Why | Size | Notes |
|---|---|---|---|
| Vulkan Memory Allocator | Header-only, essential for Vulkan | Tiny | MIT, just install the header |
| meshoptimizer | Mesh simplification & optimization | Small | MIT, CMake-native |
| volk | Vulkan loader, static linking | Small | MIT, CMake-native |
| libpng | PNG reference codec (faster than stb) | Medium | zlib already present |
| libjpeg-turbo | Fastest JPEG codec | Medium | CMake-native |
| OpenAL-Soft | Cross-platform 3D audio | Medium | LGPL, pairs with miniaudio |

---

## 5. Infrastructure

- [ ] **Incremental builds** — skip deps whose submodule commit is unchanged
      from the last `moredeps.json` (listed in TODO).
- [ ] **CCache/sccache hit rate tuning** — review cache keys, hash inputs.
- [ ] **Website improvements** — rich badge table for older releases, file
      sizes in tooltips.
- [ ] **`lua_pure` variant** — strip `io`, `os`, `package`, `debug` libs;
      require user allocator.
- [ ] **Docs refresh** — update stale `docs/build_plan.md` timelines, add
      shared-lib caveats, expand test harness docs.
