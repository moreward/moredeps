/*
 * examples/mfs-lua/main.c
 *
 * Demonstrates a sandboxed Lua state where scripts are loaded from a
 * PhysFS-mounted archive.  Direct filesystem access is removed from the Lua
 * global environment and replaced with PhysFS-backed shims loaded from
 * scripts/shim.lua.
 *
 * The host controls the sandbox by setting the global __MFS_CONFIG *before*
 * scripts/shim.lua is loaded.  Capabilities can be turned on/off and every
 * filesystem/io/os/debug/load operation can be observed with pre/post hooks.
 *
 * Usage: mfs-lua <archive-or-dir> [write-dir]
 *
 * The optional write-dir argument sets the PhysFS write directory. If omitted,
 * the current working directory is used.  The write directory is also mounted
 * so the io shim can read back what it writes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <physfs.h>
#include "mfs.h"

extern int luaopen_mfs(lua_State *L);

/* Load a Lua chunk from the PhysFS archive and run it. Returns 0 on success. */
static int run_lua_file(lua_State *L, const char *path)
{
    size_t len;
    char *buf = mfs_load_text(path, &len);
    if (!buf) {
        fprintf(stderr, "could not load %s: %s\n", path, mfs_last_error());
        return 1;
    }

    int status = luaL_loadbuffer(L, buf, len, path) == LUA_OK
               ? lua_pcall(L, 0, 0, 0)
               : LUA_ERRSYNTAX;
    if (status != LUA_OK) {
        fprintf(stderr, "lua error in %s: %s\n", path, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    free(buf);
    return status == LUA_OK ? 0 : 1;
}

/* Compile a short Lua function body and push it as a closure. */
static int push_lua_fn(lua_State *L, const char *body)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "return function(...) %s end", body);
    if (luaL_loadstring(L, buf) != LUA_OK) {
        fprintf(stderr, "hook compile error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        fprintf(stderr, "hook creation error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

/* Push __MFS_CONFIG onto the Lua stack.  The host controls every optional
 * capability and can register pre/post hooks for any filesystem/io/os/debug/load
 * operation. */
static void push_mfs_config(lua_State *L)
{
    lua_newtable(L);                    /* __MFS_CONFIG */

    lua_newtable(L);                    /* capabilities */
    /* The following are safe-ish host features that can be delegated. */
    lua_pushboolean(L, 1); lua_setfield(L, -2, "io_stdin");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "io_stdout");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "io_stderr");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "debug");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "loadstring");
    /* The following are more dangerous; enable only when you really need them. */
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "os_execute"); */
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "os_exit"); */
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "os_remove"); */
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "os_rename"); */
    lua_pushboolean(L, 1); lua_setfield(L, -2, "os_setlocale");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "os_tmpname");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "os_getenv");
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "bytecode"); */
    /* lua_pushboolean(L, 1); lua_setfield(L, -2, "native_modules"); */
    lua_setfield(L, -2, "capabilities");

    lua_newtable(L);                    /* hooks */

    lua_newtable(L);                    /* mfs_open_read */
    if (push_lua_fn(L, "print('[hook] pre mfs_open_read', ...)") == 1) {
        lua_setfield(L, -2, "pre");
    }
    if (push_lua_fn(L, "print('[hook] post mfs_open_read', ...)") == 1) {
        lua_setfield(L, -2, "post");
    }
    lua_setfield(L, -2, "mfs_open_read");

    lua_newtable(L);                    /* file_read */
    if (push_lua_fn(L, "print('[hook] pre file_read', ...)") == 1) {
        lua_setfield(L, -2, "pre");
    }
    lua_setfield(L, -2, "file_read");

    lua_newtable(L);                    /* io_open */
    if (push_lua_fn(L, "print('[hook] pre io_open', ...)") == 1) {
        lua_setfield(L, -2, "pre");
    }
    lua_setfield(L, -2, "io_open");

    lua_newtable(L);                    /* os_execute */
    if (push_lua_fn(L, "print('[hook] pre os_execute', ...)") == 1) {
        lua_setfield(L, -2, "pre");
    }
    lua_setfield(L, -2, "os_execute");

    lua_setfield(L, -2, "hooks");
    lua_setglobal(L, "__MFS_CONFIG");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <zip-or-dir> [write-dir]\n", argv[0]);
        return 1;
    }

    const char *archive = argv[1];
    const char *write_dir = (argc >= 3) ? argv[2] : ".";

    if (!PHYSFS_init(argv[0])) {
        fprintf(stderr, "PHYSFS_init failed: %s\n", mfs_last_error());
        return 1;
    }

    if (!PHYSFS_setWriteDir(write_dir)) {
        fprintf(stderr, "PHYSFS_setWriteDir(%s) failed: %s\n", write_dir, mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    if (!PHYSFS_mount(write_dir, NULL, 0)) {
        fprintf(stderr, "PHYSFS_mount write_dir(%s) failed: %s\n", write_dir, mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    if (!PHYSFS_mount(archive, NULL, 1)) {
        fprintf(stderr, "PHYSFS_mount(%s) failed: %s\n", archive, mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "luaL_newstate failed\n");
        PHYSFS_deinit();
        return 1;
    }

    /* Keep the base libraries (string, table, math, etc.) available. */
    luaL_openlibs(L);

    /* Register the low-level MFS module so scripts/shim.lua can use it. */
    luaL_requiref(L, "mfs", luaopen_mfs, 1);
    lua_pop(L, 1);

    /* Configure the sandbox before shim.lua is loaded. */
    push_mfs_config(L);

    /* Load scripts/shim.lua. It captures the original io/os/debug/etc.,
     * removes the unsafe parts, and installs PhysFS-backed shims. */
    if (run_lua_file(L, "shim.lua") != 0) {
        lua_close(L);
        PHYSFS_deinit();
        return 1;
    }

    /* Now load and run the application's entry point. */
    int status = run_lua_file(L, "main.lua");

    lua_close(L);
    PHYSFS_deinit();
    return status;
}
