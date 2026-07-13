# moredeps

A collection of dependencies frequently used across projects, built as static libraries for multiple platforms from a single source tree.

## Build philosophy

- **Library only.** Every dependency is configured to build only the library itself. Examples, tests, documentation, benchmarks, and tools are disabled by default.
- **Production quality.** No hacks or temporary workarounds. Every build path is documented, reproducible, and pinned to a specific dependency version.
- **Multi-platform.** Static libraries are produced for:
  - Windows x64 / arm64
  - Linux x64 / arm64
  - macOS arm64
  - Emscripten (wasm32)
- **CMake-driven.** The top-level `CMakeLists.txt` drives all CMake-buildable dependencies. Non-CMake or header-only dependencies live under `src/<dep>/` as small, clean wrappers.

## Documentation

- [`docs/build_plan.md`](docs/build_plan.md) — the overall build-infrastructure plan, targets, exclusions, and implementation phases.
- [`docs/build_options.md`](docs/build_options.md) — per-dependency CMake options and the defaults we selected (examples/tests/docs disabled, minimal optional features).
- [`docs/mobile_plan.md`](docs/mobile_plan.md) — iOS/Android support plan and dependency matrix.
- [`docs/shared_libs.md`](docs/shared_libs.md) — how to produce shared libraries from the static artifacts.

## Layout

- `deps/` — upstream sources as git submodules (read-only). All dependencies are top-level submodules; `git submodule update --init --recursive` is not strictly required anymore.
- `src/<dep>/` — CMake wrappers and implementation files for header-only / non-CMake dependencies.
- `toolchain/` — CMake toolchain files for cross-compilation.
- `_b/<platform>/` — CMake build trees (gitignored).
- `_out/<platform>/` — staged static libraries and public headers.
- `scripts/build_all.sh` — entry point to build all platform combinations.
- `scripts/validate_dev_env.sh` — check that the host has the required tools for a platform.
- `scripts/clean_all.sh` — remove all `_b/` and `_out/` directories for a clean start.

## Status

- **macOS arm64** — validated. All configured dependencies build and install (67 static libraries), including `cimgui`, `raylib`, `mtcc`, and `ghostty`.
- **Emscripten (wasm32)** — validated. All Emscripten-compatible dependencies build and install; `glfw`, `mtcc`, `sdl3webgpu`, `lua`, and `ghostty` are excluded on this target.
- **Linux x64 / arm64** and **Windows x64 / arm64** — toolchains are provided, but these targets remain to be validated on appropriate hosts.

See `docs/work_log.md` for the current state and known caveats.
