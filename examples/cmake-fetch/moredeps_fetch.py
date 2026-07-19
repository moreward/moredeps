#!/usr/bin/env python3
"""
examples/cmake-fetch/moredeps_fetch.py

Helper used by the CMake example to download selected moredeps packages.
"""
import argparse
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--deps", nargs="+", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_url = f"https://github.com/{REPO}/releases/download/{args.release}/moredeps.json"
    manifest_path = output_dir / "moredeps.json"
    download(manifest_url, manifest_path)
    manifest = json.loads(manifest_path.read_text())

    for dep in args.deps:
        entries = manifest.get("artifacts", {}).get(dep)
        if not entries:
            print(f"Warning: '{dep}' not found in manifest; skipping", file=sys.stderr)
            continue

        entry = entries.get(args.platform)
        if not entry or entry.get("built") is not True:
            print(f"Warning: '{dep}' not built for {args.platform}; skipping", file=sys.stderr)
            continue

        filename = entry["filename"]
        zip_url = f"https://github.com/{REPO}/releases/download/{args.release}/{filename}"
        zip_path = output_dir / filename
        unpack_dir = output_dir / dep

        print(f"Downloading {dep}: {zip_url}")
        download(zip_url, zip_path)

        unpack_dir.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(unpack_dir)
        zip_path.unlink()

    return 0


if __name__ == "__main__":
    sys.exit(main())
