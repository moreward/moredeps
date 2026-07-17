# TODO

## Test coverage

- [ ] Add test snippets for ~30 remaining deps:
  - `boringssl`, `box3d`, `budouxc`, `cglm`, `cgltf`, `cimgui`
  - `dawn`, `FastNoiseLite`, `flecs`, `fontstash`, `glfw`
  - `imgui`, `libunibreak`, `libwebsockets`, `microui`, `mtcc`
  - `nanovg`, `physfs`, `raudio`, `raylib`, `reproc`
  - `sdl3`, `sdl3webgpu`, `SheenBidi`, `sokol` variants, `stb`
  - `tinycsocket`, `tracy`, `ubench`, `utest`
- [ ] Wire WASM link tests back into CI (toolchain auto-detection bug)

## Windows

- [ ] Fix `zlibstatic.lib` not installed into `_out/windows_x64/lib/` (built but install step skips it)
- [ ] Fix curl static on Windows (`__imp_curl_version_info` — import lib picked up instead of static)
- [ ] Re-enable freetype/harfbuzz/skribidi static tests on Windows after zlibstatic.lib fix

## Website

- [ ] Show rich badge table for older releases too (CORS workaround: fetch `moredeps.json` via `raw.githubusercontent.com` or GitHub API)
- [ ] Add artifact file size to tooltip/download info

## Performance

- [ ] Incremental builds: skip deps whose submodule commit is unchanged from the last `moredeps.json`
- [ ] CI ccache/sccache hit rate tuning

## Features

- [ ] `lua_pure` variant — strip `io`, `os`, `package`, `debug` libs; require user allocator
- [ ] Mobile targets: `ios_arm64`, `ios_simulator`, `android_arm64`, `android_x64`

## Docs

- [ ] Update stale phase/timeline sections in `docs/build_plan.md`
- [ ] Add per-dep shared-lib caveats to `docs/build_options.md`
- [ ] Document test harness (`tests/README.md` exists, could be expanded)
