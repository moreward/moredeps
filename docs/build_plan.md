# Build Infrastructure Plan for `moredeps`

**Status:** Implemented and validated locally for `macos_arm64` and `wasm_emscripten`.
**Purpose:** Define the production-grade build system that produces static (and optionally shared) libraries for every dependency across all supported host/target combinations, with deterministic versioning and releasable artifacts.

## 1. Goals & Constraints

### 1.1 Targets (initial matrix)
We must produce static libraries for every buildable dependency on the following platforms:

| Platform | Arch | Status |
|---|---|---|
| Windows | x64 | Toolchain provided; pending VM validation |
| Windows | arm64 | Toolchain provided; pending VM validation |
| Linux | x64 | Toolchain provided; pending VM validation |
| Linux | arm64 | Toolchain provided; pending VM validation |
| macOS | arm64 | **Validated locally** |
| Emscripten | wasm32 | **Validated locally** |

> **Mobile** (iOS/Android) is explicitly out of scope for the first phase but must be addable later without redesigning the layout.

### 1.2 Hard requirements (from the user)
- **Library only.** Every dependency must be configured to build only the library. Examples, tests, documentation, benchmarks, and tools are disabled by default. If a particular option is required to produce the library itself, it is enabled and documented in `docs/build_options.md`.
- **Production-level only.** No hacky scripts, no “works for now,” no undocumented assumptions.
- Every dependency must be built with its **correct** build system, not forced into a one-size-fits-all script unless that is the canonical way (e.g., header-only libs wrapped in a `.c` file).
- Use a top-level `CMakeLists.txt` to drive **all CMake-buildable dependencies**.
- Header-only libraries get a dedicated `src/<DEP_NAME>/` directory containing a `.c` (or `.cpp`) file that defines the implementation macro(s) and includes the public header(s) so that CMake can produce a real static library.
- All builds go under `_b/<PLATFORM>_<ARCH>/<DEP_NAME>/` (or `wasm_emscripten/<DEP_NAME>`).
- Some combinations are **not buildable**; these must be documented, not silently skipped.
- Create `scripts/build_all.sh` that orchestrates everything.
- Capture and record things that cannot build this way.
- Later CI will produce a JSON manifest of commit hashes and artifact hashes, zip each artifact (`static lib + headers`, optional shared lib), upload to GitHub Releases, and skip unchanged dependencies.

### 1.3 Non-goals (for now)
- Running tests for every dependency (we build libraries, not run the upstream test suites by default).
- Mobile targets (added in a later phase).
- Wrapping dependencies into package managers (vcpkg/conan/etc.).

---

## 2. Dependency Inventory & Categorization

The repo contains the following submodules / vendored sources.  
Legend:
- **C**: CMake native
- **M**: Makefile only
- **H**: Header-only / implementation-macro
- **A**: Autotools / configure
- **Z**: Zig build
- **B**: Bazel available
- **X**: Special / unsupported in this infrastructure

