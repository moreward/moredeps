# Mobile build plan (iOS / Android)

This document outlines what it would take to extend the `moredeps` build system to iOS and Android, which libraries are expected to work, and which should be **gated** or require special handling.

## 1. New platforms and toolchains

We would add toolchain files under `toolchain/` for the following targets:

| Platform | Toolchain notes |
|---|---|
| `ios_arm64` | iOS device (arm64). Use Xcode + `CMAKE_SYSTEM_NAME=iOS`. Metal-capable. |
| `ios_simulator` | iOS simulator (arm64 or x86_64). Useful for CI/tests. |
| `android_arm64` | Android NDK, target ABI `arm64-v8a`. |
| `android_x64` | Android NDK, target ABI `x86_64`. |
| `android_arm` | Android NDK, target ABI `armeabi-v7a`. Optional, older devices. |
| `android_x86` | Android NDK, target ABI `x86`. Optional, mainly for emulators. |

### 1.1 iOS specifics
- Set `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_DEPLOYMENT_TARGET=15.0` (or lower), and `CMAKE_OSX_ARCHITECTURES=arm64`.
- Metal is available on-device and on Apple Silicon simulators.
- OpenGL ES is **deprecated** on iOS; Metal is preferred. `SOKOL_GLES3` may still work but is not recommended.
- The system is sandboxed: `reproc`, `mtcc`, `tinycsocket` server sockets, and raw `curl`/`libwebsockets` may work for outbound connections but not for listening or subprocesses.

### 1.2 Android specifics
- Use the Android NDK CMake toolchain (`android.toolchain.cmake`) or a custom toolchain that sets `CMAKE_SYSTEM_NAME=Android`, `ANDROID_ABI`, `ANDROID_PLATFORM`, and `ANDROID_STL=c++_shared` or `c++_static`.
- Vulkan is available on most modern Android devices; GLES3 is widely supported.
- Dynamic executables are restricted; `mtcc` and `reproc` are unlikely to be useful.

## 2. Dependency support matrix

| Library | iOS | Android | Notes |
|---|---|---|---|
| `boringssl` | ✅ | ✅ | Pure C/C++, no issues. |
| `box3d` | ✅ | ✅ | Physics engine, no platform code. |
| `budouxc` | ✅ | ✅ | Text segmentation, no platform code. |
| `cglm` | ✅ | ✅ | Math library. |
| `cimgui` / `imgui` | ✅ | ✅ | UI library, works on mobile with touch input. |
| `cJSON` | ✅ | ✅ | JSON parser. |
| `curl` | ✅ outbound | ✅ outbound | Works for HTTP/HTTPS outbound. No background networking on iOS without care. |
| `dawn` | ✅ (Metal) | ⚠️ (Vulkan) | iOS Metal works. Android needs Vulkan and the NDK Vulkan loader. |
| `enet` | ✅ | ✅ | UDP sockets available on both. |
| `flecs` | ✅ | ✅ | ECS, pure C. |
| `freetype` | ✅ | ✅ | Font rasterization. |
| `glfw` | ❌ | ❌ | GLFW does not support iOS or Android. Use `sokol_app` or `sdl3` for windowing. |
| `harfbuzz` | ✅ | ✅ | Text shaping. |
| `libunibreak` | ✅ | ✅ | Line breaking. |
| `libwebsockets` | ✅ outbound | ✅ outbound | TLS + WebSocket client. Server sockets may be restricted. |
| `lua` | ✅ | ✅ | Core Lua compiles cleanly. `loadlib` (dynamic loading) is limited on mobile. |
| `lz4` | ✅ | ✅ | Compression. |
| `miniaudio` | ✅ | ✅ | Audio playback/capture. |
| `mimalloc` | ✅ | ✅ | Allocator, but may need `MIMALLOC_USE_STD_MALLOC` on some mobile configs. |
| `mtcc` | ❌ | ❌ | TinyCC targets x86/amd64; not suitable for ARM mobile. |
| `physfs` | ✅ | ✅ | File system abstraction. |
| `raylib` | ⚠️ | ⚠️ | Has `PLATFORM_ANDROID` and `PLATFORM_IOS`/`PLATFORM_DRM` support, but mobile integration is non-trivial. |
| `reproc` | ❌ | ❌ | Process spawning is sandboxed/unsupported on iOS and unusual on Android. |
| `sdl3` | ✅ | ✅ | Official iOS/Android support. |
| `sdl3webgpu` | ✅ | ⚠️ | iOS Metal works. Android needs Vulkan + Dawn support. |
| `SheenBidi` | ✅ | ✅ | BiDi algorithm. |
| `skribidi` | ✅ | ✅ | Depends on harfbuzz/SheenBidi/libunibreak/budouxc. |
| `sokol_*` | ✅ | ✅ | `sokol_app` supports iOS (Metal/GLES3) and Android (GLES3). `sokol_gfx`/`sokol_gp` follow. |
| `sqlite-amalgamation` | ✅ | ✅ | SQLite is portable. |
| `stb_*` | ✅ | ✅ | Header-only utilities. |
| `tinycsocket` | ✅ | ✅ | BSD sockets work on mobile for outbound/client use. |
| `tracy` | ✅ | ✅ | Profiling client; server is usually on desktop. |
| `utf8proc` | ✅ | ✅ | Unicode processing. |
| `xxhash` | ✅ | ✅ | Hashing. |
| `zlib` / `zstd` | ✅ | ✅ | Compression. |

## 3. Gated / excluded libraries on mobile

Based on the matrix above, the following should be **gated** when targeting mobile:

