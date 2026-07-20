-- examples/mfs-lua/scripts/main.lua
-- A sample entry point loaded from the PhysFS archive.

print("hello from sandboxed PhysFS Lua!")

-- This module load will be resolved by the custom MFS searcher.
local helper = require("helper")
helper.greet()

-- io shim: read a file from the archive.
local mfs = require("mfs")
print("data.txt exists:", mfs.exists("data.txt"))

local f, ferr = io.open("data.txt", "r")
if f then
    print("io.open read:", f:read("*l"))
    print("io.open tell:", f:tell())
    print("io.open read all:", f:read("*a"))
    f:close()
else
    print("io.open failed:", ferr)
end

-- io shim: write and read back a file in the write directory.
local wf, werr = io.open("write_test.txt", "w")
if wf then
    wf:write("written from shim\n")
    wf:write("line 2\n")
    wf:close()

    local rf, rerr = io.open("write_test.txt", "r")
    if rf then
        print("io.open write/read:", rf:read("*a"))
        rf:close()
    else
        print("io.open read-back failed:", rerr)
    end
else
    print("io.open write failed:", werr)
end

-- mfs.list_ex returns rich directory entries.
local entries = mfs.list_ex("")
for i, e in ipairs(entries) do
    if e.is_file then
        print("list_ex file:", e.name, "size:", e.size)
    end
end

-- loadfile / dofile shims: load another script from the archive.
local ok, err = pcall(function()
    local f = loadfile("loaded.lua")
    if f then f() end
end)
if not ok then
    print("loadfile test skipped (no loaded.lua):", err)
end

-- os shim safe functions should still work.
print("os.time:", os.time() > 0)
print("os.clock:", type(os.clock()))

-- debug.traceback should still work.
print("debug.traceback:", type(debug.traceback))

-- These would raise "attempt to call a nil value" because we removed them:
-- os.execute("ls")
-- io.open("/etc/passwd", "r")
-- loadlib
-- loadstring
