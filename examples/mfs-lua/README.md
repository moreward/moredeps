# mfs-lua example

Loads and runs Lua scripts from a PhysicsFS-mounted archive, with the host
filesystem removed from the Lua global environment.

## What it demonstrates

- `src/mfs/mfs.h` — a thin, zero-overhead PhysFS helper layer.
- A custom `package.searcher` that resolves `require()` against the VFS.
- Removal of `io`, `os`, `debug`, `load`, `loadfile`, `dofile`, `loadstring`,
  native module loading, and the default `package.path`/`package.cpath`.

## Security notes

This protects the **file-loading path** only. A malicious Lua script can still:

- Consume CPU (infinite loop) or memory (huge allocations).
- Exploit bugs in the Lua runtime itself.

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
./build/mfs-lua scripts.zip
```

The example loads `main.lua` from the archive, which in turn `require`s
`helper.lua` from the same archive.
