# CI/CD Plan for moredeps

**Status:** Draft — pending review before implementation.  
**Purpose:** Define the GitHub Actions workflow that builds all platform targets, produces release artifacts, generates the manifest JSON, and makes everything available to the GitHub Pages download site.

---

## 1. Goals

1. **Manual trigger only** (`workflow_dispatch`) — no automatic runs on push/PR. CI minutes are limited.
2. **6 targets** across 3 parallel jobs:
   - `linux_x64`, `linux_arm64`, `wasm_emscripten` (Ubuntu runner)
   - `macos_arm64` (macOS runner)
   - `windows_x64`, `windows_arm64` (Windows runner)
3. **Incremental builds** — skip dependencies whose submodule commit hasn't changed since the last release.
4. **Artifact packaging** — each dependency/platform combo produces a zip with `lib/`, `include/`, `LICENSE`.
5. **Manifest JSON** — per-release asset listing all artifacts, commit hashes, build status, durations.
6. **GitHub Releases** — immutable `build-<sha>` tags + rolling `latest` alias.
7. **Web integration** — Pages site (`deps.morew4rd.com`) fetches manifest from release at runtime.

---

## 2. Job Matrix

| Job | Runner | Targets | Est. cold | Est. warm |
|-----|--------|---------|-----------|-----------|
| `build-linux` | `ubuntu-24.04` | `linux_x64`, `linux_arm64`, `wasm_emscripten` | 35-50 min | 8-15 min |
| `build-macos` | `macos-15` | `macos_arm64` | 25-35 min | 5-10 min |
| `build-windows` | `windows-2022` | `windows_x64`, `windows_arm64` | 30-45 min | 5-10 min |

**Wall clock:** ~35-50 min (all parallel). Warm builds (cached submodules + ccache) drop to ~8-15 min.

**Total: 6 targets.**

---

## 3. Submodule Checkout (CRITICAL)

**DO NOT** use `actions/checkout` with `submodules: true` or `submodules: recursive`. Dawn's nested submodules include private Google repos (`chrome-internal.googlesource.com`) that require authentication and will fail the checkout.

**Correct approach:**
```yaml
- uses: actions/checkout@v4
  with:
    submodules: false

- run: git submodule update --init --jobs 4
```

This initializes only the top-level submodules (56 total) and does not recurse into dawn's private submodules. The `dawn_third_party/` directory is a separate top-level submodule that provides the needed dependencies.

---

## 4. Caching Strategy

### 4.1 Submodule cache (Tier 1)

Cache the entire `deps/` directory keyed by submodule commit hashes. Invalidates only when a submodule changes.

```yaml
- uses: actions/cache@v4
  with:
    path: deps/
    key: submodules-${{ hashFiles('.gitmodules') }}-${{ hashFiles('deps/*/HEAD') }}
    restore-keys: |
      submodules-${{ hashFiles('.gitmodules') }}-
```

**First run:** 3-5 min to clone all submodules.  
**Subsequent runs:** ~30-60 seconds to verify/update changed submodules.

### 4.2 Build cache (Tier 2)

Use `ccache` (Linux/macOS) or `sccache` (Windows) to cache compiled object files. Keyed by source file hashes.

```yaml
- uses: actions/cache@v4
  with:
    path: ~/.cache/ccache
    key: ccache-${{ runner.os }}-${{ hashFiles('CMakeLists.txt', 'src/**/*.c', 'src/**/*.h', 'src/**/*.cpp') }}
```

**Impact:** Reduces rebuild time by 60-80% when only a few deps change.

---

## 5. Incremental Build Logic

### 5.1 Manifest as source of truth

The manifest JSON (`build_manifest.json`) lives as a **GitHub Release asset** attached to the `latest` release. It records the submodule commit hash for each dependency at the time it was built.

**Per-dependency decision:**
1. Read `deps/<name>/HEAD` → current commit
2. Look up `<name>` → `<platform>` → `commit` in manifest
3. If match → **skip build**, reuse artifact from previous release
4. If mismatch or missing → **build**, create new zip, update manifest
5. If dependency is excluded on this platform → mark `built: false` with `reason`

### 5.2 Artifact naming

