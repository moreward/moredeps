#!/usr/bin/env python3
"""
scripts/cache_restore.py

Restores previously-built artifacts from the latest GitHub release when a
dependency's inputs haven't changed.

A dependency is eligible for caching when its per-dependency *build_hash*
matches a previous release's entry.  build_hash captures:
  - the dependency's submodule commit
  - the target platform
  - the top-level CMakeLists.txt and toolchain/<platform>.cmake
  - dep-specific patches (patches/<dep>_*.patch)
  - the build wrapper in src/<dep>/ (if any)

Usage:
    # After cmake configure, before cmake --build:
    python scripts/cache_restore.py \\
        --platform macos_arm64 \\
        --out-dir _out/macos_arm64 \\
        --build-dir _b/macos_arm64 \\
        --repo-commit "$(git rev-parse HEAD)"

    # For the shared-libraries pass:
    python scripts/cache_restore.py \\
        --platform macos_arm64 \\
        --out-dir _out/macos_arm64 \\
        --build-dir _b/macos_arm64_shared \\
        --repo-commit "$(git rev-parse HEAD)" \\
        --shared

The script downloads the moredeps.json manifest from the 'latest' GitHub
release, compares commit hashes, and if a dependency matches it downloads the
corresponding zip, extracts the files into _out/<platform>/, and creates
ExternalProject stamp files so that CMake skips rebuilding.
"""

import argparse
import hashlib
import io
import json
import sys
import urllib.request
import zipfile
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Stable mapping: manifest dep name -> the external-project names that
# produce its libraries.  Every entry that appears in moredeps.json must
# have at least one mapping.  The keys match ci_package.py's DEP_LIBRARY_NAMES.
# ---------------------------------------------------------------------------
# For most dependencies the manifest dep name is also the ExternalProject
# name.  Three families bundle multiple ExternalProjects under one manifest
# entry:
#   sokol  – wrappers (sokol_app, sokol_gfx, …) + backend variants
#   stb    – stb_image, stb_image_write, …
#   miniaudio – additional effect-node libraries
#
# Rather than duplicating the CMakeLists.txt logic here we just record the
# “family” for those special cases.  When we scan the build directory for
# ExternalProject stamp dirs we fold them into their family.

# Deps whose ExternalProject names do NOT match the manifest dep name.
# Reversed: EP name -> manifest dep name.
_EP_FAMILIES: dict[str, str] = {}

# sokol wrappers that belong to the "sokol" manifest
for _name in [
    "sokol_app", "sokol_args", "sokol_audio", "sokol_fetch",
    "sokol_gfx", "sokol_glue", "sokol_log", "sokol_time",
]:
    _EP_FAMILIES[_name] = "sokol"

# sokol_gp is its own manifest entry; its backend variants are detected at
# runtime by the prefix check below.

# stb wrappers
for _name in [
    "stb_ds", "stb_image", "stb_image_write",
    "stb_image_resize", "stb_truetype", "stb_rect_pack",
]:
    _EP_FAMILIES[_name] = "stb"

# miniaudio effect nodes
for _name in [
    "miniaudio_channel_combiner_node", "miniaudio_channel_separator_node",
    "miniaudio_libvorbis", "miniaudio_ltrim_node",
    "miniaudio_reverb_node", "miniaudio_vocoder_node",
]:
    _EP_FAMILIES[_name] = "miniaudio"


def ep_to_manifest(ep_name: str) -> Optional[str]:
    """Map an ExternalProject name to its moredeps.json artifact key.

    Returns None only for names that are definitively not buildable deps
    (e.g. sokol_variants which is a build-helper, not an ExternalProject).
    """
    # Strip _shared suffix (for the shared-library pass).
    if ep_name.endswith("_shared"):
        base = ep_name[: -len("_shared")]
        return ep_to_manifest(base)
    # Internal helper targets that are not real deps.
    if ep_name in ("sokol_variants",):
        return None
    # EPs whose manifest name differs from the EP name (families).
    if ep_name in _EP_FAMILIES:
        return _EP_FAMILIES[ep_name]
    # sokol_<component>_<backend> variant.
    # sokol_gp_<backend> -> "sokol_gp", everything else -> "sokol".
    if ep_name.startswith("sokol_") and ep_name.count("_") >= 2:
        if ep_name.startswith("sokol_gp_"):
            return "sokol_gp"
        return "sokol"
    # Most deps: the EP name *is* the manifest key.
    return ep_name