| Dep | Build system | Category | Build approach in this repo | Known exclusions / notes |
|---|---|---|---|---|
| `box3d` | CMake | C | `ExternalProject_Add` | None known. |
| `budouxc` | CMake | C | `src/budouxc/` wrapper | Upstream CMakeLists has broken install paths; wrapper builds from source. |
| `cglm` | CMake | C | `ExternalProject_Add` | None known. |
| `cgltf` | Header-only | H | `src/cgltf/` wrapper | None known. |
| `cimgui` | CMake / Makefile | C++ wrapper | `src/cimgui/` wrapper | Upstream `CMakeLists.txt` hard-codes `SHARED`; we build static from source. |
| `cJSON` | CMake | C | `ExternalProject_Add` | None known. |
| `curl` | CMake | C | `ExternalProject_Add` | BoringSSL via `CURL_USE_OPENSSL=ON` (OpenSSL compatibility) on all platforms. |
| `dawn` | CMake | C | `ExternalProject_Add` | `DAWN_FETCH_DEPENDENCIES=OFF`; third-party deps in `deps/dawn_third_party/`. Emscripten uses `emdawnwebgpu`. |
| `enet` | CMake | C | `ExternalProject_Add` | Network lib; currently builds on Emscripten but is documented as unsuitable for the browser. |
| `FastNoiseLite` | Header-only | H | `src/FastNoiseLite/` wrapper | C header used. |
| `flecs` | CMake | C | `ExternalProject_Add` | None known. |
| `fontstash` | Header-only | H | `src/fontstash/` wrapper | None known. |
| `freetype` | CMake | C | `ExternalProject_Add` | `FT_DISABLE_HARFBUZZ=OFF` so HarfBuzz can be used. |
| `ghostty` | Zig build | Z | `src/ghostty/` wrapper | macOS only; extracts `libghostty.a` from the xcframework produced by `zig build`. |
| `glfw` | CMake | C | `ExternalProject_Add` | Excluded on `wasm_emscripten`. |
| `harfbuzz` | CMake | C | `ExternalProject_Add` | `HB_HAVE_FREETYPE=ON`; built after FreeType. |
| `libwebsockets` | CMake | C | `ExternalProject_Add` | BoringSSL; feature-detection flags forced for BoringSSL compatibility. **Excluded on `wasm_emscripten`.** |
| `libunibreak` | Makefile | M | `src/libunibreak/` wrapper | No upstream CMake; wrapper builds from source. |
| `lua-5.5.0` | Makefile | M | `src/lua/` wrapper | Built as C. |
| `lz4` | CMake (in `build/cmake`) | C | `ExternalProject_Add` via `deps/lz4/build/cmake` | None known. |
| `microui` | Single `.c` + header | H | `src/microui/` wrapper | None known. |
| `mimalloc` | CMake | C | `ExternalProject_Add` | None known. |
| `miniaudio` | CMake | C | `ExternalProject_Add` | None known. |
| `minigamepad` | Header-only | H | `src/minigamepad/` wrapper | None known. |
| `mtcc` | Makefile | M | `src/mtcc/` wrapper | Excluded on `wasm_emscripten`. |
| `nanovg` | `.c` + header | H | `src/nanovg/` wrapper | None known. |
| `boringssl` | CMake | C | `ExternalProject_Add` | TLS backend for `curl` and `libwebsockets`. |
| `physfs` | CMake | C | `ExternalProject_Add` | None known. |
| `raudio` | `.c` + header | H | `src/raudio/` wrapper | None known. |
| `raylib` | CMake | C | `ExternalProject_Add` | `BUILD_EXAMPLES=OFF`, `BUILD_SHARED_LIBS=OFF`. Emscripten uses `PLATFORM=Web`. |
| `reproc` | CMake | C | `ExternalProject_Add` | None known. |
| `sdl3` | CMake | C | `ExternalProject_Add` | None known. |
| `sdl3webgpu` | CMake | C | `src/sdl3webgpu/` wrapper | Patched at build time on Emscripten for `WGPUStringView` API; otherwise depends on `dawn` + `sdl3`. |
| `SheenBidi` | CMake | C | `ExternalProject_Add` | Unicode bidi algorithm library. |
| `skribidi` | CMake | C | `src/skribidi/` wrapper | Depends on `harfbuzz`, `SheenBidi`, `libunibreak`, `budouxc`. Upstream fetches these; we use submodules. |
| `sokol` | Header-only | H | `src/sokol_<mod>/` wrappers + generated backend variants | Per-module static libs. Platform defaults use Metal/D3D11/GLCORE/GLES3. Backend-specific variants (`*_glcore`, `*_metal`, `*_d3d11`, `*_gles3`, `*_wgpu`) are produced. `sokol_app`/`sokol_glue` WGPU is only available on Emscripten. |
| `sokol_gp` | Header-only | H | `src/sokol_gp/` wrapper + generated backend variants | Built against the vendored sokol headers in `deps/sokol_gp/thirdparty`; must not be mixed with top-level `sokol` libraries. Variants: `*_glcore`, `*_gles3`, `*_metal`, `*_d3d11`. No WGPU variant. |
| `sqlite-amalgamation` | CMake | C | `ExternalProject_Add` | None known. |
| `stb` | Header-only | H | `src/stb_<lib>/` wrappers | Per-module static libraries. |
| `tinycsocket` | CMake | C | `src/tinycsocket/` wrapper | Upstream writes into its source tree; wrapper copies to the build tree first. |
| `tracy` | CMake | C | `ExternalProject_Add` | Client library only. |
| `ubench` | Header-only | H | `src/ubench/` wrapper | Emscripten needs `emscripten/html5.h` for `emscripten_performance_now`. |
| `utest` | Header-only | H | `src/utest/` wrapper | None known. |
| `utf8proc` | CMake | C | `ExternalProject_Add` | None known. |
| `xxhash` | CMake (in `cmake_unofficial`) | C | `ExternalProject_Add` via `deps/xxhash/cmake_unofficial` | None known. |
| `zlib` | CMake | C | `ExternalProject_Add` | None known. |
| `zstd` | CMake (in `build/cmake`) | C | `ExternalProject_Add` via `deps/zstd/build/cmake` | None known. |

