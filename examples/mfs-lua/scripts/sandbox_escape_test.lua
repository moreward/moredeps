-- sandbox_escape_test.lua — verifies sandbox escape vectors are blocked
-- This is loaded by the mfs-lua binary AFTER shim.lua sets up the sandbox.

local function check(name, ok)
    if ok then
        print("  PASS: " .. name)
    else
        print("  FAIL: " .. name)
    end
end

-- 1. package.loaded.io must NOT expose the real host io
local pl_io = package.loaded.io
check("package.loaded.io is nil", pl_io == nil)

-- 2. package.loaded.os must NOT expose the real host os
local pl_os = package.loaded.os
check("package.loaded.os is nil", pl_os == nil)

-- 3. io.popen must be blocked
local ok, err = pcall(io.popen, "echo hi")
check("io.popen blocked", not ok)

-- 4. io.tmpfile must be blocked
ok, err = pcall(io.tmpfile)
check("io.tmpfile blocked", not ok)

-- 5. io.input must be blocked
ok, err = pcall(io.input, "foo")
check("io.input blocked", not ok)

-- 6. io.output must be blocked
ok, err = pcall(io.output, "foo")
check("io.output blocked", not ok)

-- 7. os.getenv must be nil (disabled by default)
check("os.getenv is nil", os.getenv == nil)

-- 8. string.dump must be blocked (bytecode disabled by default)
ok, err = pcall(string.dump, function() end)
check("string.dump blocked", not ok)

-- 9. Our mfs module must be accessible
check("mfs module available", type(mfs) == "table")

-- 10. io.open through mfs must work
local f = io.open("shim.lua", "r")
check("io.open via sandbox works", f ~= nil)
if f then f:close() end

print("\nSandbox escape test complete.")
