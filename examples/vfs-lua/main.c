/*
 * examples/vfs-lua/main.c
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
#include "md_vfs.h"

/* Convert "foo.bar" to "foo/bar.lua" (or platform-appropriate separator). */
static const char *module_name_to_path(lua_State *L, const char *name,
                                       char *out, size_t out_len)
{
    /* Use the directory separator reported by package.config. */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "config");
    const char *config = lua_tostring(L, -1);
    char dirsep = (config && config[0]) ? config[0] : '/';
    lua_pop(L, 2);

    size_t n = strlen(name);
    if (n + 4 >= out_len) return NULL;

    for (size_t i = 0; i < n; ++i)
        out[i] = (name[i] == '.') ? dirsep : name[i];
    out[n] = '.'; out[n + 1] = 'l'; out[n + 2] = 'u'; out[n + 3] = 'a';
    out[n + 4] = '\0';
    return out;
}

/* Custom package.searcher that loads Lua modules from PhysFS. */
static int md_vfs_lua_searcher(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    char path[512];
    if (!module_name_to_path(L, name, path, sizeof(path))) {
        lua_pushfstring(L, "\n\tmodule name too long: '%s'", name);
        return 1;
    }

    size_t len;
    char *buf = md_vfs_load_text(path, &len);
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
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "dofile");

    /* Prevent loading native C modules. */
    lua_getglobal(L, "package");
    lua_pushnil(L);   lua_setfield(L, -2, "loadlib");
    lua_pushstring(L, ""); lua_setfield(L, -2, "cpath");

    /* Replace package.searchers with only the VFS searcher. */
    lua_newtable(L);
    lua_pushcfunction(L, md_vfs_lua_searcher);
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
        fprintf(stderr, "PHYSFS_init failed: %s\n", md_vfs_last_error());
        return 1;
    }

    if (!PHYSFS_mount(argv[1], NULL, 1)) {
        fprintf(stderr, "PHYSFS_mount(%s) failed: %s\n", argv[1], md_vfs_last_error());
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
    char *main_script = md_vfs_load_text("main.lua", &len);
    if (!main_script) {
        fprintf(stderr, "could not load main.lua: %s\n", md_vfs_last_error());
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