### 2.1 Dependency ordering concerns
Some dependencies consume others; the build order matters if we link tests/examples, but for pure static-library output each can be built independently.  However, for libraries that *offer* optional features (e.g., `curl` with SSL, `harfbuzz` with FreeType), we need to decide per-platform defaults and document them.  Where a feature requires another dep, we may need to build the provider first and pass paths in.

### 2.2 Reproducibility / version pinning
Several submodules currently point to branch heads (`stb`, `nanovg`, `fontstash`, `minigamepad`, `ubench`, `utest`, `mtcc`, `sokol`, `sokol_gp`, `skribidi`, etc.).  For a production dependency repo, we must pin each submodule to a concrete tag or commit.  The CI/release JSON manifest will record the resolved commit hash for each dependency, but the `.gitmodules` or submodule commits should be pinned as well.

---

## 3. Build Infrastructure Design

### 3.1 Repository layout after this work

```
moredeps/
├── CMakeLists.txt              # Top-level super-build orchestrator
├── scripts/
│   ├── build_all.sh            # Entry point for all combinations
│   ├── install_dawn.cmake      # Stages emdawnwebgpu artifacts on Emscripten
│   └── patch_skribidi.py       # Build-time patcher for skribidi warning-as-error
├── src/                        # Wrappers for header-only / non-CMake / patched deps
│   ├── cgltf/
│   ├── cimgui/                 # static lib wrapper (upstream hard-codes SHARED)
│   ├── budouxc/                # wrapper; upstream install paths are broken
│   ├── FastNoiseLite/
│   ├── fontstash/
│   ├── ghostty/                # Zig build wrapper; extracts libghostty.a from xcframework
│   ├── libunibreak/            # Makefile-only wrapper
│   ├── microui/
│   ├── miniaudio/
│   ├── minigamepad/
│   ├── mtcc/                   # Makefile-based TinyCC wrapper
│   ├── nanovg/
│   ├── raudio/
│   ├── sdl3webgpu/             # wrapper to avoid cross-ExternalProject target refs
│   ├── sokol_app/              # per-module static libraries for sokol
│   ├── sokol_args/
│   ├── sokol_audio/
│   ├── sokol_fetch/
│   ├── sokol_gfx/
│   ├── sokol_glue/
│   ├── sokol_log/
│   ├── sokol_time/
│   ├── sokol_gp/
│   ├── stb_image/
│   ├── stb_image_write/
│   ├── stb_image_resize/
│   ├── stb_truetype/
│   ├── stb_rect_pack/
│   ├── stb_ds/
│   ├── tinycsocket/            # wrapper that copies upstream to build tree
│   ├── ubench/
│   └── utest/
├── toolchain/                  # CMake toolchain files
│   ├── windows_x64.cmake
│   ├── windows_arm64.cmake
│   ├── linux_x64.cmake
│   ├── linux_arm64.cmake
│   ├── macos_arm64.cmake
│   └── wasm_emscripten.cmake
├── _b/                         # Generated build trees (gitignored)
│   ├── windows_x64/
│   ├── windows_arm64/
│   ├── linux_x64/
│   ├── linux_arm64/
│   ├── macos_arm64/
│   └── wasm_emscripten/
├── _out/                       # Staged static libraries + headers
├── docs/
│   ├── build_plan.md
│   └── build_options.md
├── deps/                       # Git submodules (read-only source)
│   ├── budouxc/                # Skribidi dependency
│   ├── dawn_third_party/       # Dawn's external deps as flat submodules
│   ├── imgui/                  # Dear ImGui (used by cimgui wrapper)
│   ├── libunibreak/            # Skribidi dependency
│   └── SheenBidi/              # Skribidi dependency
└── .github/workflows/          # CI (future phase)
```