def _download(url: str) -> Optional[bytes]:
    """Download *url* and return its body, or None on failure."""
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "moredeps-cache-restore/1.0")
        with urllib.request.urlopen(req, timeout=30) as resp:
            if resp.status == 200:
                return resp.read()
            print(f"  HTTP {resp.status} fetching {url}")
            return None
    except Exception as exc:
        print(f"  Download error ({url}): {exc}")
        return None


def get_latest_release_manifest(repo: str) -> Optional[dict]:
    """Fetch moredeps.json from the 'latest' GitHub release.

    Uses the direct download URL which does not require authentication
    and does not count against the API rate limit.
    """
    url = f"https://github.com/{repo}/releases/download/latest/moredeps.json"
    data = _download(url)
    if data is None:
        return None
    try:
        return json.loads(data)
    except json.JSONDecodeError:
        print("  Failed to parse moredeps.json")
        return None


def extract_platform_files(
    zip_data: bytes,
    out_dir: Path,
    platform: str,
    linkage: str,
) -> bool:
    """Extract files for *platform* and *linkage* from a zip into *out_dir*.

    The zip layout (from ci_package.py):
        static/<platform>/include/…
        static/<platform>/lib/…
        dynamic/<platform>/include/…
        dynamic/<platform>/lib/…
        dynamic/<platform>/lib/import/…  (Windows)
        dynamic/<platform>/bin/…         (Windows DLLs)
        licenses/…

    *linkage* is "static" or "dynamic".
    Returns True if any files were extracted.
    """
    prefix = f"{linkage}/{platform}/"
    count = 0
    with zipfile.ZipFile(io.BytesIO(zip_data)) as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            if not info.filename.startswith(prefix):
                continue
            rel = info.filename[len(prefix):]
            dest = out_dir / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src:
                dest.write_bytes(src.read())
            count += 1
    return count > 0


def _stamp_dir(build_dir: Path, ep_name: str) -> Path:
    """Return the ExternalProject stamp directory for *ep_name*."""
    return build_dir / f"{ep_name}-prefix" / "src" / f"{ep_name}-stamp"


def _complete_stamp(build_dir: Path, ep_name: str) -> Path:
    """Return the CMakeFiles/<ep>-complete stamp file."""
    return build_dir / "CMakeFiles" / f"{ep_name}-complete"


# The ExternalProject steps whose stamps must exist for CMake to skip the EP.
# Must match the steps that CMake's ExternalProject.cmake creates.
_STAMP_STEPS = [
    "mkdir",
    "download",
    "update",
    "patch",
    "configure",
    "build",
    "install",
    "done",
]


def create_stamps(build_dir: Path, ep_name: str) -> None:
    """Create all ExternalProject stamp and info files for *ep_name*.

    CMake's ExternalProject uses Make with stamp-file dependencies.
    Every stamp depends on an info file (e.g. <ep>-source_dirinfo.txt);
    if the info file is missing, Make considers the stamp out-of-date and
    runs the step recipe anyway.  We create everything so all steps are
    skipped.
    """
    sdir = _stamp_dir(build_dir, ep_name)
    sdir.mkdir(parents=True, exist_ok=True)

    # Step stamps (empty marker files).
    for step in _STAMP_STEPS:
        (sdir / f"{ep_name}-{step}").touch()

    # Info files that the stamp targets depend on.
    # They can be empty — Make only checks existence, not content.
    for info in [
        f"{ep_name}-source_dirinfo.txt",
        f"{ep_name}-update-info.txt",
        f"{ep_name}-patch-info.txt",
    ]:
        (sdir / info).touch()

    # cfgcmd.txt lives in <ep>-prefix/tmp/, not the stamp dir.
    tmp_dir = build_dir / f"{ep_name}-prefix" / "tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    (tmp_dir / f"{ep_name}-cfgcmd.txt").touch()

    # Top-level complete stamp.
    complete = _complete_stamp(build_dir, ep_name)
    complete.parent.mkdir(parents=True, exist_ok=True)
    complete.touch()


