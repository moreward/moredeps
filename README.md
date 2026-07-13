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

## Layout

- `deps/` — upstream sources as git submodules (read-only). Some submodules contain nested submodules (e.g. `cimgui/imgui`), so clone with `git submodule update --init --recursive`.
- `src/<dep>/` — CMake wrappers and implementation files for header-only / non-CMake dependencies.
- `toolchain/` — CMake toolchain files for cross-compilation.
- `_b/<platform>/` — CMake build trees (gitignored).
- `_out/<platform>/` — staged static libraries and public headers.
- `scripts/build_all.sh` — entry point to build all platform combinations.

## Status

- **macOS arm64** — validated. All configured dependencies build and install (66 static libraries), including recently added `cimgui`, `raylib`, and `mtcc`.
- **Emscripten (wasm32)** — validated. All Emscripten-compatible dependencies build and install; `glfw`, `mtcc`, `sdl3webgpu`, and `lua` are excluded on this target.
- **Linux x64 / arm64** and **Windows x64 / arm64** — toolchains are provided, but these targets remain to be validated on appropriate hosts.

See `docs/work_log.md` for the current state and known caveats.
