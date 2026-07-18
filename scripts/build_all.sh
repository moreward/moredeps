#!/usr/bin/env bash
# scripts/build_all.sh
# Orchestrates the moredeps build for one or all supported platforms.
#
# Usage:
#   ./scripts/build_all.sh <platform>
#   ./scripts/build_all.sh <platform> --shared    # also build .so/.dylib/.dll
#   ./scripts/build_all.sh all
#   ./scripts/build_all.sh all --shared
#
# Set MOREDEPS_BUILD_SHARED=1 in the environment to always build shared libs.
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
BUILD_SHARED="${MOREDEPS_BUILD_SHARED:-1}"

# Parse --shared flag from remaining args
shift 2>/dev/null || true
while [[ $# -gt 0 ]]; do
  case "$1" in
    --shared) BUILD_SHARED=1 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
  shift
done

if [[ -z "${PLATFORM}" ]]; then
  echo "Usage: $0 <platform>|all [--shared]"
  echo "Supported platforms: ${PLATFORMS[*]}"
  exit 1
fi

if [[ "${PLATFORM}" == "all" ]]; then
  SHARED_FLAG=""
  if [[ "${BUILD_SHARED}" == "1" ]]; then
    SHARED_FLAG="--shared"
  fi
  for p in "${PLATFORMS[@]}"; do
    "$0" "$p" ${SHARED_FLAG}
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
      # cross-compiler (x64_arm64); on an ARM64 host the native compiler
      # (arm64) which is ~2-3x faster than emulated x64 cl.exe.
      #
      # NOTE: PROCESSOR_ARCHITECTURE is unreliable here: git-bash may run
      # x64-emulated on ARM64 Windows and then reports AMD64. RUNNER_ARCH
      # (set by GitHub Actions) reflects the real host architecture.
      case "${RUNNER_ARCH:-${PROCESSOR_ARCHITECTURE:-}}" in
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
  if ! source "${SCRIPT_DIR}/setup_vcvars.sh" "${VCVARS_ARCH}"; then
    if [[ "${VCVARS_ARCH}" == "arm64" ]]; then
      echo "Native ARM64 MSVC toolset unavailable; falling back to x64_arm64 cross-compiler"
      source "${SCRIPT_DIR}/setup_vcvars.sh" "x64_arm64"
    else
      exit 1
    fi
  fi
fi

# Validate the host environment.
"${SCRIPT_DIR}/validate_dev_env.sh" "${PLATFORM}"

REPO_COMMIT="$(git -C "${REPO_ROOT}" rev-parse HEAD)"
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

# Restore unchanged deps from the previous GitHub Release so we don't rebuild them.
if command -v python3 &> /dev/null; then
  python3 "${SCRIPT_DIR}/cache_restore.py" \
    --platform "${PLATFORM}" \
    --out-dir "${OUT_DIR}" \
    --build-dir "${BUILD_DIR}" \
    --repo-commit "${REPO_COMMIT}" || true
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

# Second pass: shared libraries (.so/.dylib/.dll)
# Emscripten shared libraries are a different concept (wasm modules), skip.
# Uses a separate temp install prefix so ExternalProject doesn't skip
# (it would see stamps from the static build if we reused the same prefix).
if [[ "${BUILD_SHARED}" == "1" && "${PLATFORM}" != "wasm_emscripten" ]]; then
  SHARED_BUILD_DIR="${REPO_ROOT}/_b/${PLATFORM}_shared"
  SHARED_TMP="${OUT_DIR}_shared_tmp"
  echo ""
  echo "======================================================================"
  echo "Building shared libraries for: ${PLATFORM}"
  echo "======================================================================"

  rm -rf "${SHARED_BUILD_DIR}" "${SHARED_TMP}"
  mkdir -p "${SHARED_BUILD_DIR}"
  cmake -S "${REPO_ROOT}" \
        -B "${SHARED_BUILD_DIR}" \
        -G "${GENERATOR}" \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DMOREDEPS_BUILD_SHARED=ON \
        -DCMAKE_INSTALL_PREFIX="${SHARED_TMP}"

  # Restore unchanged shared deps from cache.
  # Shared libs install to SHARED_TMP, then get merged into OUT_DIR later.
  if command -v python3 &> /dev/null; then
    python3 "${SCRIPT_DIR}/cache_restore.py" \
      --platform "${PLATFORM}" \
      --out-dir "${SHARED_TMP}" \
      --build-dir "${SHARED_BUILD_DIR}" \
      --repo-commit "${REPO_COMMIT}" \
      --shared || true
  fi

  cmake --build "${SHARED_BUILD_DIR}" ${BUILD_PARALLEL}

  # Merge shared libraries into the main output.
  # Static .a/.lib already installed by the static pass.
  echo "Merging shared libraries into ${OUT_DIR}/"
  # Unix: .so/.dylib in lib/
  find "${SHARED_TMP}/lib" -name '*.so' -o -name '*.so.*' -o -name '*.dylib' 2>/dev/null | while read f; do
    mkdir -p "${OUT_DIR}/lib"
    cp -a "$f" "${OUT_DIR}/lib/"
    echo "  lib/$(basename $f)"
  done
  # Windows: .dll in bin/, import .lib in lib/import/ (kept separate from static .lib)
  find "${SHARED_TMP}/bin" -name '*.dll' 2>/dev/null | while read f; do
    mkdir -p "${OUT_DIR}/bin"
    cp -a "$f" "${OUT_DIR}/bin/"
    echo "  bin/$(basename $f)"
  done
  # Copy import .lib files under lib/import/ (for linking against the DLL)
  find "${SHARED_TMP}/lib" -name '*.lib' 2>/dev/null | while read f; do
    mkdir -p "${OUT_DIR}/lib/import"
    cp -a "$f" "${OUT_DIR}/lib/import/"
    echo "  lib/import/$(basename $f) (import)"
  done
  rm -rf "${SHARED_TMP}"
  echo "Shared libraries merged."
fi

echo "======================================================================"
echo "Build completed for ${PLATFORM}."
echo "Artifacts staged in: ${OUT_DIR}"
echo "======================================================================"
