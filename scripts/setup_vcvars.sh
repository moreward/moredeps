#!/usr/bin/env bash
# scripts/setup_vcvars.sh
# Initialize the MSVC environment for a given target architecture.
# Intended to be sourced from build_all.sh.
#
# Usage: source scripts/setup_vcvars.sh <arch>
#   arch: x64, arm64, x86, etc.

set -euo pipefail

VSWHERE="${VSWHERE:-C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe}"

setup_vcvars() {
  local target_arch="$1"

  if [[ ! -f "$VSWHERE" ]]; then
    echo "Error: vswhere.exe not found at $VSWHERE"
    return 1
  fi

  # Use a temporary directory for batch files and captured output.
  local tmp_dir
  tmp_dir=$(mktemp -d)
  local path_file="${tmp_dir}/vs_path.txt"
  local ver_file="${tmp_dir}/vs_ver.txt"
  local env_file="${tmp_dir}/vcvars_env.txt"
  local batch_file="${tmp_dir}/run.bat"

  local path_file_win ver_file_win env_file_win batch_file_win
  path_file_win=$(cygpath -w "$path_file")
  ver_file_win=$(cygpath -w "$ver_file")
  env_file_win=$(cygpath -w "$env_file")
  batch_file_win=$(cygpath -w "$batch_file")

  # 1) Locate the latest VS installation with the C++ tools.
  cat > "$batch_file" <<EOF
@echo off
"${VSWHERE}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "${path_file_win}"
if "%errorlevel%" neq "0" exit /b %errorlevel%
"${VSWHERE}" -latest -property installationVersion > "${ver_file_win}"
EOF
  cmd //c "${batch_file_win}"

  local vs_install_path
  vs_install_path=$(tr -d '\r' < "$path_file" | head -n 1)
  local vs_version
  vs_version=$(tr -d '\r' < "$ver_file" | head -n 1)

  if [[ -z "$vs_install_path" ]]; then
    echo "Error: no Visual Studio installation found by vswhere"
    rm -rf "$tmp_dir"
    return 1
  fi

  local vs_major
  vs_major="${vs_version%%.*}"
  local vscom_tools="VS${vs_major}0COMNTOOLS"

  local vcvarsall="${vs_install_path}\\VC\\Auxiliary\\Build\\vcvarsall.bat"
  if [[ ! -f "$vcvarsall" ]]; then
    echo "Error: vcvarsall.bat not found at $vcvarsall"
    rm -rf "$tmp_dir"
    return 1
  fi

  local tools_dir="${vs_install_path}\\Common7\\Tools\\"

  # Save the original POSIX PATH so we can keep bash utilities available
  # after applying the MSVC PATH.
  local _ORIG_PATH="$PATH"

  # 2) Run vcvarsall.bat for the target architecture and capture the environment.
  # We clear the VS variables that would otherwise cause vcvarsall to reuse an
  # already-configured (possibly wrong-architecture) environment.
  cat > "$batch_file" <<EOF
@echo off
setlocal
set "VSINSTALLDIR="
set "VCINSTALLDIR="
set "DevEnvDir="
set "VCToolsInstallDir="
set "VCToolsVersion="
set "VCToolsRedistDir="
set "__VSCMD_PREINIT_PATH="
set "__VSCMD_PREINIT_INCLUDE="
set "__VSCMD_PREINIT_LIB="
set "__VSCMD_PREINIT_LIBPATH="
set "__VSCMD_PREINIT_EXTERNAL_INCLUDE="
set "__VSCMD_PREINIT_${vscom_tools}="
set "VS170COMNTOOLS="
set "VS180COMNTOOLS="
set "${vscom_tools}=${tools_dir}"
call "${vcvarsall}" ${target_arch}
if "%errorlevel%" neq "0" exit /b %errorlevel%
set > "${env_file_win}"
EOF

  echo "Setting up MSVC environment for ${target_arch} using ${vs_install_path}"
  if ! cmd //c "${batch_file_win}"; then
    echo "Error: vcvarsall.bat failed for ${target_arch}"
    rm -rf "$tmp_dir"
    return 1
  fi

  # 3) Apply the captured environment to the current shell.
  local line name value
  while IFS= read -r line || [[ -n "$line" ]]; do
    line=$(printf '%s' "$line" | tr -d '\r')
    [[ -z "$line" ]] && continue
    name="${line%%=*}"
    value="${line#*=}"
    [[ -n "$name" ]] || continue
    # Windows may set variables like CommonProgramFiles(Arm) that are not
    # valid bash identifiers; skip those.
    if [[ ! "$name" =~ ^[a-zA-Z_][a-zA-Z0-9_]*$ ]]; then
      continue
    fi
    # Convert PATH to POSIX format so the shell can still find its utilities.
    if [[ "$name" == "PATH" ]]; then
      value="$(cygpath -p -u "$value"):${_ORIG_PATH}"
    fi
    export "$name=$value"
  done < "$env_file"

  rm -rf "$tmp_dir"
  echo "MSVC environment ready for ${target_arch}."
}

setup_vcvars "$@"