### 3.2 Top-level `CMakeLists.txt`

The top-level `CMakeLists.txt` is a **super-build** orchestrator.  Each dependency is built in isolation with `ExternalProject_Add` so that:

* Each dependency can use its own CMake policy, generator expressions, and install rules without leaking into the parent project.
* Cross-compilation toolchains are applied cleanly to each sub-build.
* Each dependency installs into the common prefix `CMAKE_INSTALL_PREFIX` (set to `_out/<platform>` by `scripts/build_all.sh`), so later sub-builds can `find_package` earlier ones.

Key contents:

1. **Minimum CMake version** and project declaration.
2. **Policy / defaults** inherited by all `ExternalProject` sub-builds:
   - `CMAKE_POSITION_INDEPENDENT_CODE ON`.
   - `BUILD_SHARED_LIBS OFF`.
   - `CMAKE_C_STANDARD=11`, `CMAKE_CXX_STANDARD=17`.
   - `CMAKE_PREFIX_PATH` and `CMAKE_FIND_ROOT_PATH` pointing to the common install prefix so cross-compilation sub-builds can find previously installed dependencies.
3. **`moredeps_add_dep(name ...)` helper** that wraps `ExternalProject_Add` with sensible defaults.
4. **Per-dependency sections** guarded by platform when a dep is unsupported on a target (e.g. `glfw` and `mtcc` on Emscripten).
5. **Wrappers in `src/<dep>/`** for header-only / non-CMake dependencies.  These wrappers are themselves built as `ExternalProject`s, so they install static libraries and headers into the common prefix.

### 3.3 `scripts/build_all.sh`

Responsibilities:

1. Detect the host OS and available toolchains (fail fast if a requested toolchain is missing).
2. Parse arguments to build one or all combos:
   ```bash
   ./scripts/build_all.sh all
   ./scripts/build_all.sh macos_arm64
   ./scripts/build_all.sh wasm_emscripten
   ```
3. For each platform, create `_b/<platform>` and run:
   ```bash
   cmake -S . -B _b/<platform> -G "Unix Makefiles" \
         -DCMAKE_TOOLCHAIN_FILE=toolchain/<platform>.cmake \
         -DCMAKE_BUILD_TYPE=Release
   cmake --build _b/<platform> --config Release --parallel
   ```
   Emscripten uses the toolchain file (`toolchain/wasm_emscripten.cmake`) which locates the Homebrew Emscripten SDK root under `libexec/`.
4. Stage outputs under `_out/<platform>/` (the value of `CMAKE_INSTALL_PREFIX`).
5. Print a summary of built vs. skipped dependencies.

`scripts/install_dawn.cmake` is used on Emscripten to stage the `emdawnwebgpu` headers and JS files, because Dawn produces no installable static library on that target.

### 3.4 Cross-compilation toolchains

