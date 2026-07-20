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
    lua_createtable(L, 0, 7);
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

static int mfs_lua_is_symlink(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, mfs_is_symlink(path));
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

/* ========================================================================= */
/* Mount / search path                                                       */
/* ========================================================================= */

static int mfs_lua_mount(lua_State *L)
{
    const char *dir = luaL_checkstring(L, 1);
    const char *mountPoint = luaL_optstring(L, 2, NULL);
    int append = lua_toboolean(L, 3);
    int ok = mfs_mount(dir, mountPoint, append);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_unmount(lua_State *L)
{
    const char *dir = luaL_checkstring(L, 1);
    int ok = mfs_unmount(dir);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_get_real_dir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    const char *real = mfs_get_real_dir(path);
    if (!real) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushstring(L, real);
    return 1;
}

static int mfs_lua_get_mount_point(lua_State *L)
{
    const char *dir = luaL_checkstring(L, 1);
    const char *mp = mfs_get_mount_point(dir);
    if (!mp) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushstring(L, mp);
    return 1;
}

struct mfs_lua_sp_state {
    lua_State *L;
    int idx;
};

static int mfs_lua_sp_cb(void *userdata, const char *entry)
{
    struct mfs_lua_sp_state *st = (struct mfs_lua_sp_state *)userdata;
    lua_pushinteger(st->L, ++st->idx);
    lua_pushstring(st->L, entry);
    lua_settable(st->L, -3);
    return 1;
}

static int mfs_lua_get_search_path(lua_State *L)
{
    lua_newtable(L);
    struct mfs_lua_sp_state st = { L, 0 };
    mfs_get_search_path(mfs_lua_sp_cb, &st);
    return 1;
}

/* ========================================================================= */
/* Directory helpers                                                         */
/* ========================================================================= */

static int mfs_lua_get_base_dir(lua_State *L)
{
    lua_pushstring(L, mfs_get_base_dir());
    return 1;
}

static int mfs_lua_get_write_dir(lua_State *L)
{
    const char *wd = mfs_get_write_dir();
    if (!wd) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, wd);
    return 1;
}

static int mfs_lua_set_write_dir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int ok = mfs_set_write_dir(path);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_get_user_dir(lua_State *L)
{
    lua_pushstring(L, mfs_get_user_dir());
    return 1;
}

static int mfs_lua_get_pref_dir(lua_State *L)
{
    const char *org = luaL_checkstring(L, 1);
    const char *app = luaL_checkstring(L, 2);
    const char *dir = mfs_get_pref_dir(org, app);
    if (!dir) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushstring(L, dir);
    return 1;
}

static int mfs_lua_get_dir_separator(lua_State *L)
{
    lua_pushstring(L, mfs_get_dir_separator());
    return 1;
}

/* ========================================================================= */
/* Symlink policy                                                            */
/* ========================================================================= */

static int mfs_lua_permit_symlinks(lua_State *L)
{
    int allow = lua_toboolean(L, 1);
    mfs_permit_symlinks(allow);
    lua_pushboolean(L, 1);
    return 1;
}

static int mfs_lua_symlinks_permitted(lua_State *L)
{
    lua_pushboolean(L, mfs_symlinks_permitted());
    return 1;
}

/* ========================================================================= */
/* Touch                                                                     */
/* ========================================================================= */

