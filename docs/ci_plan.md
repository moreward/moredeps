# CI/CD for moredeps

**Status:** Implemented and green (as of 2026-07-14).
**Purpose:** GitHub Actions workflow that builds all platform targets, packages per-dependency release artifacts, generates the manifest JSON, publishes GitHub Releases, and updates the GitHub Pages download site.

---

## 1. Overview

1. **Manual trigger only** (`workflow_dispatch`) — no automatic runs on push/PR. CI minutes are limited.
2. **6 targets, 6 parallel jobs** — all ARM64 targets build **natively** on GitHub's ARM runners; no cross-compilation in CI:
   - `linux_x64` (`ubuntu-24.04`), `linux_arm64` (`ubuntu-24.04-arm`)
   - `macos_arm64` (`macos-15`)
   - `windows_x64` (`windows-2022`), `windows_arm64` (`windows-11-arm`)
   - `wasm_emscripten` (`ubuntu-24.04`)
3. **Compiler caching** — ccache (Unix) / sccache (Windows) via `actions/cache`. This is the only incremental mechanism; every run builds everything (see §5).
4. **Artifact packaging** — each dependency/platform combo produces a zip with `lib/`, `include/`, `LICENSE`.
5. **Manifest JSON** — per-release asset listing all artifacts, pinned upstream commits, repo URLs, and exclusions.
6. **GitHub Releases** — immutable `build-<sha>` tags + rolling `latest` alias. Only the **last 3 builds** are retained; older releases are pruned automatically by the release job.
7. **Web integration** — Pages site (`deps.morew4rd.com`) serves the manifest **same-origin**; the build workflow redeploys the site after each release (see §8 for why).

Measured durations (warm cache): Linux ~7 min, macOS ~7.5 min, wasm ~10 min, Windows x64 ~16 min, Windows arm64 ~23 min. Cold cache: 30–70 min per platform.

---

## 2. Jobs

`.github/workflows/build.yml`:

| Job | Runner | Notes |
|-----|--------|-------|
| `configure` | `ubuntu-latest` | Parses the `platforms` input into per-job booleans |
| `build-linux-x64` | `ubuntu-24.04` | apt deps incl. X11/ALSA/GL dev packages, ccache |
| `build-linux-arm64` | `ubuntu-24.04-arm` | native ARM build |
| `build-wasm` | `ubuntu-24.04` | cached Emscripten SDK (pinned `EMSDK_VERSION`, see below) |
| `build-macos` | `macos-15` | brew ninja + ccache |
| `build-windows-x64` | `windows-2022` | choco ninja + sccache |
| `build-windows-arm64` | `windows-11-arm` | native ARM64 MSVC toolset |
| `release` | `ubuntu-24.04` | packages zips, manifest, publishes releases |
| `deploy-site` | `ubuntu-24.04` | redeploys Pages with the fresh manifest |

The `release` job runs only if every *requested* build job succeeded or was skipped — no partial releases. `deploy-site` runs only after a successful `release`.

### Platform selection

`workflow_dispatch` input `platforms`: `all`, `linux`, `macos`, `windows`, `wasm`, or comma-separated platform names. The `configure` job maps it to job-level `if:` conditions.

---

## 3. Submodule Checkout (CRITICAL)

**DO NOT** use `actions/checkout` with `submodules: true` or `recursive`. Dawn's nested submodules include private Google repos that require authentication and will fail the checkout.

Every build job does:

```yaml
- uses: actions/checkout@v4
  with:
    submodules: false
- run: |
    git submodule update --init --jobs 4
    cd deps/cimgui && git submodule update --init imgui   # nested, needed
```

A `deps/` submodule cache was tried and **removed** (2026-07-14): `actions/cache` cannot reliably preserve gitlink metadata, and restores kept producing broken submodule states. Fresh `git submodule update --init` takes ~3 min per job and is always correct.

---

## 4. Caching

### 4.1 Compiler cache (the mechanism that matters)

