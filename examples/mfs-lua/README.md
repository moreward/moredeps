# mfs-lua example

Loads and runs Lua scripts from a PhysFS-mounted archive, with the host
filesystem removed from the Lua global environment and replaced by
PhysFS-backed shims loaded from `scripts/shim.lua`.

The sandbox is now **configurable from the host**: the host sets
`__MFS_CONFIG` before loading `shim.lua`, enabling individual capabilities and
registering pre/post hooks for every filesystem, I/O, OS, debug, and load
operation.

## What it demonstrates

- `src/mfs/mfs.h` — a thin, zero-overhead PhysFS helper layer.
- `mfs_lua.c` — a Lua C module exposing the MFS API to Lua.
- `scripts/shim.lua` — a configurable sandbox shim that replaces:
  - `io` with a PhysFS-backed file I/O implementation.
  - `loadfile` / `dofile` with versions that load from the PhysFS archive.
  - `os` with a safe subset by default (`time`, `date`, `clock`, `difftime`, `getenv`).
  - `debug` with `traceback` only by default.
  - `package.searchers` with a PhysFS-backed module loader.
- **Host-controlled capabilities** for everything that was previously removed:
  stdin/stdout/stderr, `os.execute`, `os.exit`, `os.remove`, `os.rename`,
  `os.setlocale`, `os.tmpname`, full `debug` module, `loadstring`, bytecode
  loading, and native module loading.
- **Pre/post hooks** on every MFS operation, `io` operation, `os` operation,
  `debug` function, `loadfile`/`dofile`/`load`/`loadstring`, and `require`.

## Host configuration (`__MFS_CONFIG`)

Before `shim.lua` is loaded, the host creates the global table
`__MFS_CONFIG`.  If it is absent, every optional capability defaults to **OFF**
(the original safe sandbox).  See `main.c` for a concrete example.

```lua
__MFS_CONFIG = {
    capabilities = {
        -- io
        io_stdin    = true,
        io_stdout   = true,
        io_stderr   = true,

        -- os (dangerous; default is OFF)
        os_execute  = true,
        os_exit     = true,
        os_remove   = true,
        os_rename   = true,
        os_setlocale = true,
        os_tmpname  = true,

        -- debug
        debug       = true,

        -- load / require
        loadstring  = true,
        bytecode    = true,
        native_modules = true,
    },

    hooks = {
        -- MFS low-level
        mfs_open_read   = { pre = function(path) ... end, post = function(result, path) ... end },
        mfs_open_write  = { pre = function(path) ... end, post = function(result, path) ... end },
        mfs_open_append = { pre = function(path) ... end, post = function(result, path) ... end },
        mfs_read        = { ... },
        mfs_write       = { ... },
        mfs_close       = { ... },
        mfs_seek        = { ... },
        mfs_tell        = { ... },
        mfs_flush       = { ... },
        mfs_eof         = { ... },
        mfs_stat        = { ... },
        mfs_exists      = { ... },
        mfs_is_dir      = { ... },
        mfs_is_file     = { ... },
        mfs_list        = { ... },
        mfs_list_ex     = { ... },
        mfs_mkdir       = { ... },
        mfs_remove      = { ... },
        mfs_load        = { ... },
        mfs_load_text   = { ... },
        mfs_write_file  = { ... },
        mfs_append_file = { ... },

        -- File methods (called on the opened file object)
        file_read   = { ... },
        file_write  = { ... },
        file_close  = { ... },
        file_flush  = { ... },
        file_seek   = { ... },
        file_tell   = { ... },
        file_eof    = { ... },
        file_size   = { ... },

        -- io high-level
        io_open     = { ... },
        io_read     = { ... },   -- io.read() without a file arg
        io_write    = { ... },   -- io.write() without a file arg
        io_close    = { ... },
        io_lines    = { ... },
        io_type     = { ... },

        -- os
        os_time      = { ... },
        os_date      = { ... },
        os_clock     = { ... },
        os_difftime  = { ... },
        os_getenv    = { ... },
        os_execute   = { ... },
        os_exit      = { ... },
        os_remove    = { ... },
        os_rename    = { ... },
        os_setlocale = { ... },
        os_tmpname   = { ... },

        -- debug functions are prefixed with "debug_", e.g. debug_getinfo
        debug_traceback = { ... },
        debug_getinfo   = { ... },

        -- load / require
        loadfile    = { ... },
        dofile      = { ... },
        load        = { ... },
        loadstring  = { ... },
        require     = { ... },
        package_loadlib = { ... },
    },
}
```

### Hook semantics

- **pre**`(...)` is called before the operation. It may return a first value of
  `true` to **skip** the operation. Any additional return values become the
  result of the operation.
- **post**`(result_table, ...)` is called after the operation. It receives the
  result as a table and may return a new table to replace it. Returning `nil`
  keeps the original result.

Example hook that blocks writes to a specific path:

```lua
hooks = {
    mfs_open_write = {
        pre = function(path)
            if path == "forbidden.txt" then
                return true, nil, "write blocked by host"
            end
        end,
    },
}
```

## Security notes

This protects the **file-loading path** only. A malicious Lua script can still:

- Consume CPU (infinite loop) or memory (huge allocations).
- Exploit bugs in the Lua runtime itself.
- Write to the PhysFS write directory if one is configured (the example sets
  one via a command-line argument, defaulting to the current directory).
- Escape the sandbox if you enable dangerous capabilities such as
  `os.execute`, `os.exit`, `native_modules`, or `bytecode`.

For untrusted scripts, keep the defaults (all optional capabilities OFF) and
add:

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
`require()` (which resolves through the PhysFS-backed searcher), the
`io`/`loadfile`/`dofile` shims, and the controlled capabilities enabled by the
host in `main.c`.

## Implementation details

- `print()` is intentionally left untouched; it writes through the C runtime
  stdout.
- When `io_stdout` is enabled, `io.write()` delegates to the original host
  `io.stdout:write()`. When disabled, it returns an error.
- When `io_stdin` is enabled, `io.read()` delegates to the original host
  `io.stdin:read()`. When disabled, it returns an error.
- The `mfs` module is replaced by a hooked Lua wrapper that forwards calls to
  the C module, so the host can observe every low-level filesystem operation.
