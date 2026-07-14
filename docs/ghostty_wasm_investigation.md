# Ghostty WASM Build Investigation Notes

## Summary

Ghostty upstream has explicit WASM support, but it produces a **different artifact** than the desktop `libghostty.a` static library. The WASM build is a browser-targeted terminal emulator VT layer, not a drop-in replacement for the desktop C library.

## Evidence from upstream source

### 1. WASM target detection in build.zig

`deps/ghostty/build.zig` (line ~105):
```zig
const libghostty_vt_shared = shared: {
    if (config.target.result.cpu.arch.isWasm()) {
        break :shared try buildpkg.GhosttyLibVt.initWasm(
            b,
            &mod,
        );
    }
    break :shared try buildpkg.GhosttyLibVt.initShared(
        b,
        &mod,
    );
};
```

When the target CPU arch is WASM, ghostty builds `libghostty-vt` via `initWasm()` instead of the normal `initShared()`.

### 2. WASM-specific exports in lib_vt.zig

`deps/ghostty/src/lib_vt.zig` (lines 150-160):
```zig
@export(&alloc.allocOpaque, .{ .name = "ghostty_wasm_alloc_opaque" });
@export(&alloc.freeOpaque, .{ .name = "ghostty_wasm_free_opaque" });
@export(&alloc.allocU8Array, .{ .name = "ghostty_wasm_alloc_u8_array" });
@export(&alloc.freeU8Array, .{ .name = "ghostty_wasm_free_u8_array" });
@export(&alloc.allocU16Array, .{ .name = "ghostty_wasm_alloc_u16_array" });
@export(&alloc.freeU16Array, .{ .name = "ghostty_wasm_free_u16_array" });
@export(&alloc.allocU8, .{ .name = "ghostty_wasm_alloc_u8" });
@export(&alloc.freeU8, .{ .name = "ghostty_wasm_free_u8" });
@export(&alloc.allocUsize, .{ .name = "ghostty_wasm_alloc_usize" });
@export(&alloc.freeUsize, .{ .name = "ghostty_wasm_free_usize" });
@export(&c.wasm_alloc_sgr_attribute, .{ .name = "ghostty_wasm_alloc_sgr_attribute" });
@export(&c.wasm_free_sgr_attribute, .{ .name = "ghostty_wasm_free_sgr_attribute" });
```

These are WASM-specific exports. The desktop `libghostty.a` does NOT have these functions — it has the standard C API from `include/ghostty.h`.

### 3. build_config.zig artifact detection

`deps/ghostty/src/build_config.zig` (lines 81-89):
```zig
pub const Artifact = enum {
    exe,
    lib,
    wasm_module,

    pub fn detect() Artifact {
        if (builtin.target.cpu.arch.isWasm()) {
            assert(builtin.output_mode == .Obj);
            assert(builtin.link_mode == .Static);
            return .wasm_module;
        }
        // ...
    }
};
```

When built for WASM, ghostty identifies itself as a `wasm_module` artifact, not a `lib`.

### 4. SharedDeps WASM branch

`deps/ghostty/src/build/SharedDeps.zig` (lines 341-353):
```zig
// Wasm we do manually since it is such a different build.
if (step.rootModuleTarget().cpu.arch == .wasm32) {
    if (b.lazyDependency("zig_js", .{
        .target = target,
        .optimize = optimize,
    })) |js_dep| {
        step.root_module.addImport(
            "zig-js",
            js_dep.module("zig-js"),
        );
    }
    return static_libs;
}
```

For WASM, ghostty skips ALL the desktop dependencies (freetype, harfbuzz, fontconfig, glslang, spirv-cross, oniguruma, etc.) and only adds `zig-js`. This is a fundamentally different build.

### 5. Config.wasm_target and wasm_shared

`deps/ghostty/src/build/Config.zig`:
- `wasm_target: WasmTarget` — hardcoded to `.browser` (line 97)
- `wasm_shared: bool = true` — controls whether the WASM build is a shared module
- SIMD is disabled for WASM: `if (target.result.cpu.arch.isWasm()) break :simd false;` (line 179)

### 6. Renderer backend for WASM

`deps/ghostty/src/renderer/backend.zig` (lines 12-15):
```zig
pub fn default(target: std.Target, wasm_target: WasmTarget) Backend {
    if (target.cpu.arch == .wasm32) {
        return switch (wasm_target) {
            .browser => .opengl,
        };
    }
    // ...
}
```

On WASM, the renderer backend is forced to OpenGL (for browser WebGL).

## What the WASM build produces

Based on the source analysis, a `zig build` with `-Dtarget=wasm32-freestanding` (or similar) would produce:

1. A **WASM module** (`.wasm` file) — not a `.a` static library
2. **WASM-specific exports** (`ghostty_wasm_*` functions) — not the C API from `ghostty.h`
3. **No `include/ghostty.h`** — the WASM build uses a different interface
4. **Minimal dependencies** — only `zig-js`, no freetype/harfbuzz/etc.

## Comparison: Desktop vs WASM

| Aspect | Desktop `libghostty.a` | WASM `libghostty-vt` |
|---|---|---|
| Build command | `zig build -Dapp-runtime=none` | `zig build -Dtarget=wasm32-freestanding` |
| Output format | Static library (`.a`) | WASM module (`.wasm`) |
| Public API | C API (`ghostty.h`) | WASM exports (`ghostty_wasm_*`) |
| Font rendering | freetype + harfbuzz | Browser-provided (via WebGL/canvas) |
| Dependencies | ~15 C libraries | `zig-js` only |
| Use case | Embed terminal in desktop app | Embed terminal in web page |
| Linking | Link into executable at build time | Loaded at runtime via JS |

## Open questions / unverified claims

1. **Exact zig target triple**: Does ghostty expect `wasm32-freestanding` or `wasm32-emscripten`? The build.zig uses `.isWasm()` which checks for any wasm arch, but the actual target triple for Emscripten might need to be `wasm32-emscripten` to match our toolchain.

2. **Output path**: Where does `initWasm()` write the `.wasm` file? The desktop build uses `lib.getEmittedBin()` — for WASM this might be in a different location.

3. **JS glue**: Does ghostty produce any JS wrapper code, or is the `.wasm` loaded directly? The `zig-js` dependency suggests there might be JS bindings.

4. **Emscripten compatibility**: Ghostty's WASM build is designed for the browser (`.browser` target). Emscripten's WASM environment is slightly different from raw browser WASM. There may be incompatibilities with Emscripten's syscall emulation.

5. **Whether `libghostty` (not just `libghostty-vt`) can build for WASM**: The full `libghostty` (with all features) is NOT built for WASM — only the VT layer is. The full library depends on platform APIs not available in WASM (subprocess spawning, file I/O, etc.).

## Recommendation

The WASM build is a **different product** than the desktop `libghostty.a`. Adding it to moredeps would require:
- A separate wrapper or significant branching in the existing wrapper
- Different output handling (`.wasm` instead of `.a`)
- Different documentation (WASM API vs C API)
- Potential confusion for consumers expecting the desktop C API

If a downstream project needs ghostty in the browser, they should use ghostty's WASM build directly from upstream, not through moredeps.
