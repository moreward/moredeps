#!/usr/bin/env bash
#
# examples/download-deps.sh
#
# Download selected moredeps packages and unzip them into a local directory.
#
# Usage:
#   ./examples/download-deps.sh --release latest --deps sokol,glfw,cglm --output ./third_party
#
# The release can be a tag name such as "latest" or "build-<sha>".

set -euo pipefail

REPO="moreward/moredeps"
RELEASE="latest"
DEPS=""
OUTPUT_DIR="./moredeps-packages"
CLEAN=0

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --release <tag>     Release tag to fetch from (default: latest)
  --deps <list>       Comma-separated dependency names
  --output <dir>      Directory to unpack into (default: ./moredeps-packages)
  --clean             Remove output dir before unpacking
  -h, --help          Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)
            RELEASE="$2"
            shift 2
            ;;
        --deps)
            DEPS="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -z "$DEPS" ]]; then
    echo "Error: --deps is required" >&2
    usage
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "Error: jq is required. Install it with your package manager." >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
if [[ "$CLEAN" -eq 1 ]]; then
    rm -rf "$OUTPUT_DIR"/*
fi

MANIFEST_URL="https://github.com/${REPO}/releases/download/${RELEASE}/moredeps.json"
MANIFEST_FILE="${OUTPUT_DIR}/moredeps.json"

echo "Downloading manifest: ${MANIFEST_URL}"
curl -fsSL "$MANIFEST_URL" -o "$MANIFEST_FILE"

echo "Selected deps: ${DEPS}"
IFS=',' read -ra DEP_LIST <<< "$DEPS"

for dep in "${DEP_LIST[@]}"; do
    dep=$(echo "$dep" | tr -d '[:space:]')
    filename=$(jq -r --arg dep "$dep" '
      .artifacts[$dep] // {}
      | to_entries[]
      | select(.value.built == true)
      | .value.filename
    ' "$MANIFEST_FILE" | head -n1)

    if [[ -z "$filename" || "$filename" == "null" ]]; then
        echo "Warning: no built artifact found for '${dep}' in ${RELEASE}; skipping" >&2
        continue
    fi

    zip_url="https://github.com/${REPO}/releases/download/${RELEASE}/${filename}"
    zip_path="${OUTPUT_DIR}/${filename}"

    echo "Downloading ${dep}: ${zip_url}"
    curl -fsSL "$zip_url" -o "$zip_path"

    echo "Unzipping ${dep} -> ${OUTPUT_DIR}"
    unzip -q -o "$zip_path" -d "$OUTPUT_DIR"
    rm -f "$zip_path"
done

echo "Done. Packages are in ${OUTPUT_DIR}/"