static int mfs_lua_touch(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int ok = mfs_touch(path);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* ========================================================================= */
/* setRoot                                                                   */
/* ========================================================================= */

static int mfs_lua_set_root(lua_State *L)
{
    const char *archive = luaL_checkstring(L, 1);
    const char *subdir = luaL_checkstring(L, 2);
    int ok = mfs_set_root(archive, subdir);
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, mfs_last_error());
        return 2;
    }
    lua_pushboolean(L, 1);
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
    { "is_symlink", mfs_lua_is_symlink },
    { "list", mfs_lua_list },
    { "list_ex", mfs_lua_list_ex },
    { "mkdir", mfs_lua_mkdir },
    { "remove", mfs_lua_remove },
    { "touch", mfs_lua_touch },
    { "mount", mfs_lua_mount },
    { "unmount", mfs_lua_unmount },
    { "get_real_dir", mfs_lua_get_real_dir },
    { "get_mount_point", mfs_lua_get_mount_point },
    { "get_search_path", mfs_lua_get_search_path },
    { "get_base_dir", mfs_lua_get_base_dir },
    { "get_write_dir", mfs_lua_get_write_dir },
    { "set_write_dir", mfs_lua_set_write_dir },
    { "get_user_dir", mfs_lua_get_user_dir },
    { "get_pref_dir", mfs_lua_get_pref_dir },
    { "get_dir_separator", mfs_lua_get_dir_separator },
    { "permit_symlinks", mfs_lua_permit_symlinks },
    { "symlinks_permitted", mfs_lua_symlinks_permitted },
    { "set_root", mfs_lua_set_root },
    { NULL, NULL }
};

