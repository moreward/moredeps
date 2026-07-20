-- examples/mfs-lua/scripts/main.lua
-- A sample entry point loaded from the PhysFS archive.

print("hello from sandboxed PhysFS Lua!")

-- This module load will be resolved by the custom MFS searcher.
local helper = require("helper")
helper.greet()

-- These would raise "attempt to call a nil value" because we removed them:
-- os.execute("ls")
-- io.open("/etc/passwd")
