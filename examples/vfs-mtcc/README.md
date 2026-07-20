# vfs-mtcc example

Loads a C source file from a PhysicsFS-mounted archive and compiles/runs it
in memory with mtcc.

## What it demonstrates

- `src/vfs/md_vfs.h` — a thin, zero-overhead PhysFS helper layer.
- Loading C source from PhysFS instead of the host filesystem.
- `tcc_compile_string` + `TCC_OUTPUT_MEMORY` to avoid writing files.
- Whitelisting only the host symbols the JIT code is allowed to call.

## Security notes

This is **only** safe for the source-loading step. The compiled C code still
runs natively in the host process with the same privileges. If you expose
`fopen`, `socket`, `system`, or any other libc function via `tcc_add_symbol`,
the JIT code can use it freely.

To sandbox untrusted JIT code you must either:

- Expose only harmless, vetted symbols (as this example does with
  `host_print`), or
- Run the compiled function in a separate, sandboxed process.

Because the example does not add any include paths or library paths, the C
source cannot use `#include`. It must be self-contained.

## Build

Assuming `moredeps` is installed or available via `find_package`:

```bash
cd examples/vfs-mtcc
cmake -B build -S .
cmake --build build
```

## Run

```bash
zip -j -r scripts.zip scripts
./build/vfs-mtcc scripts.zip
```

The example loads `main.c` from the archive, compiles it, and calls the
`run` function.
