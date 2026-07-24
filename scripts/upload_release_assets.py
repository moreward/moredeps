#!/usr/bin/env python3
"""
scripts/upload_release_assets.py

Create or update a GitHub release and upload assets with retry/backoff to
avoid GitHub's secondary abuse-rate limits when many assets are uploaded in
quick succession.

Usage:
    GITHUB_TOKEN=... python3 scripts/upload_release_assets.py \
        --repo owner/repo \
        --tag build-<sha> \
        --target <sha> \
        --title "Build <sha>" \
        --notes "..." \
        --assets-dir release_assets/
"""
import argparse
import sys
import subprocess
import time
from pathlib import Path


def log(msg: str) -> None:
    print(msg, file=sys.stderr, flush=True)


def run_with_retry(
    args: list[str],
    check: bool = True,
    max_retries: int = 5,
    base_delay: float = 2.0,
) -> subprocess.CompletedProcess[str]:
    """Run a subprocess command, retrying on rate-limit errors."""
    last_err = None
    for attempt in range(max_retries):
        try:
            log(f"  {' '.join(args)}")
            result = subprocess.run(
                args,
                check=check,
                capture_output=True,
                text=True,
            )
            return result
        except subprocess.CalledProcessError as e:
            last_err = e
            err_text = (e.stderr or "") + (e.stdout or "")
            if attempt == max_retries - 1:
                break
            # Secondary rate limits are reported as 403 with "rate limit" or
            # "exceeded". 429 is the explicit retry-after status.
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


def release_exists(tag: str, repo: str) -> bool:
    result = subprocess.run(
        ["gh", "release", "view", tag, "-R", repo],
        check=False,
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


def ensure_release(
    tag: str,
    target: str,
    title: str,
    notes_file: str | Path,
    repo: str,
) -> None:
    """Create the release if it does not exist, otherwise edit it in place."""
    notes_file = Path(notes_file)
    if not notes_file.is_file():
        log(f"Error: notes file not found: {notes_file}")
        raise FileNotFoundError(notes_file)

    if release_exists(tag, repo):
        log(f"Editing existing release {tag}")
        run_with_retry(
            [
                "gh",
                "release",
                "edit",
                tag,
                "-R",
                repo,
                "--target",
                target,
                "--title",
                title,
                "--notes-file",
                str(notes_file),
            ]
        )
    else:
        log(f"Creating release {tag}")
        run_with_retry(
            [
                "gh",
                "release",
                "create",
                tag,
                "-R",
                repo,
                "--target",
                target,
                "--title",
                title,
                "--notes-file",
                str(notes_file),
            ]
        )


def upload_asset(tag: str, asset: Path, repo: str) -> None:
    """Upload one asset, retrying on rate limits and transient errors."""
    run_with_retry(
        [
            "gh",
            "release",
            "upload",
            "--clobber",
            tag,
            str(asset),
            "-R",
            repo,
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Upload release assets with retries")
    parser.add_argument("--repo", required=True, help="owner/repo")
    parser.add_argument("--tag", required=True, help="release tag")
    parser.add_argument("--target", required=True, help="target commitish")
    parser.add_argument("--title", required=True, help="release title")
    parser.add_argument("--notes", help="release notes/body (inline)")
    parser.add_argument("--notes-file", help="release notes/body file (takes precedence over --notes)")
    parser.add_argument("--assets-dir", required=True, help="directory containing assets")
    parser.add_argument(
        "--sleep",
        type=float,
        default=0.5,
        help="seconds to sleep between asset uploads (default 0.5)",
    )
    args = parser.parse_args()

    assets_dir = Path(args.assets_dir)
    if not assets_dir.is_dir():
        log(f"Error: assets directory does not exist: {assets_dir}")
        return 1

    assets = sorted(p for p in assets_dir.iterdir() if p.is_file())
    if not assets:
        log(f"Error: no assets found in {assets_dir}")
        return 1

    notes_file = None
    if args.notes_file:
        notes_file = Path(args.notes_file)
    elif args.notes is not None:
        notes_file = Path(f"/tmp/release-notes-{args.tag.replace('/', '-')}.md")
        notes_file.write_text(args.notes)
    else:
        log("Error: either --notes or --notes-file is required")
        return 1

    ensure_release(args.tag, args.target, args.title, notes_file, args.repo)

    log(f"Uploading {len(assets)} assets to release {args.tag} (repo: {args.repo})")
    for i, asset in enumerate(assets):
        upload_asset(args.tag, asset, args.repo)
        log(f"Uploaded {asset.name} ({i + 1}/{len(assets)})")
        if i < len(assets) - 1:
            time.sleep(args.sleep)

    log("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