def discover_external_projects(build_dir: Path) -> list[str]:
    """Return sorted list of ExternalProject names found in *build_dir*.

    We look for directories matching `<name>-prefix/src/<name>-stamp`.
    """
    eps: list[str] = []
    for d in sorted(build_dir.glob("*-prefix")):
        name = d.name.removesuffix("-prefix")
        # Verify the stamp dir exists (it's created by CMake configure).
        if _stamp_dir(build_dir, name).exists():
            eps.append(name)
    return eps


def group_eps_by_manifest(ep_names: list[str]) -> dict[str, list[str]]:
    """Group ExternalProject names by their manifest dep key."""
    groups: dict[str, list[str]] = {}
    unknown: list[str] = []
    for ep in ep_names:
        mf = ep_to_manifest(ep)
        if mf is None:
            unknown.append(ep)
            continue
        groups.setdefault(mf, []).append(ep)
    if unknown:
        print(f"  Note: {len(unknown)} unknown EP(s), skipping: {', '.join(sorted(unknown))}")
    return groups


# Vendored deps whose deps/ directory name differs from the dependency name.
# Mirrored from ci_package.py.
_DIR_ALIAS = {
    "lua": "lua-5.5.0",
}


def get_dep_commit(repo_root: Path, dep_name: str) -> Optional[str]:
    """Return the submodule commit for *dep_name* from git ls-tree.

    Uses git ls-tree on the superproject so it works even when the
    deps/ directory is not checked out (the release job checks out with
    submodules: false).

    For vendored dependencies (plain directories, not gitlinks) the tree
    SHA is its repo version; we return the repo's own commit in that case.
    """
    dep_path = Path("deps") / _DIR_ALIAS.get(dep_name, dep_name)
    try:
        import subprocess
        result = subprocess.run(
            ["git", "-C", str(repo_root), "ls-tree", "HEAD", "--", str(dep_path)],
            capture_output=True,
            text=True,
            check=True,
        )
        parts = result.stdout.split()
        if len(parts) >= 3:
            if parts[1] == "commit":
                return parts[2]  # gitlink: the recorded submodule commit
            # Plain tree: its version is the repo commit.
            head = subprocess.run(
                ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
                capture_output=True,
                text=True,
                check=True,
            )
            return head.stdout.strip()
        return None
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def compute_build_hash(repo_root: Path, dep_name: str, platform: str,
                       dep_commit: str) -> str:
    """Compute a hash that captures everything affecting this dep's build.

    Hash = SHA256(dep_commit + platform +
                  hash(CMakeLists.txt) +
                  hash(toolchain/<platform>.cmake) +
                  hash(patches/<dep>_*.patch) +
                  hash(src/<dep>/ directory tree))

    If this hash matches between two builds, the compiled artifacts are
    identical.  Used as a per-dependency cache key.
    """
    def _file_hash(f: Path) -> str:
        h = hashlib.sha256()
        with open(f, "rb") as fh:
            for chunk in iter(lambda: fh.read(65536), b""):
                h.update(chunk)
        return h.hexdigest()

    h = hashlib.sha256()
    h.update(dep_commit.encode())
    h.update(platform.encode())

    # Global build context — files that affect every dep.
    for rel in ["CMakeLists.txt", f"toolchain/{platform}.cmake"]:
        f = repo_root / rel
        if f.is_file():
            h.update(_file_hash(f).encode())

    # KNOWN_HEADERS affects zip contents — a change means different artifacts.
    from ci_package import KNOWN_HEADERS
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


def download_zip_for_dep(repo: str, manifest_entry: dict) -> Optional[bytes]:
    """Download the zip referenced by a per-platform manifest entry.

    Uses the direct download URL pattern which does not require
    authentication.  The zip lives on the 'latest' release (the rolling
    alias), which is where every build publishes its artifacts.
    """
    filename = manifest_entry.get("filename")
    if not filename:
        return None
    url = f"https://github.com/{repo}/releases/download/latest/{filename}"
    return _download(url)


