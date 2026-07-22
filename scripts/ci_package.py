#!/usr/bin/env python3
"""
scripts/ci_package.py

Packages built artifacts into per-dependency zip files and generates a
moredeps.json manifest.

Each zip contains one dependency, organized by linkage type and platform:

    <dependency>.zip
      static/
        <platform>/
          lib/        - static libraries (.a, .lib)
          include/    - public headers
      dynamic/
        <platform>/
          lib/        - shared libraries (.so, .dylib) and import .lib
          lib/import/ - Windows import libraries (kept separate from static .lib)
          bin/        - Windows runtime DLLs
          include/    - public headers
      licenses/     - upstream license files (best effort)

Usage:
    python scripts/ci_package.py --out-dir _out/ --repo-sha <sha> --manifest-out moredeps.json

The script expects _out/ to contain subdirectories like:
    _out/linux_x64/
    _out/linux_arm64/
    _out/macos_arm64/
    _out/windows_x64/
    _out/windows_arm64/
    _out/wasm_emscripten/

Each platform directory should have:
    lib/          - static libraries
    lib/import/   - Windows import libraries (for the DLL)
    bin/          - Windows runtime DLLs
    include/      - public headers
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import zipfile
from pathlib import Path
from datetime import datetime, timezone

PLATFORMS = [
    "linux_x64",
    "linux_arm64",
    "macos_arm64",
    "windows_x64",
    "windows_arm64",
    "wasm_emscripten",
]

# Dependencies whose deps/ directory name differs from the dependency name.
DIR_ALIAS = {
    "lua": "lua-5.5.0",
}

# Mapping of dependency name to the library file stems (without lib prefix and .a/.lib suffix)
# that belong to that dependency. This is derived from the actual build outputs.
DEP_LIBRARY_NAMES = {
    "box3d": ["box3d"],
    "budouxc": ["budouxc"],
    "boringssl": ["crypto", "ssl"],
    "cglm": ["cglm"],
    "cgltf": ["cgltf"],
    "cimgui": ["cimgui"],
    "cJSON": ["cjson"],
    "curl": ["curl"],
    "dawn": ["webgpu_dawn"],
    "enet": ["enet"],
    "FastNoiseLite": ["FastNoiseLite"],
    "flecs": ["flecs_static"],
    "freetype": ["freetype"],
    "glfw": ["glfw3"],
    "ggml": ["ggml", "ggml-base", "ggml-cpu", "ggml-blas"],
    "harfbuzz": ["harfbuzz", "harfbuzz-subset"],
    "libunibreak": ["libunibreak"],  # liblibunibreak.a / libunibreak.lib
    "libuv": ["uv", "uv_a"],
    "libyaml": ["yaml"],
    "libwebsockets": ["websockets", "websockets_static"],
    "lua": ["lua"],
    "luv": ["luv", "luv_a"],
    "lz4": ["lz4"],
    "md4c": ["md4c"],
    "microui": ["microui"],
    "mimalloc": ["mimalloc"],
    "miniaudio": ["miniaudio", "miniaudio_channel_combiner_node", "miniaudio_channel_separator_node",
                   "miniaudio_libvorbis", "miniaudio_ltrim_node", "miniaudio_reverb_node",
                   "miniaudio_vocoder_node"],
    "minigamepad": ["minigamepad"],
    "mtcc": ["tcc", "tcc1"],  # libtcc.a + libtcc1.a runtime support
    "nanovg": ["nanovg"],
    "pcre2": ["pcre2-8", "pcre2-8-static"],
    "physfs": ["physfs", "physfs-static"],
    "raudio": ["raudio"],
    "raylib": ["raylib"],
    "reproc": ["reproc"],
    "sdl3": ["SDL3", "SDL3-static"],
    "sdl3webgpu": ["sdl3webgpu"],
    "SheenBidi": ["SheenBidi"],
    "skribidi": ["skribidi"],
    "sokol": ["sokol_app", "sokol_app_glcore", "sokol_app_gles3", "sokol_app_metal", "sokol_app_d3d11", "sokol_app_wgpu",
               "sokol_args",
               "sokol_audio",
               "sokol_fetch",
               "sokol_gfx", "sokol_gfx_glcore", "sokol_gfx_gles3", "sokol_gfx_metal", "sokol_gfx_d3d11", "sokol_gfx_wgpu",
               "sokol_glue", "sokol_glue_glcore", "sokol_glue_gles3", "sokol_glue_metal", "sokol_glue_d3d11", "sokol_glue_wgpu",
               "sokol_log",
               "sokol_time"],
    "sokol_gp": ["sokol_gp", "sokol_gp_glcore", "sokol_gp_gles3", "sokol_gp_metal", "sokol_gp_d3d11"],
    "sqlite-amalgamation": ["sqlite3"],
    "stb": ["stb_ds", "stb_image", "stb_image_resize", "stb_image_write",
            "stb_rect_pack", "stb_truetype"],
    "tinycsocket": ["tinycsocket"],
    "tinycthread": ["tinycthread"],
    "tomlc99": ["tomlc99"],
    "tracy": ["TracyClient"],
    "ubench": ["ubench"],
    "utest": ["utest"],
    "utf8proc": ["utf8proc", "utf8proc_static"],
    "xxhash": ["xxhash"],
    "zlib": ["z", "zs", "zlibstatic"],
    "zstd": ["zstd", "zstd_static"],
}

# Dependencies that are excluded on specific platforms
EXCLUDED = {
    # Emscripten exclusions
    ("glfw", "wasm_emscripten"): "No desktop windowing on the web",
    ("dawn", "wasm_emscripten"): "Browser provides WebGPU (emdawnwebgpu; no prebuilt lib)",
    ("mtcc", "wasm_emscripten"): "Target-specific C/ASM cannot compile to WASM",
    ("libuv", "wasm_emscripten"): "No libuv Emscripten platform backend",
    ("luv", "wasm_emscripten"): "Depends on libuv (no WASM backend)",
    ("reproc", "wasm_emscripten"): "Process spawning not supported on the web",
    # Windows ARM64 exclusions
    ("mtcc", "windows_arm64"): "TinyCC PE backend lacks ARM64 support",
}

# Bundles: combined distribution zips containing multiple deps plus helpers/examples.
BUNDLES = {
    "sandbox": ["lua", "mtcc", "physfs", "mimalloc", "pcre2", "utf8proc",
                "sqlite-amalgamation", "tinycthread", "libuv", "luv",
                "libwebsockets", "boringssl",
                "curl", "cJSON", "reproc", "zlib", "zstd", "xxhash", "stb",
                "libyaml", "md4c", "tomlc99"],
}


def get_submodule_urls() -> dict[str, str]:
    """Map deps/<dir> -> remote URL from .gitmodules (normalized to https)."""
    try:
        result = subprocess.run(
            ["git", "config", "-f", ".gitmodules", "--get-regexp", r"^submodule\..*\.(path|url)$"],
            capture_output=True,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        return {}
    paths: dict[str, str] = {}
    urls: dict[str, str] = {}
    for line in result.stdout.splitlines():
        key, _, value = line.partition(" ")
        # key = submodule.<name>.path / submodule.<name>.url
        parts = key.split(".")
        if len(parts) < 3:
            continue
        name, field = parts[1], parts[-1]
        if field == "path":
            paths[name] = value
        elif field == "url":
            url = value.strip()
            if url.startswith("git@"):
                url = "https://" + url[4:].replace(":", "/", 1)
            urls[name] = url.removesuffix(".git")
    return {paths[n]: urls[n] for n in paths if n in urls}


def get_submodule_commit(dep_name: str) -> str:
    """Get the pinned commit of a submodule from the superproject's tree.

    Uses `git ls-tree` on the superproject so it works even when submodules
    are not checked out (the CI release job checks out with
    `submodules: false`). Running `git rev-parse HEAD` inside an empty
    deps/<dep> directory would walk up to the superproject and wrongly
    return the repo's own commit for every dependency.
    """
    dep_path = Path("deps") / DIR_ALIAS.get(dep_name, dep_name)
    if not dep_path.exists():
        return "unknown"
    try:
        result = subprocess.run(
            ["git", "ls-tree", "HEAD", "--", dep_path.as_posix()],
            capture_output=True,
            text=True,
            check=True,
        )
        # Format: "<mode> <type> <sha>\tdeps/<dep>"
        parts = result.stdout.split()
        if len(parts) >= 3:
            if parts[1] == "commit":
                # Gitlink: the recorded submodule commit.
                return parts[2]
            # Vendored directory (plain tree): its version is the repo commit.
            head = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True,
                text=True,
                check=True,
            )
            return head.stdout.strip()
        return "unknown"
    except subprocess.CalledProcessError:
        return "unknown"


def compute_build_hash(dep_name: str, platform: str, dep_commit: str) -> str:
    """Compute a hash that captures everything affecting this dep's build.

    Hash = SHA256(PACKAGING_VERSION + dep_commit + platform +
                  hash(CMakeLists.txt) +
                  hash(toolchain/<platform>.cmake) +
                  hash(patches/<dep>_*.patch) +
                  hash(src/<dep>/ directory tree))

    File hashes are computed from LF-normalized text so that Linux and
    Windows runners produce identical hashes for the same file content.

    If this hash matches between two builds, the compiled AND PACKAGED
    artifacts are identical. Bump PACKAGING_VERSION whenever packaging
    logic changes in a zip-contents-affecting way.
    Mirrored in scripts/cache_restore.py.
    """
    PACKAGING_VERSION = 1  # reset: hashing is now line-ending normalized
    def _file_hash(f: Path) -> str:
        h = hashlib.sha256()
        # Normalize line endings so the same file hashes identically on
        # Linux (LF) and Windows (CRLF). The manifest is generated on Linux,
        # but Windows builds consume it.
        with open(f, "r", encoding="utf-8", errors="surrogateescape") as fh:
            for chunk in iter(lambda: fh.read(65536), ""):
                h.update(chunk.replace("\r\n", "\n").encode("utf-8"))
        return h.hexdigest()

    repo_root = Path(__file__).resolve().parent.parent
    h = hashlib.sha256()
    h.update(str(PACKAGING_VERSION).encode())
    h.update(dep_commit.encode())
    h.update(platform.encode())

    # Global build context.
    for rel in ["CMakeLists.txt", f"toolchain/{platform}.cmake"]:
        f = repo_root / rel
        if f.is_file():
            h.update(_file_hash(f).encode())

    # KNOWN_HEADERS for this dep — a change in header detection means
    # different zip contents.
    h.update(repr(sorted(KNOWN_HEADERS.get(dep_name, []))).encode())

    # Dep-specific patches.
    patches_dir = repo_root / "patches"
    for pf in sorted(patches_dir.glob(f"{dep_name}_*.patch")):
        h.update(_file_hash(pf).encode())

    # Dep wrapper directory (src/<dep>/).
    wrapper_dir = repo_root / "src" / dep_name
    if wrapper_dir.is_dir():
        for f in sorted(wrapper_dir.rglob("*")):
            if f.is_file():
                h.update(_file_hash(f).encode())

    return h.hexdigest()


def load_test_results(out_dir: Path) -> dict[str, dict]:
    """Load per-platform test results written by tests/run_tests.py --json-out.

    Returns {platform: {dep_name: {static: status, dynamic: status}}}."""
    results: dict[str, dict] = {}
    for f in sorted(out_dir.glob("test_results_*.json")):
        platform = f.stem.replace("test_results_", "")
        try:
            data = json.loads(f.read_text())
            results[platform] = data.get("results", {})
        except (json.JSONDecodeError, OSError):
            results[platform] = {}
    return results


def get_test_status(dep_name: str, platform: str,
                    test_results: dict[str, dict]) -> dict[str, str]:
    """Return test status for a dep/platform, normalized for the manifest."""
    platform_results = test_results.get(platform, {})
    dep_results = platform_results.get(dep_name, {})
    status: dict[str, str] = {}
    for linkage in ("static", "dynamic"):
        value = dep_results.get(linkage)
        if value == "ok":
            status[linkage] = "passed"
        elif value == "skipped":
            status[linkage] = "skipped"
        elif value in ("build-failed", "run-failed"):
            status[linkage] = "failed"
        elif value is None or value == "":
            # No snippet or no test recorded for this dep/platform.
            status[linkage] = "not-tested"
        else:
            status[linkage] = value
    return status


def combine_test_status(dep_names: list[str], platform: str,
                        test_results: dict[str, dict]) -> dict[str, str]:
    """Combine per-dep test status into a single bundle status.

    Only deps that are actually built for this platform are considered.
    Priority: failed > not-tested > skipped > passed.
    """
    priority = {"passed": 0, "skipped": 1, "not-tested": 2, "failed": 3}
    combined: dict[str, str] = {}
    for linkage in ("static", "dynamic"):
        worst = "passed"
        worst_rank = -1
        for dep_name in dep_names:
            if EXCLUDED.get((dep_name, platform)):
                continue
            status = get_test_status(dep_name, platform, test_results)
            value = status.get(linkage, "not-tested")
            rank = priority.get(value, 2)
            if rank > worst_rank:
                worst_rank = rank
                worst = value
        combined[linkage] = worst
    return combined


def sha256_file(path: Path) -> str:
    """Compute SHA-256 hash of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return f"sha256:{h.hexdigest()}"


