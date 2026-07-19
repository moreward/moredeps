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

## Download prebuilt libraries

CI builds all platforms and publishes per-dependency zips (library + headers + license) to GitHub Releases. The status site **[deps.morew4rd.com](https://deps.morew4rd.com)** renders the build matrix from the manifest and generates copy-paste `curl` commands for batch downloads.

- Releases: `build-<sha>` (immutable per commit) and `latest` (rolling alias). Only the **last 3 builds** are retained; older releases are pruned automatically.
- `moredeps.json` (attached to each release) lists every artifact with the dependency's pinned upstream commit and repo URL.
- **[Examples](https://github.com/moreward/moredeps/tree/main/examples)** show how to download, unpack, and vendor moredeps into your own GitHub releases so you control the binary lifetime.

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

## Examples

See the [`examples/`](https://github.com/moreward/moredeps/tree/main/examples) directory for ready-to-use scripts and workflows that show how to consume moredeps:

- **Download and unpack** a few packages locally (Bash or Python).
- **Vendor to your own GitHub release** so the binaries are pinned under your
  control and not lost when moredeps rotates its last-3-builds window.
- **Fetch at configure time** from a CMake project.

## Documentation

- [`docs/build_plan.md`](docs/build_plan.md) — the overall build-infrastructure plan, targets, exclusions, and implementation phases.
- [`docs/build_options.md`](docs/build_options.md) — per-dependency CMake options and the defaults we selected (examples/tests/docs disabled, minimal optional features).
- [`docs/ci_plan.md`](docs/ci_plan.md) — CI/CD design: GitHub Actions workflow, caching, packaging, manifest, releases, and the download site.
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

All platforms are built in CI on every run (see [`docs/ci_plan.md`](docs/ci_plan.md)):

| Platform | Status | Libraries |
|---|---|---|
| **Windows x64** | ✅ Validated | 77 |
| **Windows arm64** | ✅ Validated | 75 (native ARM64 MSVC; mtcc excluded) |
| **Linux x64** | ✅ Validated | 76 |
| **Linux arm64** | ✅ Validated | 76 |
| **macOS arm64** | ✅ Validated | 74 |
| **Emscripten (wasm32)** | ✅ Validated | 65 (excludes desktop-only deps) |

### Known exclusions

- **Emscripten**: `glfw`, `dawn` (browser provides WebGPU), `mtcc`, `enet`, `libwebsockets`, `reproc`, `tinycsocket`.
- **Windows arm64**: `mtcc` (TinyCC's PE backend lacks ARM64 support).

See `docs/work_log.md` for the current state and known caveats.
