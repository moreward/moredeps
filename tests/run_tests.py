#!/usr/bin/env python3
"""
tests/run_tests.py

Smoke-test the built artifacts by compiling a tiny program for each
dependency in both static and dynamic linkage modes.

Usage:
    python tests/run_tests.py --platform macos_arm64
    python tests/run_tests.py --platform linux_x64 --out-dir _out
    python tests/run_tests.py --platform windows_x64 --toolchain toolchain/windows_x64.cmake

The script looks for built artifacts in:
    <out-dir>/<platform>/{lib,lib/import,bin,include}

and for per-dependency test snippets in:
    tests/snippets/<dep>/test.c
    tests/snippets/<dep>/config.json   (optional)

Exit codes:
    0  all tests passed
    1  one or more tests failed
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = Path(__file__).resolve().parent
SNIPPETS = TESTS / "snippets"

# Reuse the dependency metadata from the packaging script.
sys.path.insert(0, str(ROOT / "scripts"))
from ci_package import DEP_LIBRARY_NAMES, KNOWN_HEADERS, PLATFORMS, EXCLUDED


def detect_platform() -> str:
    """Best-effort host platform detection."""
    system = platform.system().lower()
    machine = platform.machine().lower()
    mapping = {
        ("darwin", "arm64"): "macos_arm64",
        ("darwin", "aarch64"): "macos_arm64",
        ("darwin", "x86_64"): "macos_x64",
        ("linux", "x86_64"): "linux_x64",
        ("linux", "amd64"): "linux_x64",
        ("linux", "aarch64"): "linux_arm64",
        ("linux", "arm64"): "linux_arm64",
        ("windows", "x86_64"): "windows_x64",
        ("windows", "amd64"): "windows_x64",
        ("windows", "arm64"): "windows_arm64",
        ("windows", "aarch64"): "windows_arm64",
        # WASM can't be auto-detected from host; must be passed explicitly.
    }
    key = (system, machine)
    if key in mapping:
        return mapping[key]
    raise RuntimeError(f"Cannot detect platform for {system}/{machine}; pass --platform explicitly")


def is_wasm(plat: str) -> bool:
    """True if the platform string refers to an Emscripten/WASM target."""
    return plat.startswith("wasm_")


def is_win(plat: str) -> bool:
    return plat.startswith("windows_")


def is_apple(plat: str) -> bool:
    return plat.startswith("macos_")


def discover_static_libs(lib_dir: Path) -> list[Path]:
    """Return all static libraries in lib_dir, recursively."""
    libs = []
    if lib_dir.exists():
        for f in lib_dir.rglob("*"):
            if f.is_file() and f.suffix in (".a", ".lib"):
                libs.append(f)
    return libs


def discover_shared_libs(lib_dir: Path, import_dir: Path | None = None) -> list[Path]:
    """Return shared libraries (and Windows import libs) to link."""
    libs = []
    for d in (lib_dir, import_dir):
        if d and d.exists():
            for f in d.rglob("*"):
                if f.is_file() and f.suffix in (".so", ".dylib", ".lib"):
                    libs.append(f)
    return libs


def find_dep_libs(dep_name: str, lib_dir: Path, expected_names: list[str]) -> list[Path]:
    """Find the libraries in lib_dir that belong to a specific dependency."""
    names = set(expected_names)
    found = []
    if not lib_dir.exists():
        return found
    for f in lib_dir.iterdir():
        if not f.is_file():
            continue
        stem = f.stem
        candidates = {stem}
        if stem.startswith("lib"):
            candidates.add(stem[3:])
        if names & candidates:
            found.append(f)
    return found


def read_config(dep_name: str) -> dict:
    """Read optional per-dep config.json."""
    path = SNIPPETS / dep_name / "config.json"
    if path.exists():
        return json.loads(path.read_text())
    return {}


def write_cmake(build_dir: Path, dep_name: str, linkage: str, snippet: Path,
                include_dir: Path, libs: list[Path], config: dict,
                platform: str) -> None:
    """Generate a minimal CMake project for the snippet."""
    os_prefix = platform.split("_")[0]
    lang = "CXX" if snippet.suffix in (".cpp", ".cxx", ".cc") else "C"
    target = f"test_{dep_name}_{linkage}"

    # Always enable both C and CXX so the CMake-generated link line picks up
    # the C++ standard library.  Many of our static archives are C++ (Tracy,
    # Dawn, raudio, etc.) and referencing them from a pure-C executable on
    # Linux/gcc leaves __cxa_demangle / std::thread / -lm unresolved.
    lines = [
        "cmake_minimum_required(VERSION 3.25)",
        f"project({target} LANGUAGES C CXX)",
        f"add_executable({target} {snippet.name})",
    ]

    if include_dir.exists():
        lines.append(f"target_include_directories({target} PRIVATE {include_dir.as_posix()})")

    extra_inc = config.get("include_dirs", [])
    for d in extra_inc:
        lines.append(f"target_include_directories({target} PRIVATE {d})")

    for lib_dir in sorted(set(lib.parent for lib in libs)):
        lines.append(f"target_link_directories({target} PRIVATE {lib_dir.as_posix()})")

    # On macOS, Homebrew libraries live in /opt/homebrew/lib (ARM) or
    # /usr/local/lib (Intel) which CMake does not search by default.
    if sys.platform == "darwin":
        for d in ["/opt/homebrew/lib", "/usr/local/lib"]:
            lines.append(f"target_link_directories({target} PRIVATE {d})")

    # Add system frameworks on Apple platforms if requested.
    frameworks = (config.get(f"frameworks_{os_prefix}")
                  or config.get("frameworks", []))
    if sys.platform == "darwin" and frameworks:
        for fw in frameworks:
            lines.append(f"target_link_libraries({target} PRIVATE \"-framework {fw}\")")

    if lang == "C":
        lines.append(f"set_property(TARGET {target} PROPERTY C_STANDARD 11)")
    else:
        lines.append(f"set_property(TARGET {target} PROPERTY CXX_STANDARD 17)")

    # Set rpath so dynamic executables can find their .so/.dylib at runtime.
    # Use --disable-new-dtags on Linux to emit DT_RPATH (transitive) rather
    # than DT_RUNPATH (non-transitive): libskribidi.so → libharfbuzz.so needs
    # this.  Skip on Windows: MSVC link.exe doesn't understand -rpath; DLL
    # lookup is via PATH which the runner sets before execution.
    if linkage == "dynamic" and sys.platform != "win32":
        rpaths = sorted(set(lib.parent.as_posix() for lib in libs))
        for rp in rpaths:
            if sys.platform == "linux":
                lines.append(f"target_link_options({target} PRIVATE \"LINKER:--disable-new-dtags,-rpath,{rp}\")")
            else:
                lines.append(f"target_link_options({target} PRIVATE \"LINKER:-rpath,{rp}\")")

    # Force the C++ linker even for C snippets.  On Linux the C linker driver
    # does not pull -lstdc++ automatically, and many of our static archives
    # (TracyClient, raudio, Dawn) are C++.  Also add libm/libpthread/libGL/libdl
    # which are not implicitly linked on Linux but are needed by sqlite3,
    # miniaudio, sokol_gfx (GLCORE backend), etc.
    lines.append(f"set_target_properties({target} PROPERTIES LINKER_LANGUAGE CXX)")

    # Link the dep's own libraries first, then system libs after so GNU ld
    # resolves left-to-right (the .so/.a may reference GL, m, pthread).
    if libs:
        lib_paths = ' '.join(f'"{lib.as_posix()}"' for lib in libs)
        lines.append(f"target_link_libraries({target} PRIVATE {lib_paths})")

    # Extra system libs from per-dep config (e.g. freetype needs z).
    # Must come AFTER the dep's own libs so GNU ld resolves left-to-right.
    # Config keys can be platform-specific: extra_static_libs_windows, etc.
    extra = (config.get(f"extra_{linkage}_libs_{os_prefix}")
             or config.get(f"extra_{linkage}_libs")
             or config.get(f"extra_libs_{os_prefix}")
             or config.get("extra_libs", []))
    if extra:
        lines.append(f"target_link_libraries({target} PRIVATE {' '.join(extra)})")

    if sys.platform == "linux":
        lines.append(f"target_link_libraries({target} PRIVATE m GL pthread dl)")
    elif sys.platform == "win32":
        # D3D11/DXGI for sokol_gfx/sokol_gp D3D backends; opengl32 as fallback.
        lines.append(f"target_link_libraries({target} PRIVATE d3d11 dxgi opengl32)")

    lines.append(f"set_target_properties({target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY {build_dir.as_posix()})")

    (build_dir / "CMakeLists.txt").write_text("\n".join(lines) + "\n")
    shutil.copy2(snippet, build_dir / snippet.name)


def build_test(build_dir: Path, platform: str, toolchain: Path | None = None,
                verbose: bool = False) -> bool:
    """Configure and build the generated CMake project.

    On WASM targets the toolchain file is inferred from `toolchain/<platform>.cmake`
    when not explicitly provided.
    """
    cfg_args = ["cmake", "-S", str(build_dir), "-B", str(build_dir / "_b")]
    if toolchain:
        cfg_args.extend(["-DCMAKE_TOOLCHAIN_FILE", str(toolchain)])
    elif is_wasm(platform):
        default_tc = ROOT / "toolchain" / f"{platform}.cmake"
        if default_tc.exists():
            cfg_args.extend(["-DCMAKE_TOOLCHAIN_FILE", str(default_tc)])
    if sys.platform == "win32":
        if shutil.which("ninja"):
            cfg_args.extend(["-G", "Ninja"])
    cfg_args.append("-DCMAKE_BUILD_TYPE=Release")
    if not verbose:
        cfg_args.append("-Wno-dev")

    res = subprocess.run(cfg_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if verbose:
        print(res.stdout)
    if res.returncode != 0:
        print("CONFIGURE FAILED:\n" + res.stdout)
        return False

    res = subprocess.run(["cmake", "--build", str(build_dir / "_b"), "--config", "Release"],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if verbose:
        print(res.stdout)
    if res.returncode != 0:
        print("BUILD FAILED:\n" + res.stdout)
        return False

    return True


def run_executable(build_dir: Path, dep_name: str, linkage: str,
                   bin_dir: Path | None = None) -> bool:
    """Run the built executable.

    On Windows the .dll files are in `bin_dir`; they must be on PATH so the
    dynamic linker can find them at runtime.
    """
    exe = build_dir / f"test_{dep_name}_{linkage}"
    if sys.platform == "win32":
        exe = exe.with_suffix(".exe")
    if not exe.exists():
        # CMake may place it under a config subdir on Windows multi-config generators.
        exe = build_dir / "Release" / f"test_{dep_name}_{linkage}.exe"
    if not exe.exists():
        print(f"EXECUTABLE NOT FOUND: {exe}")
        return False

    env = os.environ.copy()
    if bin_dir and bin_dir.exists():
        sep = ";" if sys.platform == "win32" else ":"
        env["PATH"] = str(bin_dir) + sep + env.get("PATH", "")

    res = subprocess.run([str(exe)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
    if res.returncode != 0:
        print(f"RUN FAILED (exit {res.returncode}):\n{res.stdout}")
        return False
    return True


def test_dep(dep_name: str, platform: str, out_dir: Path, bin_dir: Path | None,
             toolchain: Path | None, verbose: bool, results: dict) -> None:
    """Run static and/or dynamic link tests for a dependency."""
    snippet = SNIPPETS / dep_name / "test.c"
    if not snippet.exists():
        snippet = SNIPPETS / dep_name / "test.cpp"
    if not snippet.exists():
        return

    config = read_config(dep_name)
    expected_names = config.get("link_libs", DEP_LIBRARY_NAMES.get(dep_name, [dep_name]))
    platform_dir = out_dir / platform

    static_dir = platform_dir / "lib"
    dynamic_dir = platform_dir / "lib"
    import_dir = platform_dir / "lib" / "import"
    include_dir = platform_dir / "include"

    # Resolve any relative include dirs against the platform dir.
    extra_inc = config.get("include_dirs", [])
    if extra_inc:
        config = dict(config)  # shallow copy so we don't mutate the original
        config["include_dirs"] = [str(platform_dir / d).replace('\\', '/') for d in extra_inc]

    static_libs = find_dep_libs(dep_name, static_dir, expected_names)
    dynamic_libs = find_dep_libs(dep_name, dynamic_dir, expected_names)
    if import_dir.exists():
        dynamic_libs += find_dep_libs(dep_name, import_dir, expected_names)

    # For dynamic, keep only shared/import libraries (not leftover static archives).
    dynamic_libs = [lib for lib in dynamic_libs if lib.suffix in (".so", ".dylib", ".lib")]

    # On Windows, static .lib files live in lib/ and import .lib in lib/import/.
    # For dynamic tests we only want the import libs.  Also skip rpath entirely
    # (MSVC link.exe doesn't understand -rpath; DLL lookup is via PATH).
    if sys.platform == "win32":
        dynamic_libs = [lib for lib in dynamic_libs if lib.parent.name == "import"]
        static_libs = [lib for lib in static_libs if lib.parent.name != "import"]

    skip_static = config.get("skip_static", not bool(static_libs))
    skip_dynamic = config.get("skip_dynamic", not bool(dynamic_libs))

    # WASM has no native dynamic linkage (emcc produces .wasm modules, not .so).
    if is_wasm(platform):
        skip_dynamic = True

    no_run = config.get("no_run", False) or is_wasm(platform)
    bin_dir_for_run = bin_dir if is_win(platform) else None

    for linkage, libs, skip in [("static", static_libs, skip_static),
                                 ("dynamic", dynamic_libs, skip_dynamic)]:
        if skip:
            results[dep_name][linkage] = "skipped"
            continue

        with tempfile.TemporaryDirectory(prefix=f"moredeps-test-{dep_name}-{linkage}-") as td:
            build_dir = Path(td)
            write_cmake(build_dir, dep_name, linkage, snippet, include_dir, libs, config, platform)
            ok = build_test(build_dir, platform, toolchain=toolchain, verbose=verbose)
            if not ok:
                results[dep_name][linkage] = "build-failed"
                continue
            if not no_run:
                ok = run_executable(build_dir, dep_name, linkage, bin_dir=bin_dir_for_run)
                if not ok:
                    results[dep_name][linkage] = "run-failed"
                    continue
            results[dep_name][linkage] = "ok"


def main():
    parser = argparse.ArgumentParser(description="Smoke-test built artifacts")
    parser.add_argument("--platform", help=f"Platform to test. One of: {', '.join(PLATFORMS)}")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "_out", help="Directory containing _out/<platform>")
    parser.add_argument("--toolchain", type=Path, help="CMake toolchain file to use")
    parser.add_argument("--deps", help="Comma-separated deps to test (default: all with snippets)")
    parser.add_argument("--verbose", action="store_true", help="Print CMake output")
    args = parser.parse_args()

    platform = args.platform or detect_platform()
    if platform not in PLATFORMS:
        print(f"Error: unsupported platform {platform}", file=sys.stderr)
        sys.exit(1)

    platform_dir = args.out_dir / platform
    if not platform_dir.exists():
        print(f"Error: output directory {platform_dir} does not exist", file=sys.stderr)
        sys.exit(1)

    deps = [d.strip() for d in args.deps.split(",")] if args.deps else sorted(DEP_LIBRARY_NAMES.keys())

    results: dict[str, dict[str, str]] = {}
    for dep in deps:
        results[dep] = {}
        test_dep(dep, platform, args.out_dir, platform_dir / "bin", args.toolchain, args.verbose, results)

    # Print summary
    print(f"\n{'='*60}")
    print(f"Test results for {platform}")
    print(f"{'='*60}")
    ok = 0
    failed = 0
    skipped = 0
    for dep, res in sorted(results.items()):
        if not res:
            continue
        for linkage, status in sorted(res.items()):
            if status == "ok":
                ok += 1
                print(f"  PASS  {dep} ({linkage})")
            elif status == "skipped":
                skipped += 1
                print(f"  SKIP  {dep} ({linkage})")
            else:
                failed += 1
                print(f"  FAIL  {dep} ({linkage}): {status}")
    print(f"{'='*60}")
    print(f"PASS: {ok}, SKIP: {skipped}, FAIL: {failed}")
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
