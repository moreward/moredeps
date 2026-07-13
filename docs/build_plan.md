# Build Infrastructure Plan for `moredeps`

**Status:** Review draft (not yet implemented).  
**Purpose:** Define the production-grade build system that produces static (and optionally shared) libraries for every dependency across all supported host/target combinations, with deterministic versioning and releasable artifacts.

---

## 1. Goals & Constraints

### 1.1 Targets (initial matrix)
We must produce static libraries for every buildable dependency on the following platforms:

| Platform | Arch | Toolchain notes |
|---|---|---|
| Windows | x64 | MSVC or clang-cl via CMake |
| Windows | arm64 | MSVC or clang-cl cross-compile |
| Linux | x64 | GCC or Clang native |
| Linux | arm64 | GCC/Clang cross-compile (e.g., `aarch64-linux-gnu`) |
| macOS | arm64 | Apple Clang native (Apple Silicon) |
| Emscripten | wasm32 | `emcc`/`emcmake`/`emmake` |

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
- Capture and record things that cannot build this way (e.g., `ghostty`).
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
| `box3d` | CMake | C | Top-level CMake | Verify Windows/macOS/Linux cross-builds. |
| `cglm` | CMake / Meson / Autotools | C | Top-level CMake | Math lib, no tricky deps. |
| `cgltf` | Header-only | H | `src/cgltf/*.c` with `CGTF_IMPLEMENTATION` | glTF loader. |
| `cimgui` | CMake / Makefile | C++ wrapper | `src/cimgui/` wrapper | Upstream `CMakeLists.txt` hard-codes `SHARED`; we build static from source. |
| `cJSON` | CMake / Makefile | C | Top-level CMake | None known. |
| `curl` | CMake / Autotools | C | Top-level CMake | Use BoringSSL (`CURL_USE_BORINGSSL=ON`) on all platforms. |
| `dawn` | CMake / Bazel | C | Top-level CMake | Emscripten must use `emdawnwebgpu` instead of `USE_WEBGPU`. Very heavy; may need options to reduce build (disable tests, samples). |
| `enet` | CMake | C | Top-level CMake | Network lib. Check Windows/Wasm viability. |
| `FastNoiseLite` | Header-only | H | `src/FastNoiseLite/*.c` | C++ template header also available; we use the C header. |
| `flecs` | CMake / Meson / Bazel | C | Top-level CMake | ECS library, builds as C lib. |
| `fontstash` | Header-only | H | `src/fontstash/*.c` | Font rasterization. Usually needs `FONTSTASH_IMPLEMENTATION`. |
| `freetype` | CMake / Meson / Makefile / Autotools | C | Top-level CMake | Common dependency; keep build minimal. |
| `ghostty` | Zig build / Makefile | Z | CMake `IMPORTED` target via `zig build` | Build `libghostty`. Requires patched `zig` 0.15.2. |
| `glfw` | CMake | C | Top-level CMake | Windowing. Does not make sense on Emscripten? Web platform usually replaces GLFW. |
| `harfbuzz` | CMake / Meson | C | Top-level CMake | Text shaping. Often depends on FreeType. Build order / dependency graph matters. |
| `libwebsockets` | CMake | C | Top-level CMake | Depends on BoringSSL (`LWS_WITH_BORINGSSL=ON`). Windows/macOS/Linux OK; Emscripten unlikely. |
| `lua-5.5.0` | Makefile | M | Custom Makefile step (not CMake) | No CMake; must be built directly. Lua is compiled as C. |
| `lz4` | Makefile | M | `src/lz4/` wrapper (uses `build/cmake`) | Use CMake wrapper in `deps/lz4/build/cmake/`. |
| `microui` | Single `.c` + header | H | `src/microui/*.c` | Simple UI. |
| `mimalloc` | CMake | C | Top-level CMake | Override allocator. Must be careful with build modes (secure, debug). |
| `miniaudio` | CMake / header-only | C / H | Top-level CMake or wrapper | Check if CMake is upstream; otherwise `src/miniaudio/*.c` with `MINIAUDIO_IMPLEMENTATION`. |
| `minigamepad` | Makefile / header-only | M / H | `src/minigamepad/*.c` or Makefile | Gamepad input abstraction. |
| `mtcc` | Makefile / Autotools | M | Makefile or custom CMake | **Cannot build with Emscripten** (uses target-specific C code). Documented exclusion. |
| `nanovg` | `.c` + header | H | `src/nanovg/*.c` | 2D vector graphics. |
| `boringssl` | CMake / Bazel | C | Top-level CMake | Replaces OpenSSL. Built as static TLS backend for `curl` and `libwebsockets`. |
| `physfs` | CMake | C | Top-level CMake | File system abstraction. |
| `raudio` | `.c` + header | H | `src/raudio/*.c` | Audio. |
| `raylib` | CMake / Zig | C | Top-level CMake | Game framework; may pull many subsystems. Build minimal config. |
| `reproc` | CMake | C | Top-level CMake | Process library. Windows/Linux/macOS; Emscripten likely excluded. |
| `sdl3` | CMake | C | Top-level CMake | Core windowing/input/audio. Emscripten build possible but uses special flags. |
| `sdl3webgpu` | CMake | C | Top-level CMake | Depends on SDL3 and Dawn/WebGPU. Emscripten path may differ. |
| `skribidi` | CMake | C | Top-level CMake | Text library. |
| `sokol` | Header-only (with optional CMake tools) | H | `src/sokol_<mod>/` wrappers | Per-module static libraries: `sokol_app`, `sokol_gfx`, `sokol_audio`, `sokol_time`, `sokol_log`, `sokol_args`, `sokol_fetch`, `sokol_glue`. |
| `sokol_gp` | Makefile | H | `src/sokol_gp/` wrapper | Header-only style; create implementation `.c`. |
| `sqlite-amalgamation` | CMake | C | Top-level CMake | Two files, trivial build. |
| `stb` | Header-only | H | `src/stb_<lib>/` wrappers | Per-module static libraries: `stb_image`, `stb_image_write`, `stb_image_resize`, `stb_truetype`, `stb_rect_pack`, `stb_ds`, etc. |
| `tinycsocket` | CMake | C | Top-level CMake | Tiny cross-platform sockets; Emscripten may be excluded. |
| `tracy` | CMake / Meson | C | Top-level CMake | Profiling. Usually optional in clients; build client library. |
| `ubench` | Header-only | H | `src/ubench/*.c` | Micro-benchmarking. |
| `utest` | Header-only | H | `src/utest/*.c` | Unit testing. |
| `utf8proc` | CMake / Makefile | C | Top-level CMake | Unicode text processing. |
| `xxhash` | Makefile | M | `src/xxhash/` wrapper (uses `cmake_unofficial`) | Use CMake wrapper in `deps/xxhash/cmake_unofficial/`. |
| `zlib` | CMake / Makefile / Autotools / Bazel | C | Top-level CMake | Compression. |
| `zstd` | Makefile | M | `src/zstd/` wrapper (uses `build/cmake`) | Use CMake wrapper in `deps/zstd/build/cmake/`. |

