/* examples/mfs-lua/mfs_lua.c
 *
 * Lua C module that exposes the MFS (PhysFS helper) API to Lua.
 *
 * This module is intentionally low-level: it wraps the C MFS functions with
 * as little policy as possible. The higher-level io/os/loadfile/dofile shims
 * live in scripts/shim.lua, which is loaded by main.c after this module is
 * registered.
 */

#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <physfs.h>
#include "mfs.h"

#define MFS_FILE_MT "mfs_file"

/* -------------------------------------------------------------------------- */
/* File handle userdata                                                       */
/* -------------------------------------------------------------------------- */

typedef struct {
    PHYSFS_File *f;
} mfs_lua_file;

static PHYSFS_File *mfs_lua_checkfile(lua_State *L, int idx)
{
    mfs_lua_file *hf = (mfs_lua_file *)luaL_checkudata(L, idx, MFS_FILE_MT);
    if (!hf->f) luaL_error(L, "attempt to use a closed file");
    return hf->f;
}

static int mfs_lua_file_close(lua_State *L)
{
    mfs_lua_file *hf = (mfs_lua_file *)luaL_checkudata(L, 1, MFS_FILE_MT);
    if (hf->f) {
        mfs_close(hf->f);
        hf->f = NULL;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_file_read(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    lua_Integer len = luaL_checkinteger(L, 2);
    if (len < 0) luaL_error(L, "read length must be non-negative");
    if (len == 0) {
        lua_pushliteral(L, "");
        return 1;
    }
    char *buf = (char *)malloc((size_t)len);
    if (!buf) luaL_error(L, "out of memory");
    PHYSFS_sint64 n = mfs_read(f, buf, (size_t)len);
    if (n < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    if (n == 0) {
        free(buf);
        lua_pushnil(L);
        return 1; /* EOF */
    }
    lua_pushlstring(L, buf, (size_t)n);
    free(buf);
    return 1;
}

static int mfs_lua_file_write(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    size_t len;
    const char *s = luaL_checklstring(L, 2, &len);
    if (len == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    PHYSFS_sint64 n = mfs_write(f, s, len);
    if (n < 0) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)n);
    return 1;
}

static int mfs_lua_file_flush(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    int ok = mfs_flush(f);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_file_seek(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    const char *whence = luaL_optstring(L, 2, "cur");
    lua_Integer offset = luaL_optinteger(L, 3, 0);
    PHYSFS_sint64 base = 0;
    if (strcmp(whence, "set") == 0) {
        base = 0;
    } else if (strcmp(whence, "cur") == 0) {
        base = mfs_tell(f);
        if (base < 0) {
            lua_pushnil(L);
            lua_pushstring(L, mfs_last_error());
            return 2;
        }
    } else if (strcmp(whence, "end") == 0) {
        base = mfs_file_size(f);
        if (base < 0) {
            lua_pushnil(L);
            lua_pushstring(L, mfs_last_error());
            return 2;
        }
    } else {
        luaL_error(L, "bad argument #2 to 'seek' (invalid whence)");
    }
    PHYSFS_uint64 pos = (PHYSFS_uint64)(base + offset);
    if (offset < 0 && (PHYSFS_uint64)(-offset) > (PHYSFS_uint64)base) {
        lua_pushnil(L);
        lua_pushstring(L, "seek before start of file");
        return 2;
    }
    if (!mfs_seek(f, pos)) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)pos);
    return 1;
}

static int mfs_lua_file_tell(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    PHYSFS_sint64 pos = mfs_tell(f);
    if (pos < 0) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)pos);
    return 1;
}

static int mfs_lua_file_eof(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    lua_pushboolean(L, mfs_eof(f));
    return 1;
}

static int mfs_lua_file_size(lua_State *L)
{
    PHYSFS_File *f = mfs_lua_checkfile(L, 1);
    PHYSFS_sint64 sz = mfs_file_size(f);
    if (sz < 0) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushinteger(L, (lua_Integer)sz);
    return 1;
}

static int mfs_lua_file_gc(lua_State *L)
{
    mfs_lua_file *hf = (mfs_lua_file *)luaL_checkudata(L, 1, MFS_FILE_MT);
    if (hf->f) {
        mfs_close(hf->f);
        hf->f = NULL;
    }
    return 0;
}

static int mfs_lua_file_tostring(lua_State *L)
{
    mfs_lua_file *hf = (mfs_lua_file *)luaL_checkudata(L, 1, MFS_FILE_MT);
    if (hf->f)
        lua_pushfstring(L, "mfs_file (%p)", (void *)hf->f);
    else
        lua_pushliteral(L, "mfs_file (closed)");
    return 1;
}