def classify_lib(path: Path) -> str:
    """Return 'static' or 'shared' for a library file path.

    On Windows, import libraries for DLLs are staged under lib/import/ and are
    considered shared linkage artifacts. Static .lib files live directly in lib/.
    Versioned .so / .dylib files are detected correctly.
    """
    name = path.name
    suffix = path.suffix.lower()
    if suffix == ".a":
        return "static"
    if suffix in (".so", ".dylib") or ".so." in name or suffix.lstrip(".").isdigit():
        return "shared"
    if suffix == ".dll":
        return "shared"
    if suffix == ".lib":
        # Import libs are staged under lib/import/ on Windows; everything else
        # in lib/ is a static library.
        if path.parent.name == "import" and path.parent.parent.name == "lib":
            return "shared"
        return "static"
    return "static"


def find_lib_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find all library files belonging to a dependency in a platform directory.
    """
    expected_names = DEP_LIBRARY_NAMES.get(dep_name, [dep_name])
    static_exts = {".a", ".lib"}
    shared_exts = {".so", ".dylib", ".dll"}
    files = []

    for search_dir in (platform_dir / "lib", platform_dir / "bin"):
        if not search_dir.exists():
            continue
        for f in search_dir.rglob("*"):
            if not f.is_file():
                continue
            sfx = f.suffix.lower()
            # Versioned .so files (e.g. libreproc.so.14) are shared libs.
            is_static = sfx in static_exts
            is_shared = sfx in shared_exts or (".so" in f.name and sfx not in static_exts)
            if not is_static and not is_shared:
                continue
            # Match both the raw stem and the stem without a "lib" prefix:
            # libSDL3.a vs SDL3-static.lib, liblibunibreak.a vs libunibreak.lib
            stem = f.stem
            candidates = [stem]
            if stem.startswith("lib"):
                candidates.append(stem[3:])
            # Strip version suffix from shared lib stems:
            #   libreproc.so.14 -> stem libreproc.so -> base libreproc
            #   libcurl.4.dylib  -> stem libcurl.4  -> base libcurl
            sfx = f.suffix.lower()
            if sfx.lstrip(".").isdigit():
                # .so.X file: strip .so from stem (libreproc.so -> libreproc)
                if "." in stem and stem.rsplit(".", 1)[-1] == "so":
                    base = stem.rsplit(".", 1)[0]
                    candidates.append(base)
                    if base.startswith("lib"):
                        candidates.append(base[3:])
            elif "." in stem:
                # .X.Y.Z.dylib: strip all trailing numeric parts
                base = stem
                while "." in base:
                    head, tail = base.rsplit(".", 1)
                    if tail.isdigit():
                        base = head
                    else:
                        break
                if base != stem:
                    candidates.append(base)
                    if base.startswith("lib"):
                        candidates.append(base[3:])
            if any(c in expected_names for c in candidates):
                files.append(f)

    return files


def find_license_files(dep_name: str) -> list[Path]:
    """Find all license files for a dependency (best effort)."""
    dep_dir = Path("deps") / DIR_ALIAS.get(dep_name, dep_name)
    if not dep_dir.exists():
        return []

    patterns = [
        "LICENSE", "LICENSE.*", "LICENCE", "LICENCE.*",
        "COPYING", "COPYING.*", "COPYRIGHT", "COPYRIGHT.*",
        "LICENSES/*",
    ]
    files = []
    for pat in patterns:
        for f in sorted(dep_dir.glob(pat)):
            if f.is_file() and f not in files:
                files.append(f)
    # Lua has the MIT license embedded in lua.h — extract a copy
    if dep_name == "lua" and not files:
        lua_h = dep_dir / "src" / "lua.h"
        if lua_h.exists():
            files.append(lua_h)  # we'll rename it to LICENSE in the zip
    return files


# Header names/dirs that differ from the dependency name.
# Maps dep_name -> list of search terms.  The CI packaging finds headers
# by (a) looking for include/<term>/ subdirectories, then (b) files in
# include/ whose stem contains <term>.  Case matters for subdirectories.
KNOWN_HEADERS = {
    "boringssl": ["openssl"],
    "budouxc": ["budoux"],
    "cJSON": ["cjson"],
    "curl": ["curl"],
    "dawn": ["dawn", "webgpu"],
    "enet": ["enet"],
    "flecs": ["flecs"],
    "freetype": ["freetype2"],
    "glfw": ["GLFW"],
    "ggml": ["ggml"],
    "harfbuzz": ["harfbuzz"],
    "libunibreak": ["linebreak", "unibreak", "wordbreak", "graphemebreak",
                    "eastasianwidth", "emojidef", "indicconjunctbreak"],
    "libuv": ["uv"],
    "libyaml": ["yaml"],
    "libwebsockets": ["libwebsockets", "lws_config", "lws_map"],
    "lua": ["lua", "lauxlib", "luaconf", "lualib"],
    "luv": ["luv"],
    "mtcc": ["libtcc", "tcc"],
    "pcre2": ["pcre2"],
    "sdl3": ["SDL3"],
    "sdl3webgpu": ["sdl3webgpu"],
    "skribidi": ["skb"],
    "sqlite-amalgamation": ["sqlite3"],
    "stb": ["stb"],
    "tracy": ["tracy"],
    "utf8proc": ["utf8proc"],
    "zlib": ["zlib", "zconf"],
    "zstd": ["zstd"],
}


def find_header_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find all header files belonging to a dependency in a platform directory."""
    include_dir = platform_dir / "include"
    if not include_dir.exists():
        return []

    search_terms = KNOWN_HEADERS.get(dep_name, [dep_name.lower()])

    files = []
    seen_paths = set()

    # Look for dep-specific subdirectories: include/<dep_name> and
    # include/<term> (e.g. include/openssl, include/freetype2)
    for subdir in [dep_name] + search_terms:
        dep_include_dir = include_dir / subdir
        if dep_include_dir.is_dir():
            for f in dep_include_dir.rglob("*"):
                if f.is_file() and f not in seen_paths:
                    files.append(f)
                    seen_paths.add(f)

    # Also check for headers directly in include/ matching the search terms
    for f in include_dir.iterdir():
        if not f.is_file():
            continue
        stem = f.stem.lower()
        for term in search_terms:
            if term.lower() in stem:
                if f not in seen_paths:
                    files.append(f)
                    seen_paths.add(f)
                break

    return files