### 2.1 Dependency ordering concerns
Some dependencies consume others; the build order matters if we link tests/examples, but for pure static-library output each can be built independently.  However, for libraries that *offer* optional features (e.g., `curl` with SSL, `harfbuzz` with FreeType), we need to decide per-platform defaults and document them.  Where a feature requires another dep, we may need to build the provider first and pass paths in.

### 2.2 Reproducibility / version pinning
Several submodules currently point to branch heads (`stb`, `nanovg`, `fontstash`, `minigamepad`, `ubench`, `utest`, `mtcc`, `sokol`, `sokol_gp`, `skribidi`, etc.).  For a production dependency repo, we must pin each submodule to a concrete tag or commit.  The CI/release JSON manifest will record the resolved commit hash for each dependency, but the `.gitmodules` or submodule commits should be pinned as well.

---

## 3. Build Infrastructure Design

### 3.1 Repository layout after this work

```
moredeps/
├── CMakeLists.txt              # Top-level driver for all CMake deps
├── scripts/
│   └── build_all.sh            # Entry point for all combinations
├── src/                        # Implementation files for header-only / non-CMake deps
│   ├── cgltf/
│   │   └── cgltf.c             # #define CGLTF_IMPLEMENTATION / #include "cgltf.h"
│   ├── FastNoiseLite/
│   ├── fontstash/
│   ├── microui/
│   ├── miniaudio/
│   ├── minigamepad/
│   ├── nanovg/
│   ├── raudio/
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
│   ├── ubench/
│   └── utest/
├── toolchain/                  # CMake toolchain files (optional but recommended)
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
│   └── build_plan.md           # This file
├── deps/                       # Git submodules (read-only source)
└── .github/workflows/          # CI (future phase)
```

