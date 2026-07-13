#!/usr/bin/env bash
# scripts/build_all.sh
# Orchestrates the moredeps build for one or all supported platforms.
#
# Usage:
#   ./scripts/build_all.sh <platform>
#   ./scripts/build_all.sh all
#
# Supported platforms:
#   macos_arm64, linux_x64, linux_arm64, windows_x64, windows_arm64, wasm_emscripten

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PLATFORMS=(
  "macos_arm64"
  "linux_x64"
  "linux_arm64"
  "windows_x64"
  "windows_arm64"
  "wasm_emscripten"
)

PLATFORM="${1:-}"
if [[ -z "${PLATFORM}" ]]; then
  echo "Usage: $0 <platform>|all"
  echo "Supported platforms: ${PLATFORMS[*]}"
  exit 1
fi

if [[ "${PLATFORM}" == "all" ]]; then
  for p in "${PLATFORMS[@]}"; do
    "$0" "$p"
  done
  exit 0
fi

# Validate platform.
valid=0
for p in "${PLATFORMS[@]}"; do
  if [[ "${p}" == "${PLATFORM}" ]]; then
    valid=1
    break
  fi
done
if [[ ${valid} -eq 0 ]]; then
  echo "Error: unknown platform '${PLATFORM}'"
  echo "Supported platforms: ${PLATFORMS[*]}"
  exit 1
fi

# Validate toolchain file exists.
TOOLCHAIN="${REPO_ROOT}/toolchain/${PLATFORM}.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "Error: toolchain file not found: ${TOOLCHAIN}"
  exit 1
fi

# Validate host capability for the requested platform.
case "${PLATFORM}" in
  macos_arm64)
    if [[ "$(uname -s)" != "Darwin" ]]; then
      echo "Error: macos_arm64 can only be built on macOS."
      exit 1
    fi
    ;;
  windows_x64|windows_arm64)
    echo "Note: Windows builds require MSVC or clang-cl in the environment."
    ;;
  linux_x64)
    if command -v gcc &>/dev/null; then
      : # ok
    else
      echo "Warning: gcc not found in PATH."
    fi
    ;;
  linux_arm64)
    if command -v aarch64-linux-gnu-gcc &>/dev/null; then
      : # ok
    else
      echo "Warning: aarch64-linux-gnu-gcc not found in PATH."
    fi
    ;;
  wasm_emscripten)
    if command -v emcc &>/dev/null; then
      : # ok
    else
      echo "Error: emcc not found in PATH."
      exit 1
    fi
    ;;
esac

BUILD_DIR="${REPO_ROOT}/_b/${PLATFORM}"
OUT_DIR="${REPO_ROOT}/_out/${PLATFORM}"

echo "======================================================================"
echo "Building platform: ${PLATFORM}"
echo "Build directory: ${BUILD_DIR}"
echo "Output directory: ${OUT_DIR}"
echo "======================================================================"

mkdir -p "${BUILD_DIR}"

# Configure the top-level super-project. We use Unix Makefiles for portability
# (ExternalProject with Makefiles avoids needing explicit BUILD_BYPRODUCTS).
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  cmake -S "${REPO_ROOT}" \
        -B "${BUILD_DIR}" \
        -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DCMAKE_BUILD_TYPE=Release
fi

# Build all targets.
cmake --build "${BUILD_DIR}" --parallel

echo "======================================================================"
echo "Build completed for ${PLATFORM}."
echo "Artifacts staged in: ${OUT_DIR}"
echo "======================================================================"
