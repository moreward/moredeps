-- examples/mfs-lua/scripts/shim.lua
--
-- Sandbox setup: capture the original io/os/debug/load globals, then replace
-- them with PhysFS-backed (MFS) shims. This is intentionally loaded before
-- main.lua so the application runs in a sandboxed environment.
--
-- NOTE: This is a first version. Later it can be made configurable or hot-
-- swappable by the host application.

local mfs = require("mfs")

-- Capture originals before we replace them.
local io = io
local os = os
local debug = debug
local load = load
local package = package

-- ===== File handle wrapper ================================================

local file_mt = {}

local function check_open(f)
    if not f.__handle then
        error("attempt to use a closed file", 3)
    end
end

local file_methods = {}

function file_methods:read(...)
    check_open(self)
    local args = {...}
    if #args == 0 then args = {"*l"} end
    local results = {}

    for _, fmt in ipairs(args) do
        if fmt == "*a" then
            -- Read rest of file in chunks.
            local parts = {}
            while true do
                local chunk = self.__handle:read(4096)
                if not chunk or #chunk == 0 then break end
                table.insert(parts, chunk)
            end
            table.insert(results, table.concat(parts))

        elseif fmt == "*l" then
            local parts = {}
            local got = false
            while true do
                local ch = self.__handle:read(1)
                if not ch or #ch == 0 then break end
                got = true
                if ch == "\n" then break end
                table.insert(parts, ch)
            end
            table.insert(results, got and table.concat(parts) or nil)

        elseif fmt == "*n" then
            -- Skip whitespace, then read a number.
            local chars = {}
            while true do
                local ch = self.__handle:read(1)
                if not ch or #ch == 0 then break end
                if not ch:match("%s") then
                    table.insert(chars, ch)
                    break
                end
            end
            if #chars > 0 then
                while true do
                    local ch = self.__handle:read(1)
                    if not ch or #ch == 0 then break end
                    if ch:match("[%+%-%.%deE]") or ch:match("%d") then
                        table.insert(chars, ch)
                    else
                        -- Push back one byte if possible.
                        local pos = self.__handle:tell()
                        if pos and pos > 0 then
                            self.__handle:seek("set", pos - 1)
                        end
                        break
                    end
                end
            end
            table.insert(results, tonumber(table.concat(chars)))

        elseif type(fmt) == "number" then
            if fmt <= 0 then
                table.insert(results, "")
            else
                local data = self.__handle:read(fmt)
                table.insert(results, data or "")
            end
        end
    end

    return table.unpack(results)
end

function file_methods:write(...)
    check_open(self)
    for i = 1, select("#", ...) do
        local s = tostring(select(i, ...))
        if #s > 0 then
            local n = self.__handle:write(s)
            if not n or n ~= #s then
                return nil, mfs.last_error()
            end
        end
    end
    return self
end

function file_methods:close()
    if self.__handle then
        self.__handle:close()
        self.__handle = nil
    end
    return true
end

function file_methods:flush()
    check_open(self)
    return self.__handle:flush()
end

function file_methods:seek(whence, offset)
    check_open(self)
    return self.__handle:seek(whence or "cur", offset or 0)
end

function file_methods:tell()
    check_open(self)
    return self.__handle:tell()
end

function file_methods:lines()
    check_open(self)
    local f = self
    return function()
        return f:read("*l")
    end
end

function file_methods:eof()
    check_open(self)
    return self.__handle:eof()
end

file_mt.__index = file_methods
file_mt.__gc = function(self)
    if self.__handle then
        self.__handle:close()
        self.__handle = nil
    end
end

local function new_file(handle)
    local f = { __handle = handle }
    setmetatable(f, file_mt)
    return f
end

-- ===== io shim ============================================================

local io_shim = {}

function io_shim.open(path, mode)
    mode = mode or "r"
    local handle
    if mode == "r" or mode == "rb" then
        handle = mfs.open_read(path)
    elseif mode == "w" or mode == "wb" then
        handle = mfs.open_write(path)
    elseif mode == "a" or mode == "ab" then
        handle = mfs.open_append(path)
    else
        error("unsupported mode: " .. mode)
    end
    if not handle then
        return nil, mfs.last_error()
    end
    return new_file(handle)
end

function io_shim.close(file)
    if file then
        return file:close()
    end
    return true
end

function io_shim.lines(path, ...)
    local f, err = io_shim.open(path, "r")
    if not f then return nil, err end
    return f:lines()
end

function io_shim.type(obj)
    if type(obj) ~= "table" then return nil end
    if getmetatable(obj) == file_mt then
        return obj.__handle and "file" or "closed file"
    end
    return nil
end

-- stdin/stdout/stderr are not backed by PhysFS. To keep the sandbox tight,
-- we do not expose the OS-backed originals here. print() still works through
-- the C runtime, but io.read/io.write without a file argument will fail.

function io_shim.read(...)
    return nil, "io.read: stdin is not available in the sandbox"
end

function io_shim.write(...)
    return nil, "io.write: stdout is not available in the sandbox"
end

-- ===== os shim ============================================================

local os_shim = {
    time = os.time,
    date = os.date,
    clock = os.clock,
    difftime = os.difftime,
    getenv = os.getenv,
    -- Intentionally NOT provided: execute, exit, remove, rename, setlocale, tmpname.
}

-- ===== loadfile / dofile / safe load ======================================

function loadfile(path)
    local src, err = mfs.load_text(path)
    if not src then return nil, err end
    return load(src, "@" .. path, "t")
end

function dofile(path)
    local f, err = loadfile(path)
    if not f then error(err) end
    return f()
end

-- Restrict load() to text mode only (no bytecode).
local function safe_load(chunk, name, mode, env)
    return load(chunk, name, "t", env)
end

-- ===== package searcher ===================================================

local function module_to_path(name)
    return name:gsub("%.", "/") .. ".lua"
end

local function mfs_searcher(name)
    local path = module_to_path(name)
    local src, err = mfs.load_text(path)
    if not src then
        return "\n\tno MFS module '" .. path .. "'"
    end
    local f, err2 = load(src, "@" .. path, "t")
    if not f then
        error("error loading module '" .. name .. "' from '" .. path .. "':\n\t" .. err2)
    end
    return f, path
end

package.searchers = { mfs_searcher }
package.cpath = ""
package.path = ""

-- ===== Install globals ====================================================

_G.io = io_shim
_G.os = os_shim
_G.debug = { traceback = debug.traceback }
_G.loadfile = loadfile
_G.dofile = dofile
_G.load = safe_load

-- Remove anything that could escape the sandbox.
_G.loadstring = nil
_G.package.loadlib = nil

-- Mark the shim as loaded so main.lua can rely on the environment being set up.
return true