### 3.2 Top-level `CMakeLists.txt`

The top-level `CMakeLists.txt` is **not** a monolithic build of all deps at once.  It is a dispatcher that, given the current platform and a selected subset, calls `add_subdirectory(deps/<dep>)` or builds a wrapper target in `src/<dep>` for each dependency that is configured to build on this platform.

Key contents:

1. **Minimum CMake version** and project declaration.
2. **Policy / defaults**:
   - `CMAKE_POSITION_INDEPENDENT_CODE ON` (needed for later shared libs and safe for static too).
   - Disable in-source builds.
   - Default to `Release` if not specified.
3. **Platform detection** from the toolchain / environment variables.  We will use CMake toolchain files for cross-compilation so the build directory encodes the platform.
4. **Per-dependency build sections**.  Each section is guarded by an option like `MOREDEPS_BUILD_<DEP>_<PLATFORM>` or a single `MOREDEPS_BUILD_<DEP>` that is platform-aware.  Example:
   ```cmake
   option(MOREDEPS_BUILD_SOKOL_APP "Build sokol_app static library" ON)
   if(MOREDEPS_BUILD_SOKOL_APP)
       add_subdirectory(src/sokol_app)   # wrapper, not deps/sokol
   endif()
   ```
5. **For CMake-native deps** (e.g., `zlib`, `freetype`, `sdl3`), we will `add_subdirectory(deps/<dep>)` with the right options, possibly with `EXCLUDE_FROM_ALL` to avoid building tests/examples.
6. **For non-CMake deps** (currently only `lua`), `build_all.sh` will call the native build and install the outputs into a predictable layout, or we will create a CMake `add_custom_command` wrapper that invokes the native build.  BoringSSL is CMake-native and will be driven by the top-level `CMakeLists.txt`.
7. **Header-only wrappers** live in `src/<dep>/CMakeLists.txt` and create a real `STATIC` library that includes the implementation file(s).

### 3.3 `scripts/build_all.sh`

Responsibilities:

1. Detect the host OS and available toolchains (fail fast if a requested toolchain is missing).
2. Parse arguments to build one or all combos:
   ```bash
   ./scripts/build_all.sh all
   ./scripts/build_all.sh macos_arm64
   ./scripts/build_all.sh windows_x64
   ```
3. For each platform, ensure the CMake toolchain file is used.
4. Create `_b/<platform>` and run:
   ```bash
   cmake -S . -B _b/<platform> -DCMAKE_TOOLCHAIN_FILE=toolchain/<platform>.cmake ...
   cmake --build _b/<platform> --config Release
   ```
5. For non-CMake deps, call their native build within `_b/<platform>/<dep>/` (e.g., `lua` make).
6. After building, stage the resulting static libraries into a platform directory under `_b/<platform>/install/` or `out/<platform>/`.
7. Record which dependencies were skipped due to known exclusions.

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
- Some deps (e.g., `glfw`, `reproc`, `tinycsocket`, `mtcc`) will be **excluded** from Emscripten because they use platform APIs not available on the web.

---

## 4. Special Cases & Exclusions