def restore_cache(
    repo: str,
    platform: str,
    repo_commit: str,
    out_dir: Path,
    build_dir: Path,
    linkage: str,
) -> int:
    """Main entry point.  Returns the number of deps restored from cache."""
    if not build_dir.exists():
        print("Build directory does not exist yet; nothing to restore.")
        return 0

    manifest = get_latest_release_manifest(repo)
    if manifest is None:
        print("No previous release manifest available; building from scratch.")
        return 0

    artifacts = manifest.get("artifacts", {})
    if not artifacts:
        print("Manifest has no artifacts; building from scratch.")
        return 0

    repo_root = Path(__file__).resolve().parent.parent
    ep_names = discover_external_projects(build_dir)
    print(f"Found {len(ep_names)} ExternalProject(s) in build directory.")

    grouped = group_eps_by_manifest(ep_names)
    print(f"Grouped into {len(grouped)} manifest dependency family(s).")

    restored = 0
    skipped = 0

    for mf_dep, ep_list in sorted(grouped.items()):
        plat_entry = artifacts.get(mf_dep, {}).get(platform)
        if plat_entry is None:
            print(f"  {mf_dep}: not in previous manifest")
            skipped += len(ep_list)
            continue
        if not plat_entry.get("built"):
            reason = plat_entry.get("reason", "unknown")
            print(f"  {mf_dep}: excluded in previous build ({reason})")
            skipped += len(ep_list)
            continue

        prev_dep_commit = plat_entry.get("commit", "")
        current_dep_commit = get_dep_commit(repo_root, mf_dep)
        if current_dep_commit is None:
            print(f"  {mf_dep}: cannot determine current commit")
            skipped += len(ep_list)
            continue

        # Compute the per-dep build hash and compare with the stored one.
        current_hash = compute_build_hash(repo_root, mf_dep, platform, current_dep_commit)
        stored_hash = plat_entry.get("build_hash", "")
        if stored_hash and stored_hash == current_hash:
            # Cache hit!
            print(
                f"  {mf_dep}: cache hit "
                f"(build_hash {current_hash[:12]}…, "
                f"{len(ep_list)} EP(s): {', '.join(ep_list)})"
            )
        else:
            if not stored_hash:
                reason = "no build_hash in manifest (old format)"
            else:
                reason = f"build_hash changed ({stored_hash[:12]}… -> {current_hash[:12]}…)"
            print(f"  {mf_dep}: {reason}")
            skipped += len(ep_list)
            continue

        zip_data = download_zip_for_dep(repo, plat_entry)
        if zip_data is None:
            print(f"    WARNING: failed to download zip; will rebuild")
            skipped += len(ep_list)
            continue

        ok = extract_platform_files(zip_data, out_dir, platform, linkage)
        if not ok:
            print(f"    WARNING: no files found for {linkage}/{platform}/ in zip")
            skipped += len(ep_list)
            continue

        for ep in ep_list:
            create_stamps(build_dir, ep)

        restored += len(ep_list)

    if skipped:
        print(f"\nRestored {restored} EP(s) from cache; {skipped} will be built.")
    else:
        print(f"\nRestored {restored} EP(s) from cache.")
    return restored


def main():
    parser = argparse.ArgumentParser(
        description="Restore moredeps build artifacts from a previous GitHub release"
    )
    parser.add_argument("--repo", default="moreward/moredeps",
                        help="GitHub repository (default: moreward/moredeps)")
    parser.add_argument("--platform", required=True,
                        help="Target platform (e.g. macos_arm64)")
    parser.add_argument("--out-dir", required=True,
                        help="Output directory (_out/<platform>/ )")
    parser.add_argument("--build-dir", required=True,
                        help="CMake build directory (_b/<platform>/ )")
    parser.add_argument("--repo-commit", required=True,
                        help="Current git commit of the moredeps repo")
    parser.add_argument("--shared", action="store_true",
                        help="Restore dynamic-library artifacts instead of static")
    args = parser.parse_args()

    out_dir = Path(args.out_dir).resolve()
    build_dir = Path(args.build_dir).resolve()

    linkage = "dynamic" if args.shared else "static"
    n = restore_cache(
        repo=args.repo,
        platform=args.platform,
        repo_commit=args.repo_commit,
        out_dir=out_dir,
        build_dir=build_dir,
        linkage=linkage,
    )
    if n:
        print(f"Restored {n} ExternalProject(s) from cache.")
    else:
        print("Nothing restored from cache.")


if __name__ == "__main__":
    main()
