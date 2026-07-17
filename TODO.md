# TODO

## Test coverage

- [x] Add test snippets (44/46 deps covered, 86 PASS):
  - [x] `zlib`, `sqlite-amalgamation`, `cJSON`, `curl`, `enet`
  - [x] `mimalloc`, `miniaudio`, `skribidi`, `sokol`, `sokol_gp`
  - [x] `lz4`, `xxhash`, `utf8proc`, `harfbuzz`, `freetype`
  - [x] `lua`, `stb_image`, `FastNoiseLite`, `libunibreak`, `SheenBidi`
  - [x] `physfs`, `box3d`, `cglm`, `cgltf`, `microui`
  - [x] `flecs`, `budouxc`, `fontstash`, `reproc`, `tinycsocket`
  - [x] `stb_image_write`, `stb_rect_pack`, `sokol_args`, `sokol_fetch`
  - [x] `sokol_log`, `sokol_time`, `stb_ds`, `stb_truetype`
  - [x] `mtcc`, `ubench`, `raudio`, `sdl3`, `glfw`
- [ ] Still unsnippeted (hard APIs): `boringssl`, `cimgui`, `dawn`,
      `imgui`, `tracy`, `libwebsockets`, `nanovg`, `raylib`,
      `utest`, `sdl3webgpu`, `stb_image_resize`
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
