# mfs-mtcc-embedded example

Loads a C source file from a PhysFS-mounted archive and compiles/runs it
in-memory with mtcc, where the mtcc runtime files (`libtcc1.a` and builtin
headers) are also supplied inside the same archive.

## What it demonstrates

- `src/mfs/mfs.h` — thin PhysFS helpers.
- Distributing a self-contained archive with both the JIT source and the
  compiler runtime needed to compile it.
- Extracting runtime files from PhysFS to a temporary host directory so that
  mtcc's standard filesystem-based loader can find them (`tcc_set_lib_path`).
- Loading and compiling `jit/main.c` directly from PhysFS with
  `tcc_compile_string`.

## Build

Assuming `moredeps` is installed or available via `find_package`:

```bash
cd examples/mfs-mtcc-embedded
cmake -B build -S . -DMTCC_RUNTIME_DIR=/path/to/mtcc/runtime
cmake --build build
```

`MTCC_RUNTIME_DIR` must contain `libtcc1.a` and an `include/` directory with
mtcc's builtin headers. When moredeps is installed, this is typically:

```
<install-prefix>/lib/tcc
```

## Create the archive and run

```bash
cmake --build build --target mfs-mtcc-embedded-zip
./build/mfs-mtcc-embedded build/scripts.zip
```

## Archive layout

```
scripts.zip
  jit/main.c           # the C source compiled by the host
  runtime/libtcc1.a    # mtcc runtime library
  runtime/include/     # mtcc builtin headers (stddef.h, etc.)
```

At runtime, the host extracts `runtime/` to a temporary host directory and
points mtcc at it with `tcc_set_lib_path`. The `jit/main.c` source is read
from the archive and compiled in memory.
