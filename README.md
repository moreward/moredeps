# moredeps

A collection of dependencies frequently used across projects, built as static libraries for multiple platforms from a single source tree.

By @morew4rd.

## Build philosophy

- **Library only.** Every dependency is configured to build only the library itself. Examples, tests, documentation, benchmarks, and tools are disabled by default.
- **Production quality.** No hacks or temporary workarounds. Every build path is documented, reproducible, and pinned to a specific dependency version.
- **Multi-platform.** Static libraries are produced for:
  - Windows x64 / arm64
  - Linux x64 / arm64
  - macOS arm64
  - Emscripten (wasm32)
- **CMake-driven.** The top-level `CMakeLists.txt` drives all CMake-buildable dependencies. Non-CMake or header-only dependencies live under `src/<dep>/` as small, clean wrappers.

## Quick start

### 1. Clone and initialize submodules

```bash
git clone https://github.com/moreward/moredeps.git
cd moredeps

# Initialize all top-level submodules.
# Do NOT use --recursive; dawn's nested submodules include private Google
# repositories that require authentication and are not needed.
git submodule update --init --jobs 4
```

If a submodule fails to initialize (e.g. from a partial or interrupted clone), reset it individually:

```bash
git submodule deinit -f deps/<name>
rmdir /s /q deps\<name>        # Windows
rm -rf deps/<name>             # Unix
git submodule update --init deps/<name>
```

### 2. Build for your platform

```bash
# macOS (native)
./scripts/build_all.sh macos_arm64

# Linux (native or cross to arm64)
./scripts/build_all.sh linux_x64
./scripts/build_all.sh linux_arm64

# Windows (from x64 Native Tools Command Prompt, or Git Bash)
./scripts/build_all.sh windows_x64
./scripts/build_all.sh windows_arm64

# Emscripten / WASM
./scripts/build_all.sh wasm_emscripten

# Build all supported platforms (requires all toolchains installed)
./scripts/build_all.sh all
```

Artifacts are staged in `_out/<platform>/lib/` and `_out/<platform>/include/`.

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

| Platform | Status | Notes |
|---|---|---|
| **macOS arm64** | ✅ Validated | 76 static libraries |
| **Linux arm64** | ✅ Validated | 76 static libraries |
| **Emscripten (wasm32)** | ✅ Validated | 65 static libraries (excludes desktop-only deps) |
| **Windows x64** | ✅ Validated | MSVC via Ninja or NMake |
| **Windows arm64** | ✅ Validated | MSVC cross-compile from x64 host; mtcc excluded (no ARM64 PE backend) |
| **Linux x64** | ⏳ Pending | Toolchain provided; awaiting validation |

### Known exclusions

- **Emscripten**: `glfw`, `mtcc`, `enet`, `libwebsockets`, `reproc`, `tinycsocket` (desktop-only APIs).
- **Windows arm64**: `mtcc` (TinyCC's PE backend lacks ARM64 support).
- **All platforms**: `ghostty` is macOS-only (Zig build, library extraction from xcframework).

See `docs/work_log.md` for the current state and known caveats.