int luaopen_mfs(lua_State *L)
{
    luaL_newlib(L, mfs_lua_funcs);  /* module table at stack top */

    /* Extend the mfs module table with LFS-compatible high-level functions
     * and convenience wrappers written in pure Lua.  The low-level C
     * bindings are already on the table; this chunk adds attributes(),
     * symlinkattributes(), currentdir(), chdir(), dir(), rmdir(), and
     * sensible stubs for link/setmode/lock/lock_dir/unlock/unlock_dir.
     * After this, require("mfs") returns a superset of LuaFileSystem. */
    if (luaL_loadstring(L,
        "local M = ...\n"
        "local raw_stat = M.stat\n"
        "\n"
        "-- Virtual current working directory (seeded from the write dir).\n"
        "M._cwd = '/'\n"
        "\n"
        "-- Map PhysFS filetype to LFS mode string.\n"
        "local function _filetype_to_mode(ftype)\n"
        "  if ftype == 'directory' then return 'directory'\n"
        "  elseif ftype == 'file' then return 'file'\n"
        "  elseif ftype == 'symlink' then return 'link'\n"
        "  else return 'other' end\n"
        "end\n"
        "\n"
        "-- Resolve a path relative to the virtual CWD.\n"
        "local function _resolve(path)\n"
        "  if path == nil or path == '' then return M._cwd end\n"
        "  if path:sub(1,1) == '/' then return path end\n"
        "  if M._cwd == '/' then return '/' .. path end\n"
        "  return M._cwd .. '/' .. path\n"
        "end\n"
        "\n"
        "--[[ attributes(path, [field])\n"
        "   LFS-compatible file/directory attributes.\n"
        "   Without 'field', returns a table.  With 'field', returns only\n"
        "   that value (or nil + error if invalid field).\n"
        "   Fields: mode, size, modification, access, change,\n"
        "           permissions, nlink, dev, ino, uid, gid, rdev.\n"
        "]]\n"
        "function M.attributes(path, field)\n"
        "  path = _resolve(path)\n"
        "  local st = raw_stat(path)\n"
        "  if not st then return nil, M.last_error() end\n"
        "  local t = {\n"
        "    mode = _filetype_to_mode(st.type),\n"
        "    size = st.size,\n"
        "    modification = st.modtime,\n"
        "    access = st.accesstime,\n"
        "    change = st.createtime,\n"
        "    permissions = st.readonly and 'r--r--r--' or 'rw-rw-rw-',\n"
        "    nlink = 1,\n"
        "    dev = 0,\n"
        "    ino = 0,\n"
        "    uid = 0,\n"
        "    gid = 0,\n"
        "    rdev = 0,\n"
        "  }\n"
        "  if field then\n"
        "    local v = t[field]\n"
        "    if v == nil then error(\"invalid attribute name '\" .. field .. \"'\", 2) end\n"
        "    return v\n"
        "  end\n"
        "  return t\n"
        "end\n"
        "\n"
        "--[[ symlinkattributes(path, [field])\n"
        "   Like attributes(), but returns symlink info.  Adds a 'target'\n"
        "   field (always nil from PhysFS – we cannot resolve link targets).\n"
        "]]\n"
        "function M.symlinkattributes(path, field)\n"
        "  if field == 'target' then return nil, 'PhysFS does not resolve link targets' end\n"
        "  return M.attributes(path, field)\n"
        "end\n"
        "\n"
        "-- currentdir() – return the virtual CWD.\n"
        "function M.currentdir()\n"
        "  return M._cwd\n"
        "end\n"
        "\n"
        "-- chdir(path) – change the virtual CWD.\n"
        "function M.chdir(path)\n"
        "  path = _resolve(path)\n"
        "  if M.is_dir(path) then\n"
        "    M._cwd = path\n"
        "    return true\n"
        "  end\n"
        "  return nil, M.last_error(), 0\n"
        "end\n"
        "\n"
        "-- dir(path) – LFS-style directory iterator.\n"
        "function M.dir(path)\n"
        "  path = _resolve(path)\n"
        "  local entries = M.list(path)\n"
        "  if not entries then return nil, M.last_error() end\n"
        "  local i = 0\n"
        "  return function()\n"
        "    i = i + 1\n"
        "    local name = entries[i]\n"
        "    if name then return name end\n"
        "  end\n"
        "end\n"
        "\n"
        "-- rmdir(path) – alias for remove.  Works on empty directories and files.\n"
        "function M.rmdir(path)\n"
        "  return M.remove(_resolve(path))\n"
        "end\n"
        "\n"
        "-- link(old, new, [symlink]) – not supported in PhysFS sandbox.\n"
        "function M.link(old, new, symlink)\n"
        "  return nil, 'link is not available in the PhysFS sandbox'\n"
        "end\n"
        "\n"
        "-- setmode(file, mode) – PhysFS is always binary; not supported.\n"
        "function M.setmode(file, mode)\n"
        "  return nil, 'setmode is not available in the PhysFS sandbox'\n"
        "end\n"
        "\n"
        "--[[ lock_dir(path, [stale_seconds])\n"
        "   Creates a lockfile.lfs marker in the directory, LFS-compatible.\n"
        "   Returns a lock object for use with unlock_dir().\n"
        "   If stale_seconds is given and the existing lock is older than\n"
        "   that, the stale lock is broken and a new one is acquired.\n"
        "]]\n"
        "function M.lock_dir(path, stale_seconds)\n"
        "  path = _resolve(path)\n"
        "  local lock_path = path .. '/lockfile.lfs'\n"
        "  if M.exists(lock_path) then\n"
        "    if stale_seconds then\n"
        "      local st = raw_stat(lock_path)\n"
        "      if st and os.time() - st.modtime > stale_seconds then\n"
        "        M.remove(lock_path)  -- break stale lock\n"
        "      else\n"
        "        return nil, 'directory is locked'\n"
        "      end\n"
        "    else\n"
        "      return nil, 'directory is locked'\n"
        "    end\n"
        "  end\n"
        "  local ok = M.write_file(lock_path, '', 0)\n"
        "  if not ok then return nil, M.last_error() end\n"
        "  return { _path = lock_path, _dir = path }\n"
        "end\n"
        "\n"
        "-- unlock_dir(lock) – remove the lockfile created by lock_dir.\n"
        "function M.unlock_dir(lock)\n"
        "  if type(lock) ~= 'table' or not lock._path then\n"
        "    return nil, 'invalid lock'\n"
        "  end\n"
        "  local ok = M.remove(lock._path)\n"
        "  if not ok then return nil, M.last_error() end\n"
        "  return true\n"
        "end\n"
        "\n"
        "--[[ In-process advisory file lock registry.\n"
        "   Tracks (handle, start, len, mode) tuples.  lock() checks for\n"
        "   conflicting regions; unlock() releases them.  This provides\n"
        "   intra-process advisory locking compatible with LFS semantics.\n"
        "]]\n"
        "M._lock_registry = {}\n"
        "setmetatable(M._lock_registry, {__mode = 'k'})  -- weak keys\n"
        "\n"
        "function M.lock(file, mode, start, len)\n"
        "  if type(file) ~= 'userdata' and type(file) ~= 'table' then\n"
        "    return nil, 'bad argument #1 (expected file handle)'\n"
        "  end\n"
        "  mode = mode or 'r'\n"
        "  start = start or 0\n"
        "  -- PhysFS doesn't expose file size for seek-to-end, so len=0 means\n"
        "  -- lock to end-of-file (the whole file from start).\n"
        "  local file_len\n"
        "  if type(file) == 'table' and file.size then\n"
        "    file_len = file:size()  -- shim.lua proxy\n"
        "  end\n"
        "  if len == nil or len == 0 then\n"
        "    len = file_len or (2^62)  -- effectively 'to end'\n"
        "  end\n"
        "  local end_pos = start + len - 1\n"
        "\n"
        "  -- Check for conflicts.\n"
        "  local locks = M._lock_registry[file]\n"
        "  if locks then\n"
        "    for _, existing in ipairs(locks) do\n"
        "      local e_end = existing.start + existing.len - 1\n"
        "      -- Overlap check: two ranges overlap if start1 <= end2 AND start2 <= end1\n"
        "      if start <= e_end and existing.start <= end_pos then\n"
        "        -- Exclusive lock conflicts with any lock; shared only conflicts with exclusive\n"
        "        if mode == 'w' or existing.mode == 'w' then\n"
        "          return nil, 'conflicting lock'\n"
        "        end\n"
        "      end\n"
        "    end\n"
        "  else\n"
        "    locks = {}\n"
        "    M._lock_registry[file] = locks\n"
        "  end\n"
        "\n"
        "  locks[#locks + 1] = { start = start, len = len, mode = mode }\n"
        "  return true\n"
        "end\n"
        "\n"
        "function M.unlock(file, start, len)\n"
        "  if type(file) ~= 'userdata' and type(file) ~= 'table' then\n"
        "    return nil, 'bad argument #1 (expected file handle)'\n"
        "  end\n"
        "  start = start or 0\n"
        "  local file_len\n"
        "  if type(file) == 'table' and file.size then\n"
        "    file_len = file:size()\n"
        "  end\n"
        "  if len == nil or len == 0 then\n"
        "    len = file_len or (2^62)\n"
        "  end\n"
        "  local end_pos = start + len - 1\n"
        "\n"
        "  local locks = M._lock_registry[file]\n"
        "  if not locks then return nil, 'no locks for this file' end\n"
        "\n"
        "  for i, existing in ipairs(locks) do\n"
        "    local e_end = existing.start + existing.len - 1\n"
        "    if start <= e_end and existing.start <= end_pos then\n"
        "      table.remove(locks, i)\n"
        "      if #locks == 0 then M._lock_registry[file] = nil end\n"
        "      return true\n"
        "    end\n"
        "  end\n"
        "  return nil, 'no matching lock'\n"
        "end\n"
        "\n"
        "-- Module metadata (LFS-compatible).\n"
        "M._VERSION = 'mfs ' .. (M._VERSION or '0.1.0')\n"
        "M._DESCRIPTION = 'PhysFS-backed file system module (LFS-compatible)'\n"
        "\n"
        "return M\n"
    ) == LUA_OK) {
        lua_pushvalue(L, -2);  /* duplicate module table as argument */
        lua_call(L, 1, 0);     /* call the chunk; it mutates the module table */
    } else {
        /* If the bootstrap chunk fails to compile (shouldn't happen), pop
         * the error message and return the raw module table. */
        lua_pop(L, 1);
    }

    return 1;
}
