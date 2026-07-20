# mfs-lua example

Loads and runs Lua scripts from a PhysicsFS-mounted archive, with the host
filesystem removed from the Lua global environment and replaced by
PhysFS-backed shims loaded from `scripts/shim.lua`.

## What it demonstrates

- `src/mfs/mfs.h` — a thin, zero-overhead PhysFS helper layer.
- `mfs_lua.c` — a Lua C module exposing the MFS API to Lua.
- `scripts/shim.lua` — a sandbox shim that replaces:
  - `io` with a PhysFS-backed file I/O implementation.
  - `loadfile` / `dofile` with versions that load from the PhysFS archive.
  - `os` with a safe subset (`time`, `date`, `clock`, `difftime`, `getenv`).
  - `debug` with `traceback` only.
  - `package.searchers` with a PhysFS-backed module loader.
- Removal of `loadstring`, `package.loadlib`, `package.path`, `package.cpath`,
  `io.stdin`/`io.stdout`/`io.stderr`, and unsafe `os` functions (`execute`,
  `exit`, `remove`, `rename`, `setlocale`, `tmpname`).

## Security notes

This protects the **file-loading path** only. A malicious Lua script can still:

- Consume CPU (infinite loop) or memory (huge allocations).
- Exploit bugs in the Lua runtime itself.
- Write to the PhysFS write directory if one is configured (the example sets
  one via a command-line argument, defaulting to the current directory).

For untrusted scripts, add:

- A debug hook with an instruction/time budget.
- A custom allocator with a hard limit (`lua_setallocf`).
- Optionally, run the script in a separate process.

## Build

Assuming `moredeps` is installed or available via `find_package`:

```bash
cd examples/mfs-lua
cmake -B build -S .
cmake --build build
```

## Run

```bash
zip -j -r scripts.zip scripts
./build/mfs-lua scripts.zip [write-dir]
```

The optional `write-dir` argument sets the PhysFS write directory. If
omitted, the current working directory is used. The write directory is also
mounted so the `io` shim can read back what it writes.

The example loads `scripts/shim.lua` from the archive first to set up the
sandbox, then loads and runs `main.lua` from the archive. `main.lua` uses
`require()` (which resolves through the PhysFS-backed searcher) and the
`io`/`loadfile`/`dofile` shims.

The `io.read`/`io.write` functions without a file argument are not available
because stdin/stdout are not exposed to the sandbox; `print()` still works
through the C runtime.
