#!/usr/bin/env python3
"""
scripts/ci_package.py

Packages built artifacts into per-dependency, per-platform zip files and
generates a build_manifest.json.

Usage:
    python scripts/ci_package.py --out-dir _out/ --repo-sha <sha> --manifest-out build_manifest.json

The script expects _out/ to contain subdirectories like:
    _out/linux_x64/
    _out/linux_arm64/
    _out/macos_arm64/
    _out/windows_x64/
    _out/windows_arm64/
    _out/wasm_emscripten/

Each platform directory should have:
    lib/     - static libraries
    include/ - public headers
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
    "fontstash": ["fontstash"],
    "freetype": ["freetype"],
    "glfw": ["glfw3"],
    "harfbuzz": ["harfbuzz", "harfbuzz-subset"],
    "libunibreak": ["libunibreak"],  # liblibunibreak.a / libunibreak.lib
    "libwebsockets": ["websockets", "websockets_static"],
    "lua": ["lua"],
    "lz4": ["lz4"],
    "microui": ["microui"],
    "mimalloc": ["mimalloc"],
    "miniaudio": ["miniaudio", "miniaudio_channel_combiner_node", "miniaudio_channel_separator_node",
                   "miniaudio_libvorbis", "miniaudio_ltrim_node", "miniaudio_reverb_node",
                   "miniaudio_vocoder_node"],
    "minigamepad": ["minigamepad"],
    "mtcc": ["tcc"],  # libtcc.a
    "nanovg": ["nanovg"],
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
    ("enet", "wasm_emscripten"): "UDP sockets not available in the browser",
    ("libwebsockets", "wasm_emscripten"): "BSD sockets not available in the browser",
    ("reproc", "wasm_emscripten"): "Process spawning not supported on the web",
    ("tinycsocket", "wasm_emscripten"): "No BSD sockets",
    # Windows ARM64 exclusions
    ("mtcc", "windows_arm64"): "TinyCC PE backend lacks ARM64 support",
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


def sha256_file(path: Path) -> str:
    """Compute SHA-256 hash of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return f"sha256:{h.hexdigest()}"


def find_lib_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find all library files belonging to a dependency in a platform directory.
    
    On Windows, .dll files are installed to bin/ (RUNTIME destination),
    while .lib import libraries stay in lib/ (ARCHIVE destination).
    On Unix, both .a and .so live in lib/.
    """
    expected_names = DEP_LIBRARY_NAMES.get(dep_name, [dep_name])
    static_exts = {".a", ".lib"}
    shared_exts = {".so", ".dylib", ".dll"}
    files = []

    for search_dir in ("lib", "bin"):
        d = platform_dir / search_dir
        if not d.exists():
            continue
        for f in d.iterdir():
            if not f.is_file():
                continue
            if f.suffix not in (static_exts | shared_exts):
                continue
            # Match both the raw stem and the stem without a "lib" prefix:
            # libSDL3.a vs SDL3-static.lib, liblibunibreak.a vs libunibreak.lib
            stem = f.stem
            candidates = [stem]
            if stem.startswith("lib"):
                candidates.append(stem[3:])
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


def find_header_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find all header files belonging to a dependency in a platform directory."""
    include_dir = platform_dir / "include"
    if not include_dir.exists():
        return []

    # Header names/dirs that differ from the dependency name.
    known_headers = {
        "boringssl": ["openssl"],
        "budouxc": ["budoux"],
        "cJSON": ["cjson"],
        "curl": ["curl"],
        "dawn": ["dawn", "webgpu"],
        "enet": ["enet"],
        "flecs": ["flecs"],
        "freetype": ["freetype2"],
        "glfw": ["glfw"],
        "harfbuzz": ["harfbuzz"],
        "libunibreak": ["linebreak", "unibreak", "wordbreak", "graphemebreak",
                        "eastasianwidth", "emojidef", "indicconjunctbreak"],
        "libwebsockets": ["libwebsockets"],
        "lua": ["lua"],
        "mtcc": ["libtcc", "tcc"],
        "sdl3": ["SDL3"],
        "sdl3webgpu": ["sdl3webgpu"],
        "skribidi": ["skb"],
        "sqlite-amalgamation": ["sqlite3"],
        "stb": ["stb"],
        "tracy": ["tracy"],
        "utf8proc": ["utf8proc"],
        "zlib": ["zlib"],
        "zstd": ["zstd"],
    }
    search_terms = known_headers.get(dep_name, [dep_name.lower()])

    files = []

    # Look for dep-specific subdirectories: include/<dep_name> and
    # include/<term> (e.g. include/openssl, include/freetype2)
    for subdir in [dep_name] + search_terms:
        dep_include_dir = include_dir / subdir
        if dep_include_dir.is_dir():
            for f in dep_include_dir.rglob("*"):
                if f.is_file():
                    files.append(f)

    # Also check for headers directly in include/ matching the search terms
    for f in include_dir.iterdir():
        if not f.is_file():
            continue
        stem = f.stem.lower()
        for term in search_terms:
            if term.lower() in stem:
                files.append(f)
                break

    return files