| Dependency | Issue | Resolution |
|---|---|---|
| `ghostty` | Uses Zig build, not CMake. | Build `libghostty` via CMake `add_custom_command` + `IMPORTED` target using patched `zig` 0.15.2.  If a clean static library cannot be produced, document as unsupported. |
| `mtcc` | Uses target-specific C code / inline asm that cannot compile to Emscripten/WASM. | **Exclude** from `wasm_emscripten`. Build on Windows/Linux/macOS natively. |
| `dawn` on Emscripten | Must select `emdawnwebgpu`, not `USE_WEBGPU`. | Configure in the Emscripten toolchain or per-dep section in top-level CMake. |
| `boringssl` | CMake-based build. | Build as a static library via top-level CMake. Used as the TLS backend for `curl` and `libwebsockets`. |
| `lua` | Makefile only, no CMake. | Build with `make` in `_b/<platform>/lua/` using the platform compiler. |
| `lz4`, `xxhash`, `zstd`, `sokol_gp` | Makefile only (but have CMake in subdirectories). | Use the CMake wrappers in `deps/lz4/build/cmake/`, `deps/xxhash/cmake_unofficial/`, `deps/zstd/build/cmake/`, or create `src/<dep>/` wrappers. |
| `curl` | Needs TLS backend. | Use BoringSSL (`CURL_USE_BORINGSSL=ON`) on all platforms. |
| `harfbuzz` | Optional FreeType interdependency. | Build FreeType before HarfBuzz and set `HB_HAVE_FREETYPE=ON` / `FT_DISABLE_HARFBUZZ=OFF`. |
| `glfw` on Emscripten | GLFW is not used on the web; SDL3 or emscripten HTML5 APIs are used. | **Exclude** from `wasm_emscripten`. |
| `libwebsockets` on Emscripten | Relies on BSD sockets, not available in the browser. | **Exclude** from `wasm_emscripten`. |
| `reproc` on Emscripten | Spawning processes is not supported on the web. | **Exclude** from `wasm_emscripten`. |
| `tinycsocket` on Emscripten | No BSD sockets. | **Exclude** from `wasm_emscripten`. |
| `enet` on Emscripten | UDP sockets not available in the browser. | **Exclude** from `wasm_emscripten`. |
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
6. **Ghostty:** Build `libghostty`.  Integrate via CMake by using `add_custom_command` / `add_custom_target` to invoke the patched `zig` 0.15.2 build, then expose the resulting library as an `IMPORTED` target.  Zig 0.15.2 must be installed on CI machines.
7. **Dawn scope:** WebGPU-only.  Disable examples, tests, benchmarks, samples, node bindings, SwiftShader, and protobuf.  Use `DAWN_BUILD_MONOLITHIC_LIBRARY=STATIC` and `DAWN_ENABLE_INSTALL=ON`.  On Emscripten, use `emdawnwebgpu` and do **not** set `USE_WEBGPU`.
8. **Non-CMake deps:** Create small CMake wrappers in `src/<dep>/` (one wrapper per dependency, or per module for `sokol`/`stb`). Native build systems (make) are only used when unavoidable, e.g. `lua`.
9. **Emscripten SDK:** Assume `emcc` / `emcmake` are installed and available at CMake configure time.  CI will install Emscripten before invoking the build.
10. **CI matrix:** Deferred to the CI phase.  For now, focus on local/VM validation of `macos_arm64` and `wasm_emscripten` here, and `linux_x64`/`linux_arm64`/`windows_x64`/`windows_arm64` on separate VMs.

### Remaining open questions

1. **Dawn Emscripten option name:** Verify the exact CMake variable for selecting `emdawnwebgpu` during implementation.
2. **Header-only library scope:** For `sokol` and `stb`, create a separate static library per module (e.g., `sokol_app`, `sokol_gfx`, `stb_image`, `stb_truetype`) so downstream apps only link what they use.

---

## 9. Immediate Next Steps

1. Review and approve this updated plan and `docs/build_options.md`.
2. Pin floating submodules to specific commits/tags.
3. Audit `docs/build_options.md` defaults and confirm or adjust them.
4. Begin Phase 2 implementation:
   - Create `toolchain/` files.
   - Create top-level `CMakeLists.txt` and `src/<dep>/` wrappers.
   - Create `scripts/build_all.sh`.