We will create toolchain files in `toolchain/` for each target.  They must set:
- `CMAKE_SYSTEM_NAME` / `CMAKE_SYSTEM_PROCESSOR`
- C/C++ compilers (e.g., `aarch64-linux-gnu-gcc`, `cl.exe`, `emcc`/`em++`)
- Sysroots and find-root paths as needed
- Architecture flags (`/MACHINE` for MSVC, `-arch arm64` for Apple, etc.)

For Windows cross-compilation from macOS/Linux we cannot run MSVC locally, so builds for Windows must be performed on a Windows machine or in CI with the correct toolchain.  The script must detect this and print a clear error if a required toolchain is unavailable.

### 3.5 Emscripten specifics

- The build script must locate `emcmake`/`emmake` or `emcc`/`em++` and set the Emscripten toolchain file.
- Emscripten outputs are `.a` static libraries and may be used in downstream CMake projects with `emcc`.
- **Dawn**: use the Emscripten-specific WebGPU implementation `emdawnwebgpu`.  Do **not** pass `USE_WEBGPU`/`SOKOL_USE_WEBGPU` for the Emscripten target.
- Some deps (e.g., `glfw`, `reproc`, `tinycsocket`, `mtcc`, `enet`, `libwebsockets`) will be **excluded** from Emscripten because they use platform APIs not available on the web. See the build output summary and `docs/build_options.md` for notes on which wasm artifacts are browser-usable.
- `raylib` builds on Emscripten with `PLATFORM=Web`; downstream apps must link with Emscripten's GLFW port (`-sUSE_GLFW=3`).

### 3.7 Sokol backend variants

Sokol headers are header-only, so the same source can be compiled multiple times with different backend macros to produce different static libraries. We generate per-backend variants using the `moredeps_sokol_variant()` helper and the templates in `src/sokol_variant/`.

| Platform | Default backend | Built variants (library names) |
|---|---|---|
| `macos_arm64` | `SOKOL_METAL` | `sokol_*{_glcore, _metal}` for `app`/`gfx`/`glue`; `sokol_gfx_wgpu` |
| `linux_x64` / `linux_arm64` | `SOKOL_GLCORE` | `sokol_*{_glcore, _gles3, _wgpu}` for `app`/`gfx`/`glue` |
| `windows_x64` / `windows_arm64` | `SOKOL_D3D11` | `sokol_*{_glcore, _gles3, _d3d11, _wgpu}` for `app`/`gfx`/`glue` |
| `wasm_emscripten` | `SOKOL_GLES3` | `sokol_*{_gles3, _wgpu}` for `app`/`gfx`/`glue` |

`sokol_gp` is built against the vendored sokol headers in `deps/sokol_gp/thirdparty`, which are older than the top-level `deps/sokol` submodule. `sokol_gp` variants are therefore `*_glcore`, `*_gles3`, `*_metal`, and `*_d3d11` only, and they must not be mixed with the top-level `sokol_gfx`/`sokol_app`/`sokol_glue` libraries. This is documented in the installed `include/sokol_gp/README.txt`.

`sokol_gp_wgpu` is not provided because the vendored sokol headers predate the current `deps/dawn` `webgpu.h` API. It will become available once upstream `sokol_gp` updates its vendored sokol headers, or once we patch `sokol_gp` to match the current top-level `sokol`/`dawn` versions.

### 3.8 Windows build machine requirements

Windows builds are currently tested on an arm64 Windows VM. The host must have:

- **Visual Studio 2022** (or Build Tools) with the C++ workload installed.
  - For `windows_x64`: the MSVC x64/x86 cross-compiler or native x64 compiler.
  - For `windows_arm64`: the MSVC arm64 compiler (available in VS 2022 17.4+).
- **CMake** 3.25 or newer.
- **Ninja** (recommended). `build_all.sh` will use Ninja if it is in PATH; otherwise it falls back to `NMake Makefiles` or `NMake Makefiles JOM` if `jom` is available.
- **Python 3** (used by some helper scripts).