| Library | iOS gate | Android gate | Reason |
|---|---|---|---|
| `glfw` | ❌ | ❌ | No mobile support. Use `sokol_app` or `sdl3`. |
| `mtcc` | ❌ | ❌ | TCC does not target ARM mobile. |
| `reproc` | ❌ | ⚠️ | iOS sandbox forbids process spawning; Android generally does not support it either. |
| `ghostty` | ❌ | ❌ | Desktop only (macOS, Linux, Windows). |

Optional considerations:
- `libwebsockets` / `enet` / `tinycsocket`: keep enabled for **client** networking, but document that server/listen sockets are restricted on iOS and may require permissions on Android.
- `curl`: keep enabled for outbound HTTP/HTTPS, but note that iOS background networking requires `NSURLSession` or special handling; libcurl is foreground-only.

## 4. Sokol on mobile

Sokol already supports iOS and Android natively. The variant matrix would extend as follows:

| Platform | `sokol_app` variants | `sokol_gfx` / `sokol_gp` variants |
|---|---|---|
| `ios_arm64` | `metal`, `gles3` | `metal`, `gles3` |
| `ios_simulator` | `metal`, `gles3` | `metal`, `gles3` |
| `android_arm64` | `gles3` | `gles3`, `wgpu` (if Dawn Vulkan works) |
| `android_x64` | `gles3` | `gles3`, `wgpu` |

Notes:
- `SOKOL_GLCORE33` is **not** available on iOS or Android.
- On Android, `SOKOL_GLES3` is the default and most compatible choice.
- WGPU on Android requires Vulkan support in Dawn and the device.

## 5. Build script additions

`scripts/build_all.sh` and `scripts/validate_dev_env.sh` would be extended:

- `validate_dev_env.sh ios_arm64`: check for Xcode and `xcodebuild`.
- `validate_dev_env.sh android_arm64`: check for `ANDROID_HOME`, `ANDROID_NDK`, and `clang`/`clang++` in the NDK.
- `build_all.sh` would select the NDK toolchain for Android targets.

## 6. CMake changes needed

1. Add toolchain files under `toolchain/`.
2. Add platform detection in `CMakeLists.txt` for `iOS` and `Android`.
3. Gate `glfw`, `mtcc`, `reproc`, and `ghostty` on mobile.
4. Adjust Sokol variant lists to include mobile variants.
5. For Android, ensure `BUILD_SHARED_LIBS=OFF` and `POSITION_INDEPENDENT_CODE=ON` (already set).
6. For iOS, ensure bitcode is disabled unless needed, and code signing is handled by the downstream app.

## 7. Dawn / WebGPU on mobile

**iOS:** Dawn supports iOS through its **Metal backend**. No extra third-party dependencies are needed beyond what is already in `deps/dawn_third_party/` (abseil, SPIR-V tools, Vulkan headers, etc.). The build just needs `DAWN_ENABLE_METAL=ON` and an iOS toolchain.

**Android:** Dawn supports Android through its **Vulkan backend**. The Android system provides the Vulkan loader (`libvulkan.so`), and the NDK provides the Vulkan headers. No new git submodules are required. The build needs:
- `DAWN_ENABLE_VULKAN=ON`
- An Android NDK toolchain
- `SDL_VULKAN=ON` (if using SDL3) or a custom Vulkan surface setup

**sdl3webgpu on mobile:** The upstream `sdl3webgpu` source currently has surface creation code for macOS, iOS, Linux (X11/Wayland), Windows, and Emscripten. It does **not** have an Android implementation. For Android we would need to add a `WGPUSurfaceDescriptorFromAndroidNativeWindow` path (or similar) using `ANativeWindow` from the NDK. This is a small patch but is not present upstream.

### sokol_app on Android

`sokol_app.h` explicitly restricts Android to `SOKOL_GLES3`. It does not support `SOKOL_WGPU` on Android. Therefore, `sokol_app_wgpu` and `sokol_glue_wgpu` are **not** available for Android. The path for Android WebGPU is:
- Use SDL3 or a native `ANativeWindow` for windowing.
- Create the WGPU surface manually.
- Use `sokol_gfx_wgpu` for rendering.

## 8. Raylib on mobile

Raylib has `PLATFORM_ANDROID` and `PLATFORM_IOS`/`PLATFORM_DRM` support in its documentation, but the actual source tells a different story:

1. **iOS is not implemented.** There is no `rcore_ios.c` platform backend in `deps/raylib/src/platforms/`. Raylib cannot target iOS with its current source tree.
2. **Android is Makefile-only.** There is an `rcore_android.c` backend, but the CMake build does **not** include `PLATFORM_ANDROID` support. Building raylib for Android would require porting the Makefile logic into CMake or building outside of our CMake super-project.
3. **Android graphics backend is OpenGL ES 2.0/3.0.** Existing shaders need to be GLES-compatible. On iOS (if it were supported), OpenGL ES is deprecated.

Compared to SDL3, which has first-class CMake support and platform backends for both iOS and Android, raylib's mobile support is incomplete. This is why raylib is flagged as non-trivial / risky for mobile, while SDL3 is expected to work.

The other mobile concerns (app scaffolding, code signing, asset loading, permissions) apply to **both** SDL3 and raylib, but SDL3 has the platform backends and build-system support already in place, whereas raylib does not.

## 9. Open questions / next steps

- Do we want to support iOS/Android in this repository now, or keep it as a planned future phase?
- Should we add a single `ios_arm64`/`android_arm64` build now, or wait until desktop platforms are fully validated?
- Does the downstream project (e.g., `lyte2d`) actually need mobile builds, or is this speculative?
- For Android, do we need to ship `.so` shared libraries for dependencies that are linked into a shared library? Android apps typically load native code via `.so` files, which may require building dependencies with `PIC` and possibly as shared libraries.
