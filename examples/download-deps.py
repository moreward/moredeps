#!/usr/bin/env python3
#
# examples/download-deps.py
#
# Download selected moredeps packages, verify SHA-256 hashes, and unpack them.
#
# Usage:
#   python3 examples/download-deps.py \
#       --release latest \
#       --deps sokol glfw cglm \
#       --output ./third_party

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from urllib.request import urlopen

REPO = "moreward/moredeps"


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urlopen(url) as resp, open(dest, "wb") as f:
        while True:
            chunk = resp.read(65536)
            if not chunk:
                break
            f.write(chunk)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Download and unpack moredeps packages")
    parser.add_argument("--release", default="latest", help="Release tag (default: latest)")
    parser.add_argument("--deps", nargs="+", required=True, help="Dependency names to download")
    parser.add_argument("--output", default="./moredeps-packages", help="Unpack directory")
    parser.add_argument("--verify", action="store_true", help="Verify SHA-256 hashes from manifest")
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_url = f"https://github.com/{REPO}/releases/download/{args.release}/moredeps.json"
    manifest_path = output_dir / "moredeps.json"

    print(f"Downloading manifest: {manifest_url}")
    download(manifest_url, manifest_path)
    manifest = json.loads(manifest_path.read_text())

    for dep in args.deps:
        entries = manifest.get("artifacts", {}).get(dep)
        if not entries:
            print(f"Warning: '{dep}' not found in manifest; skipping", file=sys.stderr)
            continue

        built = next((e for e in entries.values() if e and e.get("built") is True), None)
        if not built:
            print(f"Warning: '{dep}' is not built for {args.release}; skipping", file=sys.stderr)
            continue

        filename = built["filename"]
        expected_hash = built.get("artifact_hash", "")
        if expected_hash.startswith("sha256:"):
            expected_hash = expected_hash[7:]

        zip_url = f"https://github.com/{REPO}/releases/download/{args.release}/{filename}"
        zip_path = output_dir / filename

        print(f"Downloading {dep}: {zip_url}")
        download(zip_url, zip_path)

        if args.verify and expected_hash:
            actual_hash = sha256_file(zip_path)
            if actual_hash.lower() != expected_hash.lower():
                print(f"Error: SHA-256 mismatch for {dep} (expected {expected_hash}, got {actual_hash})",
                      file=sys.stderr)
                zip_path.unlink()
                return 1
            print(f"  SHA-256 OK: {actual_hash[:16]}...")

        print(f"Unzipping {dep} -> {output_dir}")
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(output_dir)
        zip_path.unlink()

    print(f"Done. Packages are in {output_dir}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