def find_config_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find cmake and pkgconfig files for a dependency.

    These are needed by dependents that use find_package / pkg-config.
    We look for cmake subdirectories whose name matches the dep or a known
    alias, and for .pc files mentioning the dep.
    """
    # Known cmake package name aliases (what find_package looks for).
    _CMAKE_ALIASES = {
        "boringssl": ["OpenSSL"],
        "glfw": ["glfw3"],
        "sdl3": ["SDL3"],
    }
    cmake_names = [dep_name] + _CMAKE_ALIASES.get(dep_name, [])

    files: list[Path] = []

    # cmake configs: lib/cmake/<name>/*
    cmake_dir = platform_dir / "lib" / "cmake"
    if cmake_dir.is_dir():
        for name in cmake_names:
            pkg_dir = cmake_dir / name
            if pkg_dir.is_dir():
                files.extend(sorted(pkg_dir.rglob("*")))

    # pkgconfig: lib/pkgconfig/*.pc
    pc_dir = platform_dir / "lib" / "pkgconfig"
    if pc_dir.is_dir():
        for f in sorted(pc_dir.glob("*.pc")):
            if f.stem.startswith(dep_name) or any(
                f.stem.startswith(n) for n in cmake_names
            ):
                files.append(f)

    return files


def collect_files_for_platform(dep_name: str, platform: str, platform_dir: Path) -> list[tuple[Path, str, str]]:
    """Collect files for one dep/platform and return (disk_path, zip_arcname, kind) tuples.

    The arcname layout is:
        static/<platform>/lib/...          for static libraries
        static/<platform>/include/...       for headers when static libs exist
        dynamic/<platform>/lib/...          for shared libraries (.so/.dylib)
        dynamic/<platform>/lib/import/...   for Windows import .lib files
        dynamic/<platform>/bin/...          for Windows runtime .dll files
        dynamic/<platform>/include/...      for headers when dynamic libs exist
    """
    lib_files = find_lib_files(dep_name, platform_dir)
    header_files = find_header_files(dep_name, platform_dir)
    if not lib_files and not header_files:
        return []

    result: list[tuple[Path, str, str]] = []
    has_static = False
    has_shared = False

    lib_dir = platform_dir / "lib"
    bin_dir = platform_dir / "bin"
    for f in lib_files:
        kind = classify_lib(f)
        if kind == "static":
            has_static = True
            rel = f.relative_to(lib_dir)
            arcname = f"static/{platform}/lib/{rel}"
        else:
            has_shared = True
            if f.is_relative_to(bin_dir):
                rel = f.relative_to(bin_dir)
                arcname = f"dynamic/{platform}/bin/{rel}"
            else:
                rel = f.relative_to(lib_dir)
                arcname = f"dynamic/{platform}/lib/{rel}"
        result.append((f, arcname, kind))

    for f in header_files:
        rel = f.relative_to(platform_dir / "include")
        # Provide headers in whichever linkage folders have artifacts.
        if has_static:
            result.append((f, f"static/{platform}/include/{rel}", "header"))
        if has_shared:
            result.append((f, f"dynamic/{platform}/include/{rel}", "header"))
        # If there are no libraries (shouldn't happen for our deps), still
        # include headers under static/ for discoverability.
        if not has_static and not has_shared:
            result.append((f, f"static/{platform}/include/{rel}", "header"))

    # cmake configs and pkgconfig files — needed by dependent deps.
    for f in find_config_files(dep_name, platform_dir):
        rel = f.relative_to(platform_dir / "lib")
        if has_static:
            result.append((f, f"static/{platform}/lib/{rel}", "config"))
        if has_shared:
            result.append((f, f"dynamic/{platform}/lib/{rel}", "config"))

    return result


def package_dependency(dep_name: str, out_dir: Path, repo_sha: str,
                       submodule_urls: dict[str, str],
                       test_results: dict[str, dict]) -> dict[str, dict | None]:
    """Create a single zip for a dependency and return per-platform manifest entries."""
    dep_commit = get_submodule_commit(dep_name)
    dep_dir = f"deps/{DIR_ALIAS.get(dep_name, dep_name)}"
    repo_url = submodule_urls.get(dep_dir)

    zip_name = f"moredeps-{repo_sha[:8]}-{dep_name}-{dep_commit[:8]}.zip"
    zip_path = out_dir / zip_name

    per_platform: dict[str, dict | None] = {}
    has_any_files = False

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        seen_arcnames: set[str] = set()

        for platform in PLATFORMS:
            reason = EXCLUDED.get((dep_name, platform))
            if reason:
                per_platform[platform] = {
                    "commit": dep_commit,
                    "built": False,
                    "reason": reason,
                    "test_status": {"static": "excluded", "dynamic": "excluded"},
                }
                continue

            files = collect_files_for_platform(dep_name, platform, out_dir / platform)
            if not files:
                per_platform[platform] = None
                continue

            has_any_files = True
            platform_files: list[dict[str, str]] = []
            libraries: dict[str, list[str]] = {"static": [], "dynamic": []}
            for disk_path, arcname, kind in files:
                zf.write(disk_path, arcname)
                seen_arcnames.add(arcname)
                platform_files.append({"path": arcname, "kind": kind})
                if kind in ("static", "shared"):
                    libraries["static" if kind == "static" else "dynamic"].append(disk_path.name)

            per_platform[platform] = {
                "commit": dep_commit,
                "built": True,
                "build_hash": compute_build_hash(dep_name, platform, dep_commit),
                "files": platform_files,
                "libraries": libraries,
                "test_status": get_test_status(dep_name, platform, test_results),
                "filename": zip_name,
                **({"repo_url": repo_url} if repo_url else {}),
            }

        # Add license files (best effort). Deduplicate by arcname to avoid
        # overwriting a header/lib with the same name.
        license_files = find_license_files(dep_name)
        for f in license_files:
            if f.name == "lua.h" and dep_name == "lua":
                arcname = "licenses/LICENSE"
            elif f.name.upper().startswith(("LICEN", "COPY", "LICENS")):
                arcname = f"licenses/{dep_name}-{f.name}"
            else:
                arcname = f"licenses/{dep_name}-LICENSE.{f.suffix.lstrip('.') or 'txt'}"
            if arcname not in seen_arcnames:
                zf.write(f, arcname)
                seen_arcnames.add(arcname)

    if not has_any_files:
        # No platform produced anything for this dependency; remove the empty zip.
        zip_path.unlink(missing_ok=True)
        return {p: None for p in PLATFORMS}

    artifact_hash = sha256_file(zip_path)
    for entry in per_platform.values():
        if entry and entry.get("built") is True:
            entry["artifact_hash"] = artifact_hash

    return per_platform


def package_bundle(bundle_name: str, dep_names: list[str], out_dir: Path, repo_sha: str,
                     submodule_urls: dict[str, str],
                     test_results: dict[str, dict]) -> dict[str, dict | None]:
    """Create a single zip containing all artifacts for a set of deps.

    The bundle layout matches the per-dependency zip layout so consumers can
    use the same toolchain configuration. It also includes the thin MFS helper
    (src/mfs/mfs.h) and the related MFS examples.
    """
    zip_name = f"moredeps-{repo_sha[:8]}-bundle-{bundle_name}.zip"
    zip_path = out_dir / zip_name

    per_platform: dict[str, dict | None] = {}
    has_any_files = False

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        seen_arcnames: set[str] = set()

        for platform in PLATFORMS:
            files: list[tuple[Path, str, str]] = []
            for dep_name in dep_names:
                reason = EXCLUDED.get((dep_name, platform))
                if reason:
                    continue
                files.extend(collect_files_for_platform(dep_name, platform, out_dir / platform))

            if not files:
                per_platform[platform] = None
                continue

            has_any_files = True
            platform_files: list[dict[str, str]] = []
            libraries: dict[str, list[str]] = {"static": [], "dynamic": []}
            for disk_path, arcname, kind in files:
                if arcname in seen_arcnames:
                    continue
                zf.write(disk_path, arcname)
                seen_arcnames.add(arcname)
                platform_files.append({"path": arcname, "kind": kind})
                if kind in ("static", "shared"):
                    libraries["static" if kind == "static" else "dynamic"].append(disk_path.name)

            per_platform[platform] = {
                "built": True,
                "files": platform_files,
                "libraries": libraries,
                "test_status": combine_test_status(dep_names, platform, test_results),
                "filename": zip_name,
            }

        # Include the MFS helper so consumers can use it immediately.
        mfs_header = Path("src/mfs/mfs.h")
        if mfs_header.exists():
            arcname = "mfs/mfs.h"
            zf.write(mfs_header, arcname)
            seen_arcnames.add(arcname)

        # Include the MFS examples so the bundle is self-contained.
        examples = ["mfs-lua", "mfs-mtcc", "mfs-mtcc-embedded"]
        examples_dir = Path("examples")
        for example in examples:
            example_dir = examples_dir / example
            if not example_dir.is_dir():
                continue
            for f in sorted(example_dir.rglob("*")):
                if not f.is_file():
                    continue
                rel = f.relative_to(examples_dir)
                arcname = f"examples/{rel}"
                if arcname not in seen_arcnames:
                    zf.write(f, arcname)
                    seen_arcnames.add(arcname)

        # Add license files for all bundled deps.
        for dep_name in dep_names:
            license_files = find_license_files(dep_name)
            for f in license_files:
                if f.name == "lua.h" and dep_name == "lua":
                    arcname = "licenses/lua-LICENSE"
                elif f.name.upper().startswith(("LICEN", "COPY", "LICENS")):
                    arcname = f"licenses/{dep_name}-{f.name}"
                else:
                    arcname = f"licenses/{dep_name}-LICENSE.{f.suffix.lstrip('.') or 'txt'}"
                if arcname not in seen_arcnames:
                    zf.write(f, arcname)
                    seen_arcnames.add(arcname)

    if not has_any_files:
        zip_path.unlink(missing_ok=True)
        return {p: None for p in PLATFORMS}

    artifact_hash = sha256_file(zip_path)
    repo_url = "https://github.com/moreward/moredeps"
    for entry in per_platform.values():
        if entry and entry.get("built") is True:
            entry["artifact_hash"] = artifact_hash
            entry["contents"] = {
                "dependencies": dep_names,
                "helpers": ["mfs"],
                "examples": examples,
            }
            entry["repo_commit"] = repo_sha
            entry["repo_url"] = repo_url

    return per_platform


def generate_manifest(out_dir: Path, repo_sha: str) -> dict:
    """Generate the full manifest from built artifacts."""
    manifest = {
        "manifest_version": 2,
        "repo_commit": repo_sha,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "artifacts": {},
        "bundles": {},
    }

    # Enumerate dependencies from the curated list. Scanning deps/ directly
    # would pick up helper directories (dawn_third_party, cimgui's nested
    # imgui checkout) as empty, non-shippable rows.
    deps = sorted(DEP_LIBRARY_NAMES.keys())
    submodule_urls = get_submodule_urls()
    test_results = load_test_results(out_dir)

    for dep in deps:
        manifest["artifacts"][dep] = package_dependency(dep, out_dir, repo_sha, submodule_urls, test_results)

    for bundle_name, dep_names in BUNDLES.items():
        manifest["bundles"][bundle_name] = package_bundle(
            bundle_name, dep_names, out_dir, repo_sha, submodule_urls, test_results
        )

    return manifest


def main():
    parser = argparse.ArgumentParser(description="Package CI artifacts and generate manifest")
    parser.add_argument("--out-dir", required=True, help="Directory containing _out/<platform>/")
    parser.add_argument("--repo-sha", required=True, help="Git SHA of the repo being built")
    parser.add_argument("--manifest-out", required=True, help="Output path for manifest JSON")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    if not out_dir.exists():
        print(f"Error: output directory {out_dir} does not exist", file=sys.stderr)
        sys.exit(1)

    # Clean up old zips from previous runs
    for old_zip in out_dir.glob("moredeps-*.zip"):
        old_zip.unlink()
        print(f"Removed old zip: {old_zip}")

    print(f"Packaging artifacts from {out_dir}...")
    manifest = generate_manifest(out_dir, args.repo_sha)

    with open(args.manifest_out, "w") as f:
        json.dump(manifest, f, indent=2)

    # Count stats
    total_deps = len(manifest["artifacts"])
    built_count = 0
    excluded_count = 0
    for dep in manifest["artifacts"].values():
        for plat in dep.values():
            if plat is None:
                continue
            if plat.get("built"):
                built_count += 1
            elif plat.get("built") is False:
                excluded_count += 1

    zip_count = len(list(out_dir.glob("moredeps-*.zip")))

    print(f"\nManifest written to {args.manifest_out}")
    print(f"Dependencies: {total_deps}")
    print(f"Built artifacts: {built_count}")
    print(f"Excluded: {excluded_count}")
    print(f"Zips created: {zip_count}")


if __name__ == "__main__":
    main()
