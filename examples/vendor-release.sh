#!/usr/bin/env bash
#
# examples/vendor-release.sh
#
# Download selected moredeps packages and re-publish them to a release in your
# own GitHub repository. This pins the binaries under your own account so they
# are not lost when moredeps rotates its last-3-builds window.
#
# Requires: curl, jq, and the `gh` CLI authenticated against the repo you
# want to publish to.
#
# Usage:
#   gh repo clone yourname/your-project
#   cd your-project
#   ../moredeps/examples/vendor-release.sh \
#       --release latest \
#       --deps sokol,glfw,cglm \
#       --vendor-tag moredeps-vendor \
#       --vendor-name "moredeps vendor"

set -euo pipefail

REPO="moreward/moredeps"
MOREDEPS_RELEASE="latest"
DEPS=""
VENDOR_TAG="moredeps-vendor"
VENDOR_NAME="moredeps vendor"
DRAFT=0

current_repo() {
    gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || true
}

VENDOR_REPO="$(current_repo)"

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --release <tag>        moredeps release tag to fetch (default: latest)
  --deps <list>          Comma-separated dependency names to vendor
  --vendor-tag <tag>    Tag name for the release in your repo (default: moredeps-vendor)
  --vendor-name <name>  Release title in your repo (default: "moredeps vendor")
  --vendor-repo <repo>  Owner/repo to publish to (default: current repo from gh)
  --draft                Create as draft release
  -h, --help             Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)
            MOREDEPS_RELEASE="$2"
            shift 2
            ;;
        --deps)
            DEPS="$2"
            shift 2
            ;;
        --vendor-tag)
            VENDOR_TAG="$2"
            shift 2
            ;;
        --vendor-name)
            VENDOR_NAME="$2"
            shift 2
            ;;
        --vendor-repo)
            VENDOR_REPO="$2"
            shift 2
            ;;
        --draft)
            DRAFT=1
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

if [[ -z "$VENDOR_REPO" ]]; then
    echo "Error: could not detect current repo. Pass --vendor-repo owner/repo." >&2
    exit 1
fi

if [[ -z "$DEPS" ]]; then
    echo "Error: --deps is required" >&2
    usage
    exit 1
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

MANIFEST_URL="https://github.com/${REPO}/releases/download/${MOREDEPS_RELEASE}/moredeps.json"
MANIFEST_FILE="${TMPDIR}/moredeps.json"

echo "Downloading moredeps manifest: ${MANIFEST_URL}"
curl -fsSL "$MANIFEST_URL" -o "$MANIFEST_FILE"

echo "Vendoring to ${VENDOR_REPO} release ${VENDOR_TAG}"
IFS=',' read -ra DEP_LIST <<< "$DEPS"
ASSET_FILES=()

for dep in "${DEP_LIST[@]}"; do
    dep=$(echo "$dep" | tr -d '[:space:]')
    filename=$(jq -r --arg dep "$dep" '
      .artifacts[$dep] // {}
      | to_entries[]
      | select(.value.built == true)
      | .value.filename
    ' "$MANIFEST_FILE" | head -n1)

    if [[ -z "$filename" || "$filename" == "null" ]]; then
        echo "Warning: no built artifact found for '${dep}'; skipping" >&2
        continue
    fi

    zip_url="https://github.com/${REPO}/releases/download/${MOREDEPS_RELEASE}/${filename}"
    zip_path="${TMPDIR}/${filename}"

    echo "Downloading ${dep}: ${zip_url}"
    curl -fsSL "$zip_url" -o "$zip_path"
    ASSET_FILES+=("$zip_path")
done

# Always include the manifest so dependents can discover the vendored files.
ASSET_FILES+=("$MANIFEST_FILE")

DRAFT_FLAG=""
if [[ "$DRAFT" -eq 1 ]]; then
    DRAFT_FLAG="--draft"
fi

# Delete any existing release with the same tag so we can re-use it.
gh release delete "$VENDOR_TAG" --repo "$VENDOR_REPO" --yes 2>/dev/null || true

gh release create "$VENDOR_TAG" \
    --repo "$VENDOR_REPO" \
    --title "$VENDOR_NAME" \
    --notes "Vendored moredeps packages from ${REPO}@${MOREDEPS_RELEASE}." \
    $DRAFT_FLAG \
    "${ASSET_FILES[@]}"

echo "Done. Vendored release: https://github.com/${VENDOR_REPO}/releases/tag/${VENDOR_TAG}"
