#!/usr/bin/env python3
"""
scripts/prune_release_assets.py

Delete release assets that the (merged) manifest no longer references.

Zip names embed the repo SHA, so a rolling "latest" release accumulates one
full asset set per successful build: the carry-forward step downloads the
previous assets and the upload step re-uploads them, but nothing ever removes
the ones the merged manifest dropped. This reconciles the remote release with
the manifest it serves.

Assets are kept when they appear as a "filename" anywhere in the manifest, or
are present in --assets-dir (e.g. moredeps.json itself, and any zips we just
built/carried forward). Everything else on the release is deleted.

Usage:
    GITHUB_TOKEN=... python3 scripts/prune_release_assets.py \
        --repo owner/repo \
        --tag latest \
        --manifest release_assets/moredeps.json \
        --assets-dir release_assets/
"""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path


def log(msg: str) -> None:
    print(msg, file=sys.stderr, flush=True)


def run_with_retry(
    args: list[str],
    max_retries: int = 5,
    base_delay: float = 2.0,
) -> subprocess.CompletedProcess[str]:
    """Run a subprocess command, retrying on rate-limit errors."""
    last_err = None
    for attempt in range(max_retries):
        try:
            log(f"  {' '.join(args)}")
            return subprocess.run(args, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            last_err = e
            err_text = (e.stderr or "") + (e.stdout or "")
            if attempt == max_retries - 1:
                break
            if (
                "rate limit" in err_text.lower()
                or "secondary" in err_text.lower()
                or "exceeded" in err_text.lower()
                or "too many" in err_text.lower()
                or "429" in err_text
            ):
                delay = base_delay * (2 ** attempt)
                log(f"Rate-limit/backoff hit (attempt {attempt + 1}), retrying in {delay}s...")
                time.sleep(delay)
            else:
                break
    if last_err:
        log(f"Command failed: {' '.join(args)}")
        log(f"stdout: {last_err.stdout}")
        log(f"stderr: {last_err.stderr}")
        raise last_err
    raise RuntimeError("unreachable")


def manifest_filenames(path: Path) -> set[str]:
    """Collect every 'filename' value appearing anywhere in the manifest."""
    with open(path) as f:
        data = json.load(f)

    out: set[str] = set()

    def collect(node: object) -> None:
        if isinstance(node, dict):
            filename = node.get("filename")
            if isinstance(filename, str):
                out.add(filename)
            for value in node.values():
                collect(value)
        elif isinstance(node, list):
            for value in node:
                collect(value)

    collect(data)
    return out


def list_remote_assets(tag: str, repo: str) -> list[str]:
    result = run_with_retry(
        ["gh", "release", "view", tag, "-R", repo, "--json", "assets", "--jq", ".assets[].name"]
    )
    return [line for line in result.stdout.splitlines() if line]


def main() -> int:
    parser = argparse.ArgumentParser(description="Delete release assets not referenced by the manifest")
    parser.add_argument("--repo", required=True, help="owner/repo")
    parser.add_argument("--tag", required=True, help="release tag")
    parser.add_argument("--manifest", required=True, help="merged manifest whose filenames are kept")
    parser.add_argument(
        "--assets-dir",
        help="directory of assets about to be uploaded; its filenames are kept too",
    )
    parser.add_argument(
        "--keep",
        action="append",
        default=[],
        help="extra asset name to always keep (repeatable)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="report what would be deleted without deleting anything",
    )
    parser.add_argument(
        "--sleep",
        type=float,
        default=0.5,
        help="seconds to sleep between deletions (default 0.5)",
    )
    args = parser.parse_args()

    keep = manifest_filenames(Path(args.manifest)) | set(args.keep)
    if args.assets_dir:
        assets_dir = Path(args.assets_dir)
        if assets_dir.is_dir():
            keep |= {p.name for p in assets_dir.iterdir() if p.is_file()}

    remote = list_remote_assets(args.tag, args.repo)
    stale = [name for name in remote if name not in keep]

    if not stale:
        log(f"No stale assets on {args.tag} (remote={len(remote)}, keep={len(keep)})")
        return 0

    action = "Would delete" if args.dry_run else "Deleting"
    log(f"{action} {len(stale)} stale asset(s) from {args.tag} (keeping {len(remote) - len(stale)}):")
    for name in stale:
        log(f"  {name}")
    if args.dry_run:
        return 0

    for i, name in enumerate(stale):
        run_with_retry(["gh", "release", "delete-asset", args.tag, name, "-y", "-R", args.repo])
        if i < len(stale) - 1:
            time.sleep(args.sleep)

    log(f"Done. {args.tag} now has {len(remote) - len(stale)} asset(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
