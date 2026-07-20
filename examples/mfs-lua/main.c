/*
 * examples/mfs-lua/main.c
 *
 * Demonstrates a sandboxed Lua state where scripts are loaded from a
 * PhysicsFS-mounted archive. Direct filesystem access is removed from the Lua
 * global environment and replaced with PhysFS-backed shims loaded from
 * scripts/shim.lua.
 *
 * Usage: mfs-lua <archive-or-dir> [write-dir]
 *
 * The optional write-dir argument sets the PhysFS write directory. If omitted,
 * the current working directory is used. The write directory is also mounted so
 * the io shim can read back what it writes.
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

    /* Load scripts/shim.lua first. It captures the original io/os/debug/etc.,
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