```
moredeps-<repo-sha>-<platform>-<dep>-<dep-commit>.zip
```

Example: `moredeps-abc1234-linux_arm64-dawn-b87a8b9.zip`

This makes artifacts immutable and traceable. The manifest maps these filenames to download URLs.

### 5.3 First run behavior

On the first CI run, no manifest exists. All dependencies are built from scratch. The manifest is created and attached to the release.

---

## 6. Release Strategy

### 6.1 Two releases per build

| Release | Type | Purpose |
|---------|------|---------|
| `build-<short-sha>` | Immutable | Permanent record of what was built at this commit. Never modified. |
| `latest` | Rolling alias | Always points to the newest build. Overwritten on each run. Web page reads from here. |

### 6.2 Release assets

Each release contains:
- `build_manifest.json` — the master manifest
- `moredeps-<sha>-<platform>-<dep>-<commit>.zip` — one per built dependency/platform

### 6.3 Release creation flow

```
1. All 3 build jobs finish
2. `release` job downloads all artifacts from build jobs
3. Generate manifest.json from build results + previous manifest (for skipped deps)
4. Create `build-<sha>` release with all zips + manifest
5. Update `latest` release (delete old assets, upload new ones)
```

### 6.4 Failure handling

**Fail fast:** If any platform job fails, the `release` job does not run. No partial releases are created. This prevents broken artifacts from being published.

Future enhancement: per-platform releases so one failure doesn't block all platforms.

---

## 7. Manifest JSON Format

```json
{
  "repo_commit": "abc1234def5678",
  "generated_at": "2026-07-14T12:34:56Z",
  "build_duration_seconds": 1847,
  "artifacts": {
    "dawn": {
      "linux_x64": {
        "commit": "b87a8b9b6489ef2a62094a75e0c10effdcdedb88",
        "artifact_hash": "sha256:abc123...",
        "filename": "moredeps-abc1234-linux_x64-dawn-b87a8b9.zip",
        "built": true,
        "build_duration_seconds": 420,
        "log_url": "https://github.com/moreward/moredeps/actions/runs/1234567890"
      },
      "windows_arm64": {
        "built": false,
        "reason": "excluded"
      }
    },
    "mtcc": {
      "windows_x64": {
        "commit": "f6c9639...",
        "artifact_hash": "sha256:def456...",
        "filename": "moredeps-abc1234-windows_x64-mtcc-f6c9639.zip",
        "built": true,
        "build_duration_seconds": 45
      },
      "windows_arm64": {
        "built": false,
        "reason": "TinyCC PE backend lacks ARM64 support"
      }
    }
  }
}
```

Fields:
- `repo_commit` — the moredeps repo commit being built
- `generated_at` — ISO 8601 timestamp
- `build_duration_seconds` — total CI time
- `artifacts.<dep>.<platform>.commit` — submodule commit hash
- `artifacts.<dep>.<platform>.artifact_hash` — SHA-256 of the zip
- `artifacts.<dep>.<platform>.filename` — zip filename
- `artifacts.<dep>.<platform>.built` — `true` (built), `false` (skipped/excluded), or missing (failed)
- `artifacts.<dep>.<platform>.reason` — human-readable reason for skip/exclusion
- `artifacts.<dep>.<platform>.build_duration_seconds` — per-dep build time
- `artifacts.<dep>.<platform>.log_url` — link to GitHub Actions run

---

## 8. Web Page Integration

The GitHub Pages site (`deps.morew4rd.com`) is a static `index.html` that:

1. Fetches `https://api.github.com/repos/moreward/moredeps/releases/latest`
2. Finds the `build_manifest.json` asset in the release
3. Downloads and parses the manifest
4. Renders a table: rows = dependencies, columns = platforms
5. Each cell shows: download link (if built), N/A with reason (if excluded), or failed status

**No commits to the repo are needed.** The page is completely dynamic based on the release data.

The Pages workflow uses **sparse checkout** (`docs/` only) and **no submodules**, so it completes in ~20 seconds and doesn't hit the dawn recursive submodule issue.

---

## 9. Workflow Structure

