#!/usr/bin/env python3
"""
Merge a newly generated manifest with a previous one.

When CI runs a subset of platforms (e.g. windows_x64 only), the new manifest
only contains entries for the rebuilt platforms. This script merges it with
the previous manifest so that platforms not rebuilt keep their old entries
(including the zip filename and artifact_hash pointing at the carried-forward zip).

Usage:
    python scripts/merge_manifest.py new_manifest.json old_manifest.json --out merged.json
"""

import argparse
import json
import sys


def merge_manifests(new_data: dict, old_data: dict) -> dict:
    """Merge new manifest into old, preferring new for rebuilt platforms."""
    merged = dict(old_data)

    # Update top-level fields from new manifest
    merged["repo_commit"] = new_data.get("repo_commit", merged.get("repo_commit"))
    merged["generated_at"] = new_data.get("generated_at", merged.get("generated_at"))

    # Merge artifacts: for each dep, merge per-platform entries.
    new_artifacts = new_data.get("artifacts", {})
    old_artifacts = merged.get("artifacts", {})
    for dep_name, dep_entries in new_artifacts.items():
        if dep_name not in old_artifacts:
            # New dep not in old manifest — use entirely.
            old_artifacts[dep_name] = dep_entries
            continue

        old_entries = old_artifacts[dep_name]
        if not isinstance(old_entries, dict) or not isinstance(dep_entries, dict):
            continue

        # For each platform, prefer the new entry if it has data (not null).
        for platform, new_entry in dep_entries.items():
            if new_entry is not None:
                old_entries[platform] = new_entry
            # If new is null, keep the old entry (which may have data or be None).

    # Merge bundles similarly.
    new_bundles = new_data.get("bundles", {})
    old_bundles = merged.get("bundles", {})
    for bundle_name, bundle_entries in new_bundles.items():
        if bundle_name not in old_bundles:
            old_bundles[bundle_name] = bundle_entries
            continue

        old_entries = old_bundles[bundle_name]
        if not isinstance(old_entries, dict) or not isinstance(bundle_entries, dict):
            continue

        for platform, new_entry in bundle_entries.items():
            if new_entry is not None:
                old_entries[platform] = new_entry

    return merged


def main():
    parser = argparse.ArgumentParser(description="Merge a new manifest into an old one")
    parser.add_argument("new_manifest", help="Path to the newly generated manifest")
    parser.add_argument("old_manifest", help="Path to the previous manifest (or '-' for none)")
    parser.add_argument("--out", required=True, help="Output path for merged manifest")
    args = parser.parse_args()

    with open(args.new_manifest) as f:
        new_data = json.load(f)

    if args.old_manifest == "-":
        old_data = {
            "manifest_version": 2,
            "repo_commit": "",
            "generated_at": "",
            "artifacts": {},
            "bundles": {},
        }
    else:
        with open(args.old_manifest) as f:
            old_data = json.load(f)

    merged = merge_manifests(new_data, old_data)

    with open(args.out, "w") as f:
        json.dump(merged, f, indent=2)

    print(f"Merged manifest written to {args.out}")


if __name__ == "__main__":
    main()
