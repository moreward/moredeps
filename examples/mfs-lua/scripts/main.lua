-- examples/mfs-lua/scripts/main.lua
-- A sample entry point loaded from the PhysFS archive.
-- This version assumes the host has enabled all controlled capabilities via
-- __MFS_CONFIG; if a capability is disabled, the corresponding test will fail
-- with "attempt to call a nil value" or a sandbox error, which is fine for demo.

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

-- Controlled stdout/stderr (host must enable io_stdout / io_stderr).
if io.stdout then
    io.stdout:write("io.stdout:write works\n")
end
if io.stderr then
    io.stderr:write("io.stderr:write works\n")
end
if io.read then
    -- Reading from stdin in this demo is not practical; just confirm it exists.
    print("io.read type:", type(io.read))
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

-- Controlled os capabilities (host must enable them).
if os.execute then
    local rc = os.execute("echo 'hello from os.execute'")
    print("os.execute rc:", rc)
end

if os.setlocale then
    print("os.setlocale:", type(os.setlocale))
end

if os.tmpname then
    print("os.tmpname:", type(os.tmpname))
end

-- Controlled debug capabilities (host must enable debug).
if debug.getinfo then
    local info = debug.getinfo(1)
    print("debug.getinfo source:", info and info.short_src or "nil")
end

-- Controlled loadstring (host must enable loadstring).
if loadstring then
    local f = loadstring("return 40 + 2")
    if f then
        print("loadstring result:", f())
    end
end

-- debug.traceback should still work.
print("debug.traceback:", type(debug.traceback))

-- These would raise "attempt to call a nil value" if the capability is off:
-- os.execute("ls")
-- io.open("/etc/passwd", "r")
-- loadlib
-- loadstring