```yaml
name: Build All Platforms

on:
  workflow_dispatch:
    inputs:
      platforms:
        description: 'Platforms to build (all, linux, macos, windows, or comma-separated list)'
        default: 'all'
        required: true

jobs:
  build-linux:
    runs-on: ubuntu-24.04
    if: contains(inputs.platforms, 'all') || contains(inputs.platforms, 'linux')
    steps:
      - checkout (no submodules)
      - init submodules (--jobs 4)
      - cache deps/
      - cache ccache
      - build linux_x64
      - build linux_arm64
      - build wasm_emscripten
      - upload platform artifacts

  build-macos:
    runs-on: macos-15
    if: contains(inputs.platforms, 'all') || contains(inputs.platforms, 'macos')
    steps:
      - checkout (no submodules)
      - init submodules (--jobs 4)
      - cache deps/
      - cache ccache
      - build macos_arm64
      - upload platform artifacts

  build-windows:
    runs-on: windows-2022
    if: contains(inputs.platforms, 'all') || contains(inputs.platforms, 'windows')
    steps:
      - checkout (no submodules)
      - init submodules (--jobs 4)
      - cache deps/
      - cache sccache
      - build windows_x64
      - build windows_arm64
      - upload platform artifacts

  release:
    needs: [build-linux, build-macos, build-windows]
    runs-on: ubuntu-24.04
    steps:
      - checkout (no submodules)
      - download all artifacts from build jobs
      - fetch previous manifest from latest release
      - generate new manifest (merge built + skipped from previous)
      - create zips with lib/ include/ LICENSE
      - create build-<sha> release
      - update latest release (rolling alias)
      - upload all zips + manifest
```

---

## 10. Platform-Specific Notes

### Linux x64
- Not yet validated locally. First CI run will be the test.
- Expected to work identically to `linux_arm64` (same toolchain, different `CMAKE_SYSTEM_PROCESSOR`).

### Linux arm64
- Cross-compiles from x64 Ubuntu runner using `aarch64-linux-gnu-gcc`.
- Requires cross-compiler packages (installed via `apt` in workflow).

### macOS arm64
- Native build on `macos-15` runner (Apple Silicon).

### Windows x64
- MSVC via `windows-2022` runner.
- `setup_vcvars.sh` auto-detects VS installation and sets up environment.

### Windows arm64
- Cross-compile from x64 Windows host using MSVC arm64 compiler.
- `mtcc` excluded (no ARM64 PE backend).
- BoringSSL `OPENSSL_NO_ASM=ON` required.

### Emscripten/WASM
- Requires Emscripten SDK installation on Ubuntu runner.
- Dawn produces no static library; `install_dawn.cmake` stages headers/JS files.
- Several deps excluded: `glfw`, `mtcc`, `enet`, `libwebsockets`, `reproc`, `tinycsocket`.

---

## 11. Open Questions

1. **Linux x64 validation:** Will it work on the first CI run? If not, what's the fix?
2. **Emscripten SDK caching:** Should we cache the EMSDK installation (~500MB) or install fresh each time?
3. **Windows cache:** `sccache` vs `ccache` — which works better with MSVC?
4. **Artifact size:** Each zip is ~1-5MB. With 50+ deps × 6 platforms = ~300 zips per release. Is this acceptable?
5. **Retention:** GitHub Releases have no storage limit for public repos, but should we clean old `build-<sha>` releases?

---

## 12. Implementation Order

1. Write `.github/workflows/build.yml` (the main CI workflow)
2. Write `scripts/ci_build.sh` — wrapper that handles incremental logic per platform
3. Write `scripts/ci_package.sh` — creates zips from `_out/<platform>/`
4. Write `scripts/ci_manifest.py` — generates/merges manifest JSON
5. Test with `workflow_dispatch` on `linux` only first
6. Expand to all platforms once Linux works
7. Verify web page renders correctly from `latest` release

---

## 13. Related Files

- `.github/workflows/pages.yml` — GitHub Pages deployment (sparse checkout, no submodules)
- `docs/index.html` — Download page (fetches manifest from release at runtime)
- `docs/CNAME` — Custom domain (`deps.morew4rd.com`)
- `scripts/build_all.sh` — Local build entry point (used by CI)
- `scripts/setup_vcvars.sh` — Windows MSVC environment setup
