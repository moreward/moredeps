#!/usr/bin/env bash
# scripts/clean_all.sh
# Remove all generated build trees and staged outputs.
#
# Usage:
#   ./scripts/clean_all.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Removing build trees and staged outputs..."
rm -rf "${REPO_ROOT}/_b"
rm -rf "${REPO_ROOT}/_out"

echo "Clean complete."
