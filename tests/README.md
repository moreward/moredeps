# moredeps link tests

This directory contains smoke tests that verify the built artifacts can actually
be linked into a small program. Each dependency can have a tiny C/C++ snippet in
`snippets/<dep>/`.

## Structure

```
tests/
  run_tests.py        # test runner
  snippets/
    <dep>/
      test.c          # test program for the dependency
      config.json     # optional per-dep config (skip modes, extra libs, etc.)
```

## Running locally

```bash
# Detect host platform automatically and test every dep that has a snippet.
python tests/run_tests.py

# Test a specific platform against the local _out/ tree.
python tests/run_tests.py --platform macos_arm64

# Test only a few deps.
python tests/run_tests.py --deps zlib,sqlite-amalgamation,sokol_gp

# Verbose output (CMake build logs).
python tests/run_tests.py --deps curl --verbose
```

## Snippet config.json

```json
{
  "skip_static": false,
  "skip_dynamic": false,
  "link_libs": ["sokol_gfx"],
  "extra_static_libs": ["objc"],
  "extra_dynamic_libs": [],
  "extra_libs": ["m"],
  "frameworks": ["Foundation", "Cocoa"],
  "no_run": false
}
```

- `skip_static` / `skip_dynamic`: skip that linkage mode.
- `link_libs`: library base names to link for this dep (defaults to all names in the packaging mapping).
- `extra_static_libs` / `extra_dynamic_libs` / `extra_libs`: additional system libraries to link.
- `frameworks`: macOS frameworks to link.
- `no_run`: only compile/link; do not run the executable.

## How it works

The runner uses the local `_out/<platform>/{lib,include}` tree (or `--out-dir`).
For **static** tests it links every static library in `lib/` so transitive
dependencies are resolved. For **dynamic** tests it links only the shared
libraries belonging to the dependency. On macOS/Linux it sets `BUILD_RPATH` to
the dynamic library directory so the test executable can find its `.so`/`.dylib`.

## Future work

- Add a snippet for every dependency.
- Wire the test runner into CI so each platform runs its link tests after the
  build/packaging stage.
- For Windows, set `PATH` to the packaged `bin/` directory when running dynamic
  tests.
- For WASM, compile with `emcc` and verify output exists without executing it.