static void mfs_lua_pushfile(lua_State *L, PHYSFS_File *f)
{
    mfs_lua_file *hf = (mfs_lua_file *)lua_newuserdata(L, sizeof(mfs_lua_file));
    hf->f = f;
    if (luaL_newmetatable(L, MFS_FILE_MT)) {
        lua_pushcfunction(L, mfs_lua_file_close);
        lua_setfield(L, -2, "close");
        lua_pushcfunction(L, mfs_lua_file_read);
        lua_setfield(L, -2, "read");
        lua_pushcfunction(L, mfs_lua_file_write);
        lua_setfield(L, -2, "write");
        lua_pushcfunction(L, mfs_lua_file_flush);
        lua_setfield(L, -2, "flush");
        lua_pushcfunction(L, mfs_lua_file_seek);
        lua_setfield(L, -2, "seek");
        lua_pushcfunction(L, mfs_lua_file_tell);
        lua_setfield(L, -2, "tell");
        lua_pushcfunction(L, mfs_lua_file_eof);
        lua_setfield(L, -2, "eof");
        lua_pushcfunction(L, mfs_lua_file_size);
        lua_setfield(L, -2, "size");
        lua_pushcfunction(L, mfs_lua_file_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, mfs_lua_file_tostring);
        lua_setfield(L, -2, "__tostring");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

/* -------------------------------------------------------------------------- */
/* Module functions                                                           */
/* -------------------------------------------------------------------------- */

static int mfs_lua_load_text(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t len;
    char *buf = mfs_load_text(path, &len);
    if (!buf) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

static int mfs_lua_load(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t len;
    void *buf = mfs_load(path, &len);
    if (!buf) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushlstring(L, (const char *)buf, len);
    free(buf);
    return 1;
}

static int mfs_lua_write_file(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int ok = mfs_write_file(path, data, len);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_append_file(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int ok = mfs_append_file(path, data, len);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_exists(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, mfs_exists(path));
    return 1;
}

static int mfs_lua_last_error(lua_State *L)
{
    lua_pushstring(L, mfs_last_error());
    return 1;
}

static int mfs_lua_open_read(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    PHYSFS_File *f = mfs_open_read(path);
    if (!f) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    mfs_lua_pushfile(L, f);
    return 1;
}

static int mfs_lua_open_write(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    PHYSFS_File *f = mfs_open_write(path);
    if (!f) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    mfs_lua_pushfile(L, f);
    return 1;
}

static int mfs_lua_open_append(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    PHYSFS_File *f = mfs_open_append(path);
    if (!f) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    mfs_lua_pushfile(L, f);
    return 1;
}

static int mfs_lua_stat(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    PHYSFS_Stat st;
    if (!mfs_stat(path, &st)) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, (lua_Integer)st.filesize);
    lua_setfield(L, -2, "size");
    lua_pushinteger(L, (lua_Integer)st.modtime);
    lua_setfield(L, -2, "modtime");
    lua_pushinteger(L, (lua_Integer)st.createtime);
    lua_setfield(L, -2, "createtime");
    lua_pushinteger(L, (lua_Integer)st.accesstime);
    lua_setfield(L, -2, "accesstime");
    const char *ftype = "other";
    if (st.filetype == PHYSFS_FILETYPE_REGULAR) ftype = "file";
    else if (st.filetype == PHYSFS_FILETYPE_DIRECTORY) ftype = "directory";
    else if (st.filetype == PHYSFS_FILETYPE_SYMLINK) ftype = "symlink";
    lua_pushstring(L, ftype);
    lua_setfield(L, -2, "type");
    lua_pushboolean(L, st.readonly);
    lua_setfield(L, -2, "readonly");
    return 1;
}

static int mfs_lua_is_dir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, mfs_is_dir(path));
    return 1;
}

static int mfs_lua_is_file(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, mfs_is_file(path));
    return 1;
}

static int mfs_lua_mkdir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int ok = mfs_mkdir(path);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_remove(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int ok = mfs_remove(path);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

struct mfs_lua_list_state {
    lua_State *L;
    int idx;
};

static int mfs_lua_list_cb(void *userdata, const char *name)
{
    struct mfs_lua_list_state *st = (struct mfs_lua_list_state *)userdata;
    lua_pushinteger(st->L, ++st->idx);
    lua_pushstring(st->L, name);
    lua_settable(st->L, -3);
    return 1;
}

static int mfs_lua_list(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_newtable(L);
    struct mfs_lua_list_state st = { L, 0 };
    if (!mfs_list(path, mfs_lua_list_cb, &st)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    return 1;
}

struct mfs_lua_list_ex_state {
    lua_State *L;
    int idx;
};

static int mfs_lua_list_ex_cb(void *userdata, const mfs_dir_entry *entry)
{
    struct mfs_lua_list_ex_state *st = (struct mfs_lua_list_ex_state *)userdata;
    lua_State *L = st->L;
    lua_newtable(L);
    lua_pushstring(L, entry->name);
    lua_setfield(L, -2, "name");
    lua_pushboolean(L, entry->is_dir);
    lua_setfield(L, -2, "is_dir");
    lua_pushboolean(L, entry->is_file);
    lua_setfield(L, -2, "is_file");
    lua_pushboolean(L, entry->is_symlink);
    lua_setfield(L, -2, "is_symlink");
    lua_pushinteger(L, (lua_Integer)entry->size);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, entry->readonly);
    lua_setfield(L, -2, "readonly");
    lua_rawseti(L, -2, ++st->idx);
    return 1;
}

static int mfs_lua_list_ex(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_newtable(L);
    struct mfs_lua_list_ex_state st = { L, 0 };
    if (!mfs_list_ex(path, mfs_lua_list_ex_cb, &st)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    return 1;
}

static const luaL_Reg mfs_lua_funcs[] = {
    { "load_text", mfs_lua_load_text },
    { "load", mfs_lua_load },
    { "write_file", mfs_lua_write_file },
    { "append_file", mfs_lua_append_file },
    { "exists", mfs_lua_exists },
    { "last_error", mfs_lua_last_error },
    { "open_read", mfs_lua_open_read },
    { "open_write", mfs_lua_open_write },
    { "open_append", mfs_lua_open_append },
    { "stat", mfs_lua_stat },
    { "is_dir", mfs_lua_is_dir },
    { "is_file", mfs_lua_is_file },
    { "list", mfs_lua_list },
    { "list_ex", mfs_lua_list_ex },
    { "mkdir", mfs_lua_mkdir },
    { "remove", mfs_lua_remove },
    { NULL, NULL }
};

int luaopen_mfs(lua_State *L)
{
    luaL_newlib(L, mfs_lua_funcs);
    return 1;
}
