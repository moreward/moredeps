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

# Validate the toolchain file exists.
TOOLCHAIN="${REPO_ROOT}/toolchain/${PLATFORM}.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "Error: toolchain file not found: ${TOOLCHAIN}"
  exit 1
fi

# For Windows targets, ensure the MSVC environment is initialized BEFORE
# validation so that cl.exe is available when validate_dev_env.sh checks.
if [[ "${PLATFORM}" == windows_* ]]; then
  case "${PLATFORM}" in
    windows_x64)
      VCVARS_ARCH="x64"
      ;;
    windows_arm64)
      # vcvarsall.bat argument is host_target. On an x64 host we need the
      # cross-compiler (x64_arm64); on an ARM64 host we need the native
      # compiler (arm64). Detect the host architecture.
      case "${PROCESSOR_ARCHITECTURE:-}" in
        ARM64|arm64)
          VCVARS_ARCH="arm64"
          ;;
        *)
          VCVARS_ARCH="x64_arm64"
          ;;
      esac
      ;;
    *)
      echo "Error: unsupported Windows platform '${PLATFORM}'"
      exit 1
      ;;
  esac
  source "${SCRIPT_DIR}/setup_vcvars.sh" "${VCVARS_ARCH}"
fi

# Validate the host environment.
"${SCRIPT_DIR}/validate_dev_env.sh" "${PLATFORM}"

BUILD_DIR="${REPO_ROOT}/_b/${PLATFORM}"
OUT_DIR="${REPO_ROOT}/_out/${PLATFORM}"

echo "======================================================================"
echo "Building platform: ${PLATFORM}"
echo "Build directory: ${BUILD_DIR}"
echo "Output directory: ${OUT_DIR}"
echo "======================================================================"

mkdir -p "${BUILD_DIR}"

# Pick a generator that works on the host/target combination.
GENERATOR="Unix Makefiles"
if [[ "${PLATFORM}" == windows_* ]]; then
  if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
  elif command -v jom &> /dev/null; then
    GENERATOR="NMake Makefiles JOM"
  else
    GENERATOR="NMake Makefiles"
  fi
fi

# Configure the top-level super-project if it has not been configured yet.
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  cmake -S "${REPO_ROOT}" \
        -B "${BUILD_DIR}" \
        -G "${GENERATOR}" \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DCMAKE_BUILD_TYPE=Release
fi

# Build all targets. Respect MOREDEPS_TOP_LEVEL_PARALLEL to limit top-level parallelism.
BUILD_PARALLEL=""
if [[ -n "${MOREDEPS_TOP_LEVEL_PARALLEL:-}" ]]; then
  BUILD_PARALLEL="--parallel ${MOREDEPS_TOP_LEVEL_PARALLEL}"
fi
cmake --build "${BUILD_DIR}" ${BUILD_PARALLEL}

if [[ "${PLATFORM}" == "wasm_emscripten" ]]; then
  echo ""
  echo "NOTE: The wasm_emscripten artifacts are built for the Emscripten target."
  echo "      Some libraries (curl, enet, libwebsockets, tinycsocket, reproc) rely"
  echo "      on BSD sockets or process APIs that are not available inside a web"
  echo "      browser. They may be usable in non-browser WASM runtimes (WASI, Node,"
  echo "      etc.), but for browser HTTP/WebSocket/fetch use emscripten_fetch or JS"
  echo "      interop instead."
  echo ""
fi

echo "======================================================================"
echo "Build completed for ${PLATFORM}."
echo "Artifacts staged in: ${OUT_DIR}"
echo "======================================================================"
