/*
 * examples/mfs-lua/main.c
 *
 * Demonstrates a sandboxed Lua state where scripts are loaded from a
 * PhysicsFS-mounted archive. Direct filesystem access is removed from the Lua
 * global environment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <physfs.h>
#include "mfs.h"

/* Convert "foo.bar" to "foo/bar.lua".
 *
 * PhysFS always uses '/' as the path separator, so we ignore the host
 * filesystem separator from package.config. */
static const char *module_name_to_path(const char *name, char *out, size_t out_len)
{
    size_t n = strlen(name);
    if (n + 4 >= out_len) return NULL;

    for (size_t i = 0; i < n; ++i)
        out[i] = (name[i] == '.') ? '/' : name[i];
    out[n] = '.'; out[n + 1] = 'l'; out[n + 2] = 'u'; out[n + 3] = 'a';
    out[n + 4] = '\0';
    return out;
}

/* Custom package.searcher that loads Lua modules from PhysFS. */
static int mfs_lua_searcher(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    char path[512];
    if (!module_name_to_path(name, path, sizeof(path))) {
        lua_pushfstring(L, "\n\tmodule name too long: '%s'", name);
        return 1;
    }

    size_t len;
    char *buf = mfs_load_text(path, &len);
    if (!buf) {
        lua_pushfstring(L, "\n\tno VFS file '%s'", path);
        return 1;
    }

    int status = luaL_loadbuffer(L, buf, len, path);
    free(buf);
    if (status != LUA_OK) {
        return luaL_error(L, "error loading module '%s' from VFS file '%s':\n\t%s",
                          name, path, lua_tostring(L, -1));
    }

    lua_pushstring(L, path); /* 2nd argument to the module loader */
    return 2;
}

/* Remove dangerous standard-library globals from the Lua state. */
static void lua_sandbox(lua_State *L)
{
    lua_pushnil(L); lua_setglobal(L, "io");
    lua_pushnil(L); lua_setglobal(L, "os");
    lua_pushnil(L); lua_setglobal(L, "debug");
    lua_pushnil(L); lua_setglobal(L, "load");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "dofile");
#if LUA_VERSION_NUM < 502
    lua_pushnil(L); lua_setglobal(L, "loadstring");
#endif

    /* Prevent loading native C modules and searching the host filesystem. */
    lua_getglobal(L, "package");
    lua_pushnil(L); lua_setfield(L, -2, "loadlib");
    lua_pushstring(L, ""); lua_setfield(L, -2, "cpath");
    lua_pushstring(L, ""); lua_setfield(L, -2, "path");

    /* Replace package.searchers with only the VFS searcher. */
    lua_newtable(L);
    lua_pushcfunction(L, mfs_lua_searcher);
    lua_rawseti(L, -2, 1);
    lua_setfield(L, -2, "searchers");
    lua_pop(L, 1);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <zip-or-dir>\n", argv[0]);
        return 1;
    }

    if (!PHYSFS_init(argv[0])) {
        fprintf(stderr, "PHYSFS_init failed: %s\n", mfs_last_error());
        return 1;
    }

    if (!PHYSFS_mount(argv[1], NULL, 1)) {
        fprintf(stderr, "PHYSFS_mount(%s) failed: %s\n", argv[1], mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "luaL_newstate failed\n");
        PHYSFS_deinit();
        return 1;
    }

    /* Keep the base libraries (string, table, math, etc.) but remove I/O. */
    luaL_openlibs(L);
    lua_sandbox(L);

    /* Load and run main.lua from the PhysFS archive. */
    size_t len;
    char *main_script = mfs_load_text("main.lua", &len);
    if (!main_script) {
        fprintf(stderr, "could not load main.lua: %s\n", mfs_last_error());
        lua_close(L);
        PHYSFS_deinit();
        return 1;
    }

    int status = luaL_loadbuffer(L, main_script, len, "main.lua") == LUA_OK
               ? lua_pcall(L, 0, 0, 0)
               : LUA_ERRSYNTAX;
    if (status != LUA_OK) {
        fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    free(main_script);
    lua_close(L);
    PHYSFS_deinit();
    return status == LUA_OK ? 0 : 1;
}