def package_dependency(dep_name: str, platform: str, out_dir: Path, repo_sha: str,
                       submodule_urls: dict[str, str]) -> dict | None:
    """Create a zip for a dependency/platform and return manifest entry."""
    platform_dir = out_dir / platform
    dep_commit = get_submodule_commit(dep_name)
    dep_dir = f"deps/{DIR_ALIAS.get(dep_name, dep_name)}"
    repo_url = submodule_urls.get(dep_dir)

    # Check exclusion first
    reason = EXCLUDED.get((dep_name, platform))
    if reason:
        return {
            "commit": dep_commit,
            "built": False,
            "reason": reason,
        }

    # Find files
    lib_files = find_lib_files(dep_name, platform_dir)
    header_files = find_header_files(dep_name, platform_dir)
    license_files = find_license_files(dep_name)

    # If no files found, this dep wasn't built for this platform
    if not lib_files and not header_files:
        return None

    # Create zip
    zip_name = f"moredeps-{repo_sha[:8]}-{platform}-{dep_name}-{dep_commit[:8]}.zip"
    zip_path = out_dir / zip_name

    zip_files = []  # track every file added

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        # Add libraries
        for f in lib_files:
            # .dll files live in bin/ on disk but ship in lib/ in the zip
            subdir = "bin" if f.parent.name == "bin" else "lib"
            arcname = f"{subdir}/{f.name}"
            zf.write(f, arcname)
            kind = "shared" if f.suffix in (".so", ".dylib", ".dll") else "static"
            zip_files.append({"path": arcname, "kind": kind})

        # Add headers
        for f in header_files:
            rel = f.relative_to(platform_dir / "include")
            arcname = f"include/{rel}"
            zf.write(f, arcname)
            zip_files.append({"path": arcname, "kind": "header"})

        # Add license files (best effort)
        seen_arcnames = {entry["path"] for entry in zip_files}
        for f in license_files:
            # For lua, the "license file" is lua.h — rename to LICENSE
            if f.name == "lua.h" and dep_name == "lua":
                arcname = "LICENSE"
            else:
                arcname = f.name if f.name.upper().startswith(("LICEN", "COPY")) else f"LICENSE.{f.suffix.lstrip('.') or 'txt'}"
            if arcname not in seen_arcnames:
                seen_arcnames.add(arcname)
                zf.write(f, arcname)
                zip_files.append({"path": arcname, "kind": "license"})

    return {
        "commit": dep_commit,
        "artifact_hash": sha256_file(zip_path),
        "filename": zip_name,
        "built": True,
        "files": zip_files,
        **({"repo_url": repo_url} if repo_url else {}),
    }


def generate_manifest(out_dir: Path, repo_sha: str) -> dict:
    """Generate the full manifest from built artifacts."""
    manifest = {
        "manifest_version": 1,
        "repo_commit": repo_sha,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "artifacts": {},
    }

    # Enumerate dependencies from the curated list. Scanning deps/ directly
    # would pick up helper directories (dawn_third_party, cimgui's nested
    # imgui checkout) as empty, non-shippable rows.
    deps = sorted(DEP_LIBRARY_NAMES.keys())
    submodule_urls = get_submodule_urls()

    for dep in deps:
        manifest["artifacts"][dep] = {}
        for platform in PLATFORMS:
            entry = package_dependency(dep, platform, out_dir, repo_sha, submodule_urls)
            if entry:
                manifest["artifacts"][dep][platform] = entry

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