Run `scripts/build_all.sh` from a **Native Tools Command Prompt** for the desired architecture (e.g., `x64 Native Tools Command Prompt` or `arm64 Native Tools Command Prompt`) so that `cl.exe` and the MSVC environment variables are available.

Because MSVC is not available on macOS or Linux hosts, `windows_x64` and `windows_arm64` cannot be built from this macOS development machine; they must be built on a Windows host or in a Windows CI runner.

---

## 4. Special Cases & Exclusions

| Dependency | Issue | Resolution |
|---|---|---|
| `ghostty` | Uses Zig build; upstream only emits `libghostty.a` inside an xcframework on macOS. | Built for `macos_arm64` only via `src/ghostty/CMakeLists.txt`; the source is copied to the build tree and `libghostty.a` is extracted from the xcframework. |
| `mtcc` | Makefile-based C compiler; target-specific C/ASM cannot compile to Emscripten/WASM. | Wrapped in `src/mtcc/CMakeLists.txt`. **Exclude** from `wasm_emscripten`. Build on Windows/Linux/macOS natively. |
| `dawn` | WebGPU; heavy. | Built via `ExternalProject_Add` with `DAWN_FETCH_DEPENDENCIES=OFF`. Dawn's third-party dependencies are pre-populated as git submodules under `deps/dawn_third_party/` and `DAWN_THIRD_PARTY_DIR` points there. On native platforms a monolithic static library is produced. On Emscripten `scripts/install_dawn.cmake` stages the `emdawnwebgpu` headers and JS files. |
| `boringssl` | CMake-based build. | Built via `ExternalProject_Add`. Used as the TLS backend for `curl` and `libwebsockets`. `OPENSSL_NO_ASM=ON` on Emscripten. |
| `lua` | Makefile only, no CMake. | Wrapped in `src/lua/CMakeLists.txt` so the build is driven by CMake. |
| `cimgui` | Upstream `CMakeLists.txt` hard-codes `SHARED`. | Wrapped in `src/cimgui/CMakeLists.txt` to build a static library from the cimgui/ImGui sources. |
| `skribidi` | Upstream fetches unpinned `harfbuzz`/`SheenBidi`/`libunibreak`/`budouxc`. | Built from `src/skribidi/` wrapper that depends on the pinned submodules under `deps/`. |
| `curl` | Needs TLS backend. | Use BoringSSL via `CURL_USE_OPENSSL=ON` (BoringSSL is OpenSSL-compatible). |
| `harfbuzz` | Optional FreeType interdependency. | Build FreeType before HarfBuzz and set `HB_HAVE_FREETYPE=ON` / `FT_DISABLE_HARFBUZZ=OFF`. |
| `glfw` on Emscripten | GLFW is not used on the web; SDL3 or emscripten HTML5 APIs are used. | **Exclude** from `wasm_emscripten`. |
| `libwebsockets` on Emscripten | Relies on BSD sockets, not available in the browser. | **Exclude** from `wasm_emscripten`. |
| `reproc` on Emscripten | Spawning processes is not supported on the web. | **Exclude** from `wasm_emscripten`. |
| `tinycsocket` on Emscripten | No BSD sockets. | **Exclude** from `wasm_emscripten`. |
| `enet` on Emscripten | UDP sockets not available in the browser. | **Exclude** from `wasm_emscripten`. |
| `libwebsockets` on Emscripten | Relies on BSD sockets, not available in the browser. | **Exclude** from `wasm_emscripten`. |
| `reproc` on Emscripten | Spawning processes is not supported on the web. | **Exclude** from `wasm_emscripten`. |
| `tinycsocket` on Emscripten | No BSD sockets. | **Exclude** from `wasm_emscripten`. |
| `minigamepad` on Emscripten | Gamepad API exists but the library may need platform-specific backend. | Evaluate; if not straightforward, **exclude** from `wasm_emscripten`. |

