/*
 * test_main.c — minimal harness to test the mfs module directly.
 * Mounts /tmp as the write dir and the current dir as the search root,
 * loads the mfs module, runs test_mfs.lua.
 */
#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <physfs.h>
#include "mfs.h"

extern int luaopen_mfs(lua_State *L);

int main(void)
{
    if (!PHYSFS_init("test_mfs")) {
        fprintf(stderr, "PHYSFS_init: %s\n", mfs_last_error());
        return 1;
    }
    if (!PHYSFS_setWriteDir("/tmp")) {
        fprintf(stderr, "PHYSFS_setWriteDir: %s\n", mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }
    if (!PHYSFS_mount("/tmp", NULL, 0)) {
        fprintf(stderr, "PHYSFS_mount /tmp: %s\n", mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }
    if (!PHYSFS_mount(".", NULL, 0)) {
        fprintf(stderr, "PHYSFS_mount .: %s\n", mfs_last_error());
        PHYSFS_deinit();
        return 1;
    }

    lua_State *L = luaL_newstate();
    if (!L) { fprintf(stderr, "luaL_newstate failed\n"); PHYSFS_deinit(); return 1; }
    luaL_openlibs(L);
    luaL_requiref(L, "mfs", luaopen_mfs, 1);
    lua_pop(L, 1);

    size_t len;
    char *src = mfs_load_text("test_mfs.lua", &len);
    if (!src) {
        fprintf(stderr, "could not load test_mfs.lua: %s\n", mfs_last_error());
        lua_close(L); PHYSFS_deinit();
        return 1;
    }
    int status = luaL_loadbuffer(L, src, len, "test_mfs.lua");
    free(src);
    if (status != LUA_OK) {
        fprintf(stderr, "load error: %s\n", lua_tostring(L, -1));
        lua_close(L); PHYSFS_deinit();
        return 1;
    }
    status = lua_pcall(L, 0, 0, 0);
    if (status != LUA_OK) {
        fprintf(stderr, "runtime error: %s\n", lua_tostring(L, -1));
        lua_close(L); PHYSFS_deinit();
        return 1;
    }
    lua_close(L);
    PHYSFS_deinit();
    return 0;
}