All build jobs export the CMake compiler launchers so every compile — including ExternalProject sub-builds, which inherit them via `MOREDEPS_COMMON_CMAKE_ARGS` in `CMakeLists.txt` — goes through the cache:

```bash
# Linux / macOS / wasm
export CMAKE_C_COMPILER_LAUNCHER=ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
# Windows
export CMAKE_C_COMPILER_LAUNCHER=sccache
export CMAKE_CXX_COMPILER_LAUNCHER=sccache
```

> **Hard-won lesson:** merely installing sccache and caching its directory does nothing without the launcher exports. Windows built cold for weeks because of this.

Cache configuration:

| Platform | Dir | Key | Restore prefix |
|----------|-----|-----|----------------|
| linux/mac/wasm | `${{ github.workspace }}/.ccache` (`CCACHE_DIR`, 2G max) | `ccache-<platform>-${{ hashFiles('CMakeLists.txt','src/**') }}` | `ccache-<platform>-` |
| windows | `${{ github.workspace }}/.sccache` (`SCCACHE_DIR`) | `sccache-<platform>-<same hash>` | `sccache-<platform>-` |

The key hashes only `CMakeLists.txt` and `src/**` — deliberately **not** `deps/**`. Dependency compile commands rarely change, so dep objects stay warm across runs via the prefix restore-key even when `src/` changes; only own sources recompile.

`actions/cache` only saves on **job success** (`post-if: success()`), so a platform's first cache exists only after its first green build. Cancelled/failed runs save nothing.

### 4.2 Emscripten SDK cache

`~/emsdk` cached as `emsdk-<os>-<EMSDK_VERSION>-<hash of toolchain/wasm_emscripten.cmake>`. `EMSDK_VERSION` is pinned (currently 5.0.7) because `latest` tracks LLVM main snapshots which have ICE'd on box3d.

---

## 5. No manifest-based incremental builds

The original plan called for skipping dependencies whose submodule commit matches the previous release's manifest. **This was never implemented and is not needed**: warm ccache/sccache makes a full build ~7–25 min per platform, all jobs run in parallel, and correctness is trivial (no stale-artifact merging logic). `ci_package.py` always repackages everything from `_out/`.

---

## 6. Windows ARM64 toolchain

`scripts/build_all.sh windows_arm64` must choose between the native ARM64 MSVC toolset (`vcvarsall arm64`) and the x64-hosted cross-compiler (`vcvarsall x64_arm64`). The emulated x64 `cl.exe` is ~2–3× slower.

Host detection uses `RUNNER_ARCH` (set by GitHub Actions), **not** `PROCESSOR_ARCHITECTURE`: git-bash may itself run x64-emulated on ARM Windows and then reports `AMD64`. If `vcvarsall arm64` fails (e.g. native toolset not installed), it falls back to `x64_arm64`.

`mtcc` is excluded on Windows ARM64 (TinyCC PE backend lacks ARM64 support), and BoringSSL builds with `OPENSSL_NO_ASM=ON` there.

---

## 7. Packaging and manifest

`scripts/ci_package.py` (run by the `release` job) creates one zip per dependency/platform from `_out/<platform>/{lib,include}` and writes `build_manifest.json`.

Artifact naming:

```
moredeps-<repo-sha8>-<platform>-<dep>-<dep-commit8>.zip
```

Important implementation details (all learned the hard way):

