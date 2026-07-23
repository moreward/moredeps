# Next Steps

## ✅ Done: Mobile targets

| Platform | Deps | Static libs | CI |
|---|---|---|---|
| `ios_arm64` | 66 | 71 | ✅ macos-15 |
| `ios_simulator_arm64` | 66 | 71 | ✅ (same job) |
| `android_arm64` | 55 | 68 | ✅ ubuntu-24.04 + NDK r29 |
| `android_x64` | 55 | 68 | ✅ (same job) |

### iOS exclusions (3)
- `miniaudio`, `raudio` — iOS 26 SDK CoreAudio→Foundation ObjC chain
- `minigamepad` — IOKit (macOS-only)

### Android exclusions (7)
- `glfw`, `raylib`, `mtcc`, `reproc` — same as iOS
- `libuv`, `luv` — `posix_spawnattr_*` needs API 28
- `sdl3webgpu` — no ANativeWindow surface path
- `ubench`, `utest` — `timespec_get` needs API 26
- `sokol_audio` — AAudio needs API 26

---

## 2. Finish test snippets (11 remaining)

Deps without test snippets: `boringssl`, `cimgui`, `dawn`, `imgui`, `tracy`,
`libwebsockets`, `nanovg`, `raylib`, `utest`, `sdl3webgpu`, `stb_image_resize`.

Each needs a `tests/snippets/<dep>/test.c` (minimal API call) and optional
`config.json`.

---

## 3. New dependencies

| Dep | Why | Size |
|---|---|---|
| Vulkan Memory Allocator | Header-only, essential for Vulkan | Tiny |
| meshoptimizer | Mesh simplification & optimization | Small |
| volk | Vulkan loader, static linking | Small |
| libpng | PNG reference codec (faster than stb) | Medium |
| libjpeg-turbo | Fastest JPEG codec | Medium |
| OpenAL-Soft | Cross-platform 3D audio | Medium |

---

## 4. Infrastructure

- [ ] **Windows fixes** — `zlibstatic.lib` not installed, curl static import lib
- [ ] **Incremental builds** — skip deps with unchanged submodule commit
- [ ] **CCache tuning** — review cache keys and hash inputs
- [ ] **`lua_pure` variant** — strip `io`, `os`, `package`, `debug`
- [ ] **Website** — file sizes in tooltips, older release badge table