### 4.1 Dynamic library variants (optional future)
For dependencies where downstream projects need shared libs, we can build them with `-DBUILD_SHARED_LIBS=ON`.  The plan is to first produce static libraries; shared variants will be produced in the same `_b/<platform>` layout but marked as `shared` in the output directory.  This will be configurable in `build_all.sh` and in the top-level CMake via `BUILD_SHARED_LIBS`.

---

## 5. Release & CI Strategy (future phase)

### 5.1 Artifact format
Each artifact is a zip file named like:
```
moredeps-<commit>-<platform>-<dep>-<version>.zip
```
Contents:
```
<dep>/
  lib/<static-or-shared-libs>
  include/<public headers>
  share/<optional CMake configs>
  LICENSE
```

### 5.2 Manifest JSON
`docs/build_manifest.json` (or similar):
```json
{
  "repo_commit": "abc123",
  "generated_at": "2026-07-12T...",
  "artifacts": {
    "sokol": {
      "linux_x64": {
        "commit": "3743ea6...",
        "artifact_hash": "sha256:...",
        "filename": "moredeps-abc123-linux_x64-sokol-3743ea6.zip",
        "built": true
      },
      "wasm_emscripten": { "built": false, "reason": "N/A" }
    }
  }
}
```

### 5.3 Incremental builds
The CI will compare the resolved submodule commit hash of each dependency against the manifest.  If unchanged, the existing artifact is reused.  If changed, only that dependency is rebuilt for the affected platforms.  The first run will build everything because the manifest is empty.

### 5.4 GitHub Releases
- Each release tag corresponds to the repo commit hash (e.g., `build-abc123`).
- Artifacts are attached to the release.
- The manifest is committed to `docs/` after a successful release, so the next CI run can detect changes.

---

## 6. Implementation Phases

### Phase 1 — Review & Plan (this document)
- Audit all dependencies for build systems, platform support, and version pins.
- Get user approval on the plan.

### Phase 2 — Foundation
- Create `docs/` and `scripts/` directories.
- Create CMake toolchain files for all six targets (or stubs when toolchains are unavailable locally).
- Create the top-level `CMakeLists.txt` with per-dependency sections and options.
- Create `src/<dep>/` wrappers for all header-only / non-CMake dependencies.
- Create `scripts/build_all.sh` with platform detection, argument parsing, and CMake dispatch.
- **Non-CMake deps** (`lua`): use a CMake `add_custom_command` wrapper in `src/lua/` only if unavoidable. For `lua`, `build_all.sh` will invoke the canonical `make` build and stage the results into `_out/<platform>/`. All other dependencies, including BoringSSL, are driven by CMake.

### Phase 3 — Local Validation
- macOS arm64: build all CMake deps and header-only wrappers.
- Linux x64 and arm64 (VM): build all CMake deps and validate cross toolchain.
- Windows x64 / arm64 (VM): build all CMake deps with MSVC.
- Emscripten (local): build all Emscripten-compatible deps and validate Dawn `emdawnwebgpu` path.
- Fix failures, document exclusions.

### Phase 4 — CI & Release Automation
- GitHub Actions workflow matrix covering all platforms.
- Artifact packaging, hashing, JSON manifest generation.
- Release upload step.
- Incremental-build logic based on manifest diff.

### Phase 5 — Mobile Expansion
- Add `ios_arm64`, `ios_simulator`, `android_arm64`, `android_x64` targets.
- Extend toolchains and the manifest.

---

## 7. Production Standards & Guardrails

1. **No hacks.** Every build path must be documented in the plan and in `docs/build_notes.md` (to be created).  If a dependency requires a workaround, we either:
   - Upstream the fix / pin a fork with a proper fix, or
   - Reject building that dep on that platform and document it as unsupported.