- **Upstream commit** is read with `git ls-tree HEAD -- deps/<dep>` on the superproject. `git rev-parse HEAD` inside an empty submodule directory walks up to the superproject and returns the *repo* commit — the release job checks out with `submodules: false`.
- **Library matching** tries each `.a`/`.lib` stem with and without the `lib` prefix (`liblibunibreak.a` vs `libunibreak.lib`), plus MSVC name variants in `DEP_LIBRARY_NAMES` (`SDL3-static`, `physfs-static`, `utf8proc_static`, `websockets_static`, `zstd_static`, `zs`/`zlibstatic`). Missing entries here previously produced header-only Windows zips that looked valid on the site.
- **Header matching** (`known_headers`) covers non-obvious layouts: `include/openssl/`, `include/freetype2/`, `skb_*.h` (skribidi), `budoux.h` (budouxc), libunibreak's `linebreak.h`/`unibreak*.h`/etc.
- **Dependency enumeration** uses the curated `DEP_LIBRARY_NAMES` list, not a raw `deps/` scan — otherwise helper dirs (`dawn_third_party`, cimgui's nested `imgui`, `lua-5.5.0`) appear as empty rows. `DIR_ALIAS` maps `lua` → `lua-5.5.0`.
- **Exclusions** (`EXCLUDED`) are recorded in the manifest with a human-readable reason and include dawn on wasm (browser provides WebGPU via emdawnwebgpu; no prebuilt lib).

Manifest format (actual):

```json
{
  "repo_commit": "<full sha>",
  "generated_at": "<ISO 8601>",
  "artifacts": {
    "<dep>": {
      "<platform>": {
        "commit": "<upstream commit>",
        "repo_url": "https://github.com/<org>/<repo>",
        "artifact_hash": "sha256:<hex>",
        "filename": "moredeps-....zip",
        "built": true
      },
      "<platform2>": { "built": false, "reason": "..." }
    }
  }
}
```

`repo_url` comes from `.gitmodules` (normalized to https). Absent for vendored deps (`lua`).

---

## 8. Releases and the download site

### Two releases per build

| Release | Type | Purpose |
|---------|------|---------|
| `build-<full-sha>` | Immutable | Permanent record for the commit |
| `latest` | Rolling alias | Deleted and recreated each run; what the site displays |

Both contain `build_manifest.json` + all zips. Old `build-<sha>` releases accumulate (no cleanup implemented yet).

### Why the manifest is served from Pages, not from the release

Fetching a release asset from the browser does not work: `github.com/.../releases/download/...` 302-redirects to `release-assets.githubusercontent.com`, and **neither response sends CORS headers**, so `fetch()` from `deps.morew4rd.com` is blocked. The API asset endpoint has the same problem (it redirects too).

Therefore:

- The `deploy-site` job (in `build.yml`, after `release`) checks out `docs/`, downloads the fresh `build_manifest.json` server-side, and redeploys GitHub Pages. The site fetches the manifest **same-origin**.
- `pages.yml` (push-triggered docs deploys) also fetches the latest manifest before deploying, so a docs push doesn't wipe it from the site.
- Both use the `pages` concurrency group to serialize deployments.

### Site features (`docs/index.html`)

- Latest view: dep × platform status matrix. Dep names link to `<repo_url>/tree/<commit>`.
- Checkboxes per artifact, per-dependency (row) and per-platform (column) select-alls, and a textarea that lists `curl -fLO <url>` for the selection (scripted multi-downloads are silently blocked by browsers; copy-paste is reliable).
- Older `build-<sha>` releases show a plain asset list (their manifests aren't same-origin).
- Direct asset links (`/releases/download/<tag>/<file>`) work in cells because navigation isn't subject to CORS — only `fetch()` is.

---

## 9. Workflow housekeeping notes

- `CMAKE_BUILD_PARALLEL_LEVEL=2` and `MOREDEPS_TOP_LEVEL_PARALLEL=1` are set globally to keep runner memory in check (dawn/boringssl are the big ones).
- Workflow-level permissions: `contents: write` (releases), `pages: write` + `id-token: write` (deploy-site), `actions: write`.
- The Node 20 deprecation annotations on `actions/*@v4` are GitHub-side; harmless (forced onto Node 24).

---

## 10. Related Files

- `.github/workflows/build.yml` — the CI workflow described here
- `.github/workflows/pages.yml` — docs deploy (sparse checkout of `docs/`, fetches latest manifest)
- `scripts/ci_package.py` — packaging + manifest generation
- `scripts/build_all.sh` — build entry point (used by CI and locally)
- `scripts/setup_vcvars.sh` — MSVC environment setup for Windows builds
- `docs/index.html` — the download site
- `docs/CNAME` — custom domain (`deps.morew4rd.com`)
