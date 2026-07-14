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
    "libunibreak": ["libunibreak"],  # installed as libunibreak.a
    "libwebsockets": ["websockets"],
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
    "physfs": ["physfs"],
    "raudio": ["raudio"],
    "raylib": ["raylib"],
    "reproc": ["reproc"],
    "sdl3": ["SDL3"],
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
    "utf8proc": ["utf8proc"],
    "xxhash": ["xxhash"],
    "zlib": ["z"],
    "zstd": ["zstd"],
}

# Dependencies that are excluded on specific platforms
EXCLUDED = {
    # Emscripten exclusions
    ("glfw", "wasm_emscripten"): "No desktop windowing on the web",
    ("mtcc", "wasm_emscripten"): "Target-specific C/ASM cannot compile to WASM",
    ("enet", "wasm_emscripten"): "UDP sockets not available in the browser",
    ("libwebsockets", "wasm_emscripten"): "BSD sockets not available in the browser",
    ("reproc", "wasm_emscripten"): "Process spawning not supported on the web",
    ("tinycsocket", "wasm_emscripten"): "No BSD sockets",
    # Windows ARM64 exclusions
    ("mtcc", "windows_arm64"): "TinyCC PE backend lacks ARM64 support",
}


def get_submodule_commit(dep_name: str) -> str:
    """Get the pinned commit of a submodule from the superproject's tree.

    Uses `git ls-tree` on the superproject so it works even when submodules
    are not checked out (the CI release job checks out with
    `submodules: false`). Running `git rev-parse HEAD` inside an empty
    deps/<dep> directory would walk up to the superproject and wrongly
    return the repo's own commit for every dependency.
    """
    dep_path = Path("deps") / dep_name
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
    """Find all library files belonging to a dependency in a platform directory."""
    lib_dir = platform_dir / "lib"
    if not lib_dir.exists():
        return []

    expected_names = DEP_LIBRARY_NAMES.get(dep_name, [dep_name])
    files = []

    for f in lib_dir.iterdir():
        if not f.is_file():
            continue
        if f.suffix not in (".a", ".lib"):
            continue
        # Strip lib prefix and suffix to get stem
        stem = f.stem
        if stem.startswith("lib"):
            stem = stem[3:]
        if stem in expected_names:
            files.append(f)

    return files


def find_header_files(dep_name: str, platform_dir: Path) -> list[Path]:
    """Find all header files belonging to a dependency in a platform directory."""
    include_dir = platform_dir / "include"
    if not include_dir.exists():
        return []

    files = []

    # Look for dep-specific subdirectory
    dep_include_dir = include_dir / dep_name
    if dep_include_dir.exists():
        for f in dep_include_dir.rglob("*"):
            if f.is_file():
                files.append(f)

    # Also check for headers directly in include/ that match dep name
    # Use a more precise matching: exact name or known header names
    known_headers = {
        "boringssl": ["openssl"],
        "cJSON": ["cjson"],
        "curl": ["curl"],
        "dawn": ["dawn", "webgpu"],
        "enet": ["enet"],
        "flecs": ["flecs"],
        "glfw": ["glfw"],
        "harfbuzz": ["harfbuzz"],
        "libwebsockets": ["libwebsockets"],
        "lua": ["lua"],
        "mtcc": ["libtcc", "tcc"],
        "sdl3": ["SDL3"],
        "sdl3webgpu": ["sdl3webgpu"],
        "sqlite-amalgamation": ["sqlite3"],
        "stb": ["stb"],
        "tracy": ["tracy"],
        "utf8proc": ["utf8proc"],
        "zlib": ["zlib"],
        "zstd": ["zstd"],
    }

    search_terms = known_headers.get(dep_name, [dep_name.lower()])
    for f in include_dir.iterdir():
        if not f.is_file():
            continue
        stem = f.stem.lower()
        for term in search_terms:
            if term in stem:
                files.append(f)
                break

    return files


def package_dependency(dep_name: str, platform: str, out_dir: Path, repo_sha: str) -> dict | None:
    """Create a zip for a dependency/platform and return manifest entry."""
    platform_dir = out_dir / platform
    dep_commit = get_submodule_commit(dep_name)

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

    # If no files found, this dep wasn't built for this platform
    if not lib_files and not header_files:
        return None

    # Create zip
    zip_name = f"moredeps-{repo_sha[:8]}-{platform}-{dep_name}-{dep_commit[:8]}.zip"
    zip_path = out_dir / zip_name

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        # Add libraries
        for f in lib_files:
            arcname = f"lib/{f.name}"
            zf.write(f, arcname)

        # Add headers
        for f in header_files:
            rel = f.relative_to(platform_dir / "include")
            arcname = f"include/{rel}"
            zf.write(f, arcname)

        # Add LICENSE if available
        license_file = Path("deps") / dep_name / "LICENSE"
        if license_file.exists():
            zf.write(license_file, "LICENSE")

    return {
        "commit": dep_commit,
        "artifact_hash": sha256_file(zip_path),
        "filename": zip_name,
        "built": True,
    }


def generate_manifest(out_dir: Path, repo_sha: str) -> dict:
    """Generate the full manifest from built artifacts."""
    manifest = {
        "repo_commit": repo_sha,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "artifacts": {},
    }

    # Find all dependencies that have submodules
    deps_dir = Path("deps")
    if deps_dir.exists():
        deps = sorted([d.name for d in deps_dir.iterdir() if d.is_dir() and not d.name.startswith(".")])
    else:
        deps = sorted(DEP_LIBRARY_NAMES.keys())

    for dep in deps:
        manifest["artifacts"][dep] = {}
        for platform in PLATFORMS:
            entry = package_dependency(dep, platform, out_dir, repo_sha)
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
