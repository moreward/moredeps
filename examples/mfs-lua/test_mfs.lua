-- Test all MFS functions: low-level C bindings + LFS-compatible wrappers
-- Run via ./test_mfs  (write dir = /tmp, search path includes /tmp and .)
--
-- IMPORTANT: all PhysFS write paths are relative to the write dir (/tmp).
-- "/tmp/foo" in the virtual FS means the OS path /tmp/tmp/foo.
-- Use bare filenames or subdirectory paths.

local mfs = require("mfs")

local passed, failed = 0, 0
local function check(name, ok, extra)
    if ok then
        passed = passed + 1
    else
        failed = failed + 1
        io.stderr:write("FAIL: " .. name .. (extra and (" -- " .. tostring(extra)) or "") .. "\n")
    end
end

local function eq(a, b, name)
    if a == b then
        passed = passed + 1
    else
        failed = failed + 1
        io.stderr:write(string.format("FAIL: %s -- expected %q, got %q\n", name, tostring(b), tostring(a)))
    end
end

-- helper: check that table contains a value
local function contains(t, val, name)
    for _, v in ipairs(t) do
        if v == val then
            passed = passed + 1
            return
        end
    end
    failed = failed + 1
    io.stderr:write(string.format("FAIL: %s -- table missing %q\n", name, tostring(val)))
end

-- helper: find entry in list_ex by name
local function find_ex(entries, name, field)
    for _, e in ipairs(entries) do
        if e.name == name then return e[field] end
    end
end

print("=== Low-level C bindings ===")

-- last_error
check("last_error returns string", type(mfs.last_error()) == "string")

-- exists
check("exists on non-existent", not mfs.exists("/nonexistent_xyzzy"))

-- is_dir / is_file / is_symlink
check("is_dir negative", not mfs.is_dir("/nonexistent_xyzzy"))
check("is_file negative", not mfs.is_file("/nonexistent_xyzzy"))
check("is_symlink negative", not mfs.is_symlink("/nonexistent_xyzzy"))

-- stat negative
local st = mfs.stat("/nonexistent_xyzzy")
check("stat negative returns nil", st == nil)