2. **Reproducible.** Submodule commits are pinned.  Build scripts fail on missing toolchains rather than fall back to the host compiler silently.
3. **Observable.** `build_all.sh` prints a summary at the end: built, skipped (with reason), failed.
4. **Isolated.** Each platform build is in its own directory.  No build writes to `deps/` or to the source tree.
5. **Static-first.** By default build static libraries.  Shared libraries are an explicit opt-in and are not part of this initial phase.
6. **Dependency graph aware.** If a downstream dep needs headers from another dep during build, we use CMake `target_link_libraries` and install/include paths correctly, not `-I` hacks.
7. **Cross-platform tooling.** Toolchain files are provided; no hidden `CC=gcc` assumptions in `build_all.sh`.
8. **Clean.** Remove generated files from `deps/` if any build step accidentally writes there.  `_b/` is gitignored; build outputs are staged to `_out/`.

---

## 8. Decisions (answered)

The following decisions have been made and are recorded here for reference.

1. **Build directory:** `_b` (already gitignored).  
   **Output directory:** `_out` (staged static libraries + headers).
2. **Shared libraries:** Deferred to a later phase; **not** part of this plan or initial implementation.
3. **Examples / tests / docs / tools:** Disabled for every dependency by default.  The per-dependency defaults are recorded in `docs/build_options.md`.
4. **TLS backend:** Use **BoringSSL** (`deps/boringssl`) as the single TLS backend for `curl` and `libwebsockets` on all platforms. This is consistent and removes the need for platform-specific TLS backends.
5. **HarfBuzz ↔ FreeType:** Enable `HB_HAVE_FREETYPE=ON` and `FT_DISABLE_HARFBUZZ=OFF` for improved auto-hinting. FreeType is built before HarfBuzz.
6. **Version pins:** Yes — pin all floating submodules to concrete tags or commits.
6. **Ghostty:** Built for `macos_arm64` only. The wrapper copies the source to the build tree, runs `zig build -Dapp-runtime=none -Demit-macos-app=false -Demit-xcframework=true`, and installs `libghostty.a` from the resulting xcframework along with `include/ghostty.h` and the `include/ghostty/` directory.
7. **Dawn scope:** WebGPU-only. Disable examples, tests, benchmarks, samples, node bindings, SwiftShader, and protobuf. Use `DAWN_BUILD_MONOLITHIC_LIBRARY=STATIC` and `DAWN_ENABLE_INSTALL=ON` on native platforms. On Emscripten, `DAWN_ENABLE_INSTALL=OFF` and `DAWN_BUILD_MONOLITHIC_LIBRARY=OFF`; `scripts/install_dawn.cmake` stages `emdawnwebgpu` headers and JS files. `DAWN_FETCH_DEPENDENCIES=OFF`; third-party dependencies are pre-populated as git submodules in `deps/dawn_third_party/` and `DAWN_THIRD_PARTY_DIR` is set to that path.
8. **Non-CMake deps:** Create small CMake wrappers in `src/<dep>/` (one wrapper per dependency, or per module for `sokol`/`stb`). Makefile-only deps (`mtcc`) are also wrapped in `src/<dep>/` so the super-build controls them.
9. **Emscripten SDK:** The toolchain file (`toolchain/wasm_emscripten.cmake`) locates the SDK under `libexec/` (matching Homebrew's layout) and sets `CMAKE_SYSTEM_NAME=Emscripten`.
10. **CI matrix:** Deferred to the CI phase. Local validation is complete for `macos_arm64` and `wasm_emscripten`; `linux_x64`/`linux_arm64`/`windows_x64`/`windows_arm64` remain to be validated on appropriate hosts.
11. **Version pins:** All `branch = ...` entries removed from `.gitmodules`; submodules are pinned to their current commits.

### Remaining open questions

1. **Dawn Emscripten option name:** Verify the exact CMake variable for selecting `emdawnwebgpu` during implementation.
2. **Header-only library scope:** For `sokol` and `stb`, create a separate static library per module (e.g., `sokol_app`, `sokol_gfx`, `stb_image`, `stb_truetype`) so downstream apps only link what they use.

---

## 9. Immediate Next Steps

1. Validate `linux_x64`, `linux_arm64`, `windows_x64`, and `windows_arm64` builds on appropriate hosts.
2. Add CI matrix and artifact packaging in a future phase.

