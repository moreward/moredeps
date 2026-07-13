# Building shared libraries from moredeps

By default, `moredeps` builds every dependency as a **static library** (`BUILD_SHARED_LIBS=OFF`). This is the safest and most portable default: it avoids symbol-visibility issues, simplifies transitive dependencies, and keeps the artifact layout easy to consume.

However, there are valid reasons to produce shared libraries:

- The downstream application needs to hot-swap or dynamically load a dependency.
- Android requires native code to be packaged as `.so` files.
- The application uses a plugin system where each plugin must link its own copy of a dependency without symbol conflicts.
- Distribution size can be reduced when several executables share the same libraries.

This document explains how to produce shared libraries from `moredeps`, the trade-offs, and the current limitations.

## 1. The simple way: `BUILD_SHARED_LIBS=ON`

CMake respects the `BUILD_SHARED_LIBS` cache variable. If it is `ON` when a library target is created, `add_library(foo)` produces a shared library (`libfoo.so` / `libfoo.dylib` / `foo.dll`) instead of a static library (`libfoo.a` / `foo.lib`).

To make `moredeps` honor this, the top-level `CMakeLists.txt` would stop forcing the value and instead expose an option:

```cmake
option(MOREDEPS_BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(BUILD_SHARED_LIBS ${MOREDEPS_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)
```

A user would then run:

```sh
./scripts/build_all.sh macos_arm64  # or cmake directly
# Then reconfigure with shared libs enabled:
cmake -B _b/macos_arm64 -S . -DBUILD_SHARED_LIBS=ON -DMOREDEPS_BUILD_SHARED_LIBS=ON
```

Most CMake-based dependencies (`boringssl`, `curl`, `freetype`, `harfbuzz`, `sdl3`, `dawn`, `raylib`, etc.) already respect `BUILD_SHARED_LIBS` and will produce shared libraries.

## 2. Wrappers and shared libraries

The header-only / non-CMake wrappers in `src/<dep>/` create static libraries with `add_library(<dep> STATIC ...)`. To support shared builds, these wrappers would need to be updated to use `add_library(<dep> ${MOREDEPS_LIBRARY_TYPE} ...)` where:

```cmake
if(MOREDEPS_BUILD_SHARED_LIBS)
  set(MOREDEPS_LIBRARY_TYPE SHARED)
else()
  set(MOREDEPS_LIBRARY_TYPE STATIC)
endif()
```

This change is straightforward for wrappers that are pure C and have no global state. For C++ wrappers or wrappers that rely on static constructors, it needs careful review.

## 3. Transitive dependencies

Shared libraries must encode their own dependencies. When `BUILD_SHARED_LIBS=ON` and CMake targets are linked correctly, CMake will automatically add `INTERFACE_LINK_LIBRARIES` to the exported config files. For example, a shared `libcurl.dylib` will declare that it depends on `libssl.dylib` and `libcrypto.dylib`.

If a dependency is an external project that is not built as a CMake target in our build, we may need to manually add link dependencies via `target_link_libraries` in the wrapper or export config.

## 4. Platform-specific caveats

### Linux / macOS
- Shared libraries export all symbols by default.
- On macOS, `install_name_tool` or `RPATH` settings may be needed so the dynamic loader finds the libraries at runtime.
- `POSITION_INDEPENDENT_CODE` is already enabled by default in `moredeps`, which is required for shared libraries.

### Windows
- Shared libraries require explicit symbol export. Many libraries use `__declspec(dllexport)` / `__declspec(dllimport)` macros. If a library does not, only a few symbols may be exported, or none at all.
- For libraries without export macros, a `.def` file can be generated from the static library to re-export symbols, or the wrapper can define the necessary export macro.
- MSVC import libraries (`.lib`) and DLLs (`.dll`) are both produced; downstream apps link the `.lib` and ship the `.dll`.

### Android
- Android apps load native code via `.so` files. Building shared libraries is often required.
- The NDK toolchain must be used, and `POSITION_INDEPENDENT_CODE` must be enabled.
- Some system libraries (e.g., OpenGL ES, Vulkan) are provided by the system and should not be bundled.

## 5. Libraries that are not "share-safe"

Not every library is suitable for shared use without additional work:

| Library | Concern |
|---|---|
| `mimalloc` | Global allocator replacement; may conflict with the system allocator if loaded as a shared library. |
| `tracy` | Profiler client has global state; multiple shared copies can cause duplicate instrumentation. |
| `boringssl` / `openssl` | Global RNG, locks, and error state. Multiple shared copies can cause subtle bugs. |
| `sokol_*` | Header-only; can be shared, but the implementation is compiled into the shared library, so downstream apps must not also compile their own `SOKOL_IMPL`. |
| `mtcc` | JIT compiler with global state. |

## 6. Recommended approach today

If you need shared libraries now, the cleanest path is:

1. **Build the dependency as a static library** from `moredeps` (the default).
2. **Create a small shim shared library** in your downstream project that links the static library and re-exports its symbols:

   ```c
   // my_shim.c
   // Intentionally empty; the static library symbols are re-exported.
   ```
   ```sh
   # Linux/macOS
   gcc -shared -o libfoo_shim.so my_shim.c libfoo.a

   # Windows (with .def or export macros)
   cl /LD my_shim.c foo.lib /Fefoo_shim.dll
   ```

This keeps the production `moredeps` build simple and static, while giving you full control over how the shared library is packaged and which symbols are exported.

## 7. Future work

To support shared libraries natively in `moredeps`, we would:

1. Add `MOREDEPS_BUILD_SHARED_LIBS` as a top-level option.
2. Update all `src/<dep>/` wrappers to use the configurable library type.
3. Audit each dependency for symbol-export correctness on Windows.
4. Add CI jobs that build and link a small downstream app using the shared artifacts.
5. Document which libraries are share-safe and which should remain static.

Until then, the recommended approach is the **shim shared library** described in section 6.