-- get_base_dir (returns full path on macOS, not ".")
local bd = mfs.get_base_dir()
check("get_base_dir returns string", type(bd) == "string" and #bd > 0)
print("  get_base_dir = " .. bd)

-- get_write_dir
local wd = mfs.get_write_dir()
eq(wd, "/tmp", "get_write_dir matches /tmp")

-- set_write_dir
local ok = mfs.set_write_dir("/tmp")
check("set_write_dir /tmp", ok == true)

-- get_user_dir
local ud = mfs.get_user_dir()
check("get_user_dir returns string", type(ud) == "string")
print("  get_user_dir = " .. ud)

-- get_pref_dir
local pd = mfs.get_pref_dir("testorg", "testapp")
check("get_pref_dir returns string", type(pd) == "string")
print("  get_pref_dir = " .. pd)

-- get_dir_separator
local sep = mfs.get_dir_separator()
check("get_dir_separator returns string", type(sep) == "string" and #sep > 0)
print("  get_dir_separator = " .. sep)

-- symlinks_permitted
check("symlinks_permitted default false", mfs.symlinks_permitted() == false)
mfs.permit_symlinks(true)
check("permit_symlinks(true)", mfs.symlinks_permitted() == true)
mfs.permit_symlinks(false)
check("permit_symlinks(false)", mfs.symlinks_permitted() == false)

-- touch (path relative to write dir)
local touch_ok = mfs.touch("mfs_touch_test.tmp")
check("touch creates file", touch_ok == true)
check("touch file exists", mfs.exists("mfs_touch_test.tmp") == true)
mfs.remove("mfs_touch_test.tmp")

-- mkdir / rmdir
local mk_ok = mfs.mkdir("mfs_testdir")
check("mkdir", mk_ok == true)
check("dir exists after mkdir", mfs.is_dir("mfs_testdir") == true)
local rm_ok = mfs.remove("mfs_testdir")
check("remove dir", rm_ok == true)
check("dir gone after remove", not mfs.exists("mfs_testdir"))

-- load_text / load / write_file / append_file
mfs.write_file("mfs_test.txt", "hello\n", 6)
check("write_file", mfs.exists("mfs_test.txt") == true)

local sz = 0
local txt = mfs.load_text("mfs_test.txt", sz)
eq(txt, "hello\n", "load_text")

mfs.append_file("mfs_test.txt", "world\n", 6)
local txt2 = mfs.load_text("mfs_test.txt")
eq(txt2, "hello\nworld\n", "append_file + load_text")

local bin = mfs.load("mfs_test.txt", 0)  -- 2nd arg ignored in Lua, use #result
check("load returns binary", bin ~= nil)
eq(#bin, 12, "load size")

mfs.remove("mfs_test.txt")

-- open_read / open_write / streaming
local f = mfs.open_write("mfs_stream_test.txt")
check("open_write", f ~= nil)
if f then
    local n = f:write("stream data\n")
    eq(n, 12, "file:write bytes")
    f:flush()
    f:close()
end

local f2 = mfs.open_read("mfs_stream_test.txt")
check("open_read", f2 ~= nil)
if f2 then
    local data = f2:read(100)
    eq(data, "stream data\n", "file:read")
    eq(f2:size(), 12, "file:size")
    eq(f2:eof(), true, "file:eof after read")
    f2:close()
end
mfs.remove("mfs_stream_test.txt")

-- open_append
mfs.write_file("mfs_append_test.txt", "line1\n", 6)
local fa = mfs.open_append("mfs_append_test.txt")
check("open_append", fa ~= nil)
if fa then
    local n = fa:write("line2\n")
    eq(n, 6, "open_append:write bytes")
    fa:close()
end
local appended = mfs.load_text("mfs_append_test.txt")
eq(appended, "line1\nline2\n", "open_append result")
mfs.remove("mfs_append_test.txt")

-- file:seek / file:tell
mfs.write_file("mfs_seek_test.txt", "0123456789", 10)
local fs = mfs.open_read("mfs_seek_test.txt")
check("file:tell at start", fs:tell() == 0)
fs:seek("set", 5)
eq(fs:tell(), 5, "file:tell after seek set 5")
local ch = fs:read(1)
eq(ch, "5", "file:read after seek")
fs:seek("cur", 2)
eq(fs:tell(), 8, "file:tell after seek cur 2")
ch = fs:read(1)
eq(ch, "8", "file:read after seek cur")
fs:seek("end", -2)
eq(fs:tell(), 8, "file:tell after seek end -2")
ch = fs:read(1)
eq(ch, "8", "file:read after seek end")
fs:close()
mfs.remove("mfs_seek_test.txt")

-- mount / unmount / get_mount_point
-- Create a temp dir in the write dir and mount it
mfs.mkdir("mfs_mount_src")
mfs.write_file("mfs_mount_src/inside.txt", "mounted!", 8)
-- Mount it at /mymount in the virtual FS
local mount_ok = mfs.mount("/tmp/mfs_mount_src", "/mymount", 1)
check("mount dir", mount_ok == true)
-- Now the file should be visible at /mymount/inside.txt
check("file visible through mount", mfs.exists("/mymount/inside.txt") == true)
local mount_txt = mfs.load_text("/mymount/inside.txt")
eq(mount_txt, "mounted!", "file content through mount")
-- get_mount_point
local mp = mfs.get_mount_point("/tmp/mfs_mount_src")
check("get_mount_point returns string", type(mp) == "string")
-- unmount
check("unmount", mfs.unmount("/tmp/mfs_mount_src") == true)
check("file gone after unmount", not mfs.exists("/mymount/inside.txt"))
mfs.remove("mfs_mount_src/inside.txt")
mfs.remove("mfs_mount_src")

-- list / list_ex (order is filesystem-dependent, so check by content)
mfs.mkdir("mfs_listdir")
mfs.write_file("mfs_listdir/a.txt", "a", 1)
mfs.write_file("mfs_listdir/b.txt", "bb", 2)
mfs.mkdir("mfs_listdir/subdir")

local names = mfs.list("mfs_listdir")
check("list returns table", type(names) == "table")
eq(#names, 3, "list has 3 entries")
contains(names, "a.txt", "list contains a.txt")
contains(names, "b.txt", "list contains b.txt")
contains(names, "subdir", "list contains subdir")

local entries = mfs.list_ex("mfs_listdir")
check("list_ex returns table", type(entries) == "table")
eq(#entries, 3, "list_ex has 3 entries")
eq(find_ex(entries, "a.txt", "name"), "a.txt", "list_ex has a.txt")
eq(find_ex(entries, "a.txt", "is_file"), true, "a.txt is_file")
eq(find_ex(entries, "subdir", "is_dir"), true, "subdir is_dir")

-- get_search_path
local sp = mfs.get_search_path()
check("get_search_path returns table", type(sp) == "table")
print("  search path: " .. table.concat(sp, ", "))

-- get_real_dir
local rd = mfs.get_real_dir("mfs_listdir/a.txt")
check("get_real_dir returns string", rd ~= nil and type(rd) == "string")
print("  get_real_dir(a.txt) = " .. (rd or "nil"))

mfs.remove("mfs_listdir/a.txt")
mfs.remove("mfs_listdir/b.txt")
mfs.remove("mfs_listdir/subdir")
mfs.remove("mfs_listdir")

print("\n=== LFS-compatible high-level API ===")

-- currentdir / chdir
eq(mfs.currentdir(), "/", "currentdir defaults to /")
mfs.chdir("/")
eq(mfs.currentdir(), "/", "currentdir after chdir /")

-- attributes (path relative to write dir)
mfs.write_file("mfs_attr_test.txt", "data", 4)
local attr = mfs.attributes("mfs_attr_test.txt")
check("attributes returns table", type(attr) == "table")
eq(attr.mode, "file", "attributes mode = file")
eq(attr.size, 4, "attributes size = 4")
check("attributes modification is number", type(attr.modification) == "number")
check("attributes access is number", type(attr.access) == "number")
check("attributes change is number", type(attr.change) == "number")
eq(attr.permissions, "rw-rw-rw-", "attributes permissions")
eq(attr.nlink, 1, "attributes nlink")
eq(attr.dev, 0, "attributes dev")

-- attributes with field
eq(mfs.attributes("mfs_attr_test.txt", "mode"), "file", "attributes field mode")

-- attributes with bad field
local ok_attr, err_attr = pcall(function() mfs.attributes("mfs_attr_test.txt", "bogus") end)
check("attributes bad field errors", not ok_attr)

-- symlinkattributes
local sattr = mfs.symlinkattributes("mfs_attr_test.txt")
check("symlinkattributes returns table", type(sattr) == "table")
eq(sattr.mode, "file", "symlinkattributes mode")

local sattr_target = mfs.symlinkattributes("mfs_attr_test.txt", "target")
check("symlinkattributes target returns nil", sattr_target == nil)

mfs.remove("mfs_attr_test.txt")

-- dir iterator
mfs.mkdir("mfs_iterdir")
mfs.write_file("mfs_iterdir/x.txt", "", 0)
mfs.write_file("mfs_iterdir/y.txt", "", 0)

local items = {}
for name in mfs.dir("mfs_iterdir") do
    items[#items + 1] = name
end
eq(#items, 2, "dir iterator returns 2 entries")
contains(items, "x.txt", "dir iterator has x.txt")
contains(items, "y.txt", "dir iterator has y.txt")

mfs.remove("mfs_iterdir/x.txt")
mfs.remove("mfs_iterdir/y.txt")
mfs.remove("mfs_iterdir")

-- rmdir (alias for remove)
mfs.mkdir("mfs_rmdir_test")
check("rmdir succeeds", mfs.rmdir("mfs_rmdir_test") == true)
check("rmdir gone", not mfs.exists("mfs_rmdir_test"))

-- lock_dir / unlock_dir
mfs.mkdir("mfs_lockdir")
local lock = mfs.lock_dir("mfs_lockdir")
check("lock_dir returns table", type(lock) == "table")
check("lock_dir creates lockfile", mfs.exists("mfs_lockdir/lockfile.lfs") == true)

-- Second lock should fail
local lock2, lerr = mfs.lock_dir("mfs_lockdir")
check("double lock_dir fails", lock2 == nil)
eq(lerr, "directory is locked", "double lock_dir message")

-- Unlock
check("unlock_dir", mfs.unlock_dir(lock) == true)
check("lockfile removed", not mfs.exists("mfs_lockdir/lockfile.lfs"))

-- Re-lock with stale
local lock3 = mfs.lock_dir("mfs_lockdir", 0) -- 0 sec staleness
check("lock_dir after unlock", type(lock3) == "table")
mfs.unlock_dir(lock3)
mfs.remove("mfs_lockdir")

-- lock_dir with bad lock
local ul_ok, ul_err = mfs.unlock_dir({})
check("unlock_dir bad lock fails", ul_ok == nil)
eq(ul_err, "invalid lock", "unlock_dir bad lock message")

-- lock / unlock on file handles
mfs.write_file("mfs_lock_test.txt", "hello world", 11)
local lf = mfs.open_read("mfs_lock_test.txt")

-- Read lock succeeds
check("lock read region", mfs.lock(lf, "r", 0, 5) == true)

-- Read locks don't conflict with each other
check("lock read overlapping ok", mfs.lock(lf, "r", 3, 3) == true)

-- Write lock conflicts with existing read lock on same region
local wl_ok, wl_err = mfs.lock(lf, "w", 3, 2)
check("lock write conflicts with read", wl_ok == nil)

-- Unlock both read locks
check("unlock read lock 1", mfs.unlock(lf, 0, 5) == true)
check("unlock read lock 2", mfs.unlock(lf, 3, 3) == true)

-- Now write lock should work (all read locks on that region cleared)
check("lock write after unlock", mfs.lock(lf, "w", 0, 5) == true)
check("unlock write", mfs.unlock(lf, 0, 5) == true)

lf:close()
mfs.remove("mfs_lock_test.txt")

-- setmode stub
local sm_ok, sm_err = mfs.setmode(lf, "binary")
check("setmode returns error", sm_ok == nil)

-- link stub
local ln_ok, ln_err = mfs.link("a", "b")
check("link returns error", ln_ok == nil)

-- _VERSION / _DESCRIPTION
check("_VERSION", type(mfs._VERSION) == "string")
check("_DESCRIPTION", type(mfs._DESCRIPTION) == "string")

print(string.format("\n=== Results: %d passed, %d failed ===", passed, failed))
os.exit(failed > 0 and 1 or 0)
