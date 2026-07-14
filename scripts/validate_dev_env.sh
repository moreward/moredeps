#!/usr/bin/env bash
# scripts/validate_dev_env.sh
# Check that the host has the tools required to build the requested platform.
#
# Usage:
#   ./scripts/validate_dev_env.sh <platform>|all

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
    "$0" "$p" || true
  done
  exit 0
fi

valid=0
for p in "${PLATFORMS[@]}"; do
  if [[ "${p}" == "${PLATFORM}" ]]; then
    valid=1
    break
  fi
done
if [[ ${valid} -eq 0 ]]; then
  echo "Error: unknown platform '${PLATFORM}'"
  exit 1
fi

errors=0
warn() { echo "  [WARN] $1"; }
fail() { echo "  [FAIL] $1"; ((errors++)) || true; }
ok() { echo "  [OK]   $1"; }

require_command() {
  if command -v "$1" &> /dev/null; then
    ok "found $1 ($($1 --version 2> /dev/null | head -1))"
  else
    fail "missing required command: $1"
    echo "         $2"
  fi
}

echo "======================================================================"
echo "Validating development environment for: ${PLATFORM}"
echo "======================================================================"

require_command cmake "Install CMake (https://cmake.org/download/). Minimum version 3.25."
require_command python3 "Install Python 3 (needed by some build scripts)."

case "${PLATFORM}" in
  macos_arm64)
    require_command cc "Install Xcode Command Line Tools: xcode-select --install"
    ;;
  linux_x64)
    require_command gcc "Install GCC or Clang (e.g. apt install build-essential)."
    echo "  [INFO] Desktop Linux also needs X11/ALSA/Vulkan dev packages for SDL3, Dawn, and sokol_audio:"
    echo "         sudo apt install libx11-dev libx11-xcb-dev libxrandr-dev libxinerama-dev libxcursor-dev"
    echo "                          libxi-dev libxext-dev libxss-dev libxtst-dev libxkbcommon-dev"
    echo "                          libasound2-dev libgl1-mesa-dev libvulkan-dev"
    ;;
  linux_arm64)
    # Native ARM64 hosts need no cross compiler; x64 hosts need the
    # aarch64-linux-gnu cross toolchain.
    if [[ "$(uname -m)" == "aarch64" ]]; then
      require_command gcc "Install GCC or Clang (e.g. apt install build-essential)."
    elif command -v aarch64-linux-gnu-gcc &> /dev/null; then
      ok "found aarch64-linux-gnu-gcc"
    else
      fail "missing cross compiler: aarch64-linux-gnu-gcc"
      echo "         On Debian/Ubuntu: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
      echo "         On Fedora:        sudo dnf install gcc-aarch64-linux-gnu-c++"
      echo "         Also ensure the target sysroot/libraries are installed (e.g. libc6-dev-arm64-cross)."
    fi
    echo "  [INFO] Linux also needs X11/ALSA/Vulkan dev packages for SDL3, Dawn, and sokol_audio:"
    echo "         sudo apt install libx11-dev libx11-xcb-dev libxrandr-dev libxinerama-dev libxcursor-dev"
    echo "                          libxi-dev libxext-dev libxss-dev libxtst-dev libxkbcommon-dev"
    echo "                          libasound2-dev libgl1-mesa-dev libvulkan-dev"
    ;;
  windows_x64|windows_arm64)
    if command -v cl &> /dev/null; then
      ok "found MSVC cl"
    else
      fail "missing MSVC cl.exe"
      echo "         Install Visual Studio 2022 Build Tools with the C++ workload:"
      echo "         https://visualstudio.microsoft.com/downloads/"
      echo "         Then run this script from a 'x64 Native Tools Command Prompt' (or arm64 variant)."
    fi
    if command -v ninja &> /dev/null; then
      ok "found Ninja"
    elif command -v jom &> /dev/null; then
      ok "found jom"
    else
      warn "no Ninja or jom found; CMake will fall back to NMake or MSBuild, which is slower"
      echo "         Install Ninja: https://ninja-build.org/"
    fi
    ;;
  wasm_emscripten)
    if command -v emcc &> /dev/null; then
      ok "found emcc ($(emcc --version 2> /dev/null | head -1))"
    else
      fail "missing emcc"
      echo "         Install the Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html"
      echo "         On macOS with Homebrew: brew install emscripten"
      echo "         Then activate it: source /path/to/emsdk/emsdk_env.sh"
    fi
    ;;
esac

echo "======================================================================"
if [[ ${errors} -eq 0 ]]; then
  echo "Environment looks good for ${PLATFORM}."
else
  echo "Environment incomplete for ${PLATFORM}. Fix the [FAIL] items above."
fi
echo "======================================================================"

exit ${errors}
