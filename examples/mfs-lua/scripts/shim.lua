-- examples/mfs-lua/scripts/shim.lua
--
-- Configurable sandbox setup.  Captures the original Lua globals before they
-- are replaced, then installs PhysFS-backed (MFS) shims and optional
-- controlled host features.
--
-- The host controls the sandbox by setting the global __MFS_CONFIG *before*
-- this file is loaded.  If the global is absent, every optional capability
-- defaults to OFF (safe mode) and no hooks are installed.

-- Capture the original modules before we touch anything.  These are the only
-- host-facing escape hatches we keep around.
local io_orig = io
local os_orig = os
local debug_orig = debug
local load_orig = load
local loadstring_orig = loadstring
local package_orig = package

local mfs_orig = require("mfs")

-- ---------------------------------------------------------------------------
-- Configuration
-- ---------------------------------------------------------------------------

local function bool(v, default)
    if v == nil then return default end
    return not not v
end

local raw_cfg = _G.__MFS_CONFIG or {}

local cfg = {
    capabilities = {},
    hooks = raw_cfg.hooks or {},
}

local caps = cfg.capabilities
local rcaps = raw_cfg.capabilities or {}

-- Default everything to OFF.  The host must explicitly enable capabilities.
caps.io_stdin    = bool(rcaps.io_stdin,    false)
caps.io_stdout   = bool(rcaps.io_stdout,   false)
caps.io_stderr   = bool(rcaps.io_stderr,   false)

caps.os_execute  = bool(rcaps.os_execute,  false)
caps.os_exit     = bool(rcaps.os_exit,     false)
caps.os_remove   = bool(rcaps.os_remove,   false)
caps.os_rename   = bool(rcaps.os_rename,   false)
caps.os_setlocale = bool(rcaps.os_setlocale, false)
caps.os_tmpname  = bool(rcaps.os_tmpname,  false)

caps.os_getenv   = bool(rcaps.os_getenv,   false)  -- host env info leak

caps.debug       = bool(rcaps.debug,       false)

caps.loadstring  = bool(rcaps.loadstring,  false)
caps.bytecode    = bool(rcaps.bytecode,    false)
caps.native_modules = bool(rcaps.native_modules, false)

-- ---------------------------------------------------------------------------
-- Hook helper
--
-- Each hook entry is { pre = fn, post = fn }.
--
-- pre(...)  -> (skip, ...)
--   If skip is true, the wrapped function is NOT called and the extra returns
--   become the result of the operation.
--
-- post(result_table, ...) -> new_result_table | nil
--   Called after the wrapped function.  May return a new table to replace the
--   result.  A nil return keeps the original result.
-- ---------------------------------------------------------------------------

local function wrap_with_hooks(name, fn)
    local hook = cfg.hooks[name]
    if not hook then return fn end

    return function(...)
        local pre = hook.pre
        local post = hook.post

        if pre then
            local ok, skip, a, b, c, d, e = pcall(pre, ...)
            if not ok then
                error("pre-hook error for " .. name .. ": " .. tostring(skip), 2)
            end
            if skip then
                return a, b, c, d, e
            end
        end

        local result = { fn(...) }

        if post then
            local ok, new_result = pcall(post, result, ...)
            if not ok then
                error("post-hook error for " .. name .. ": " .. tostring(new_result), 2)
            end
            if new_result ~= nil then
                result = new_result
            end
        end

        return table.unpack(result)
    end
end

-- ---------------------------------------------------------------------------
-- MFS module wrappers (low-level hooks + capability passthrough)
-- ---------------------------------------------------------------------------

local mfs = {}

for _, name in ipairs({
    "load_text", "load", "write_file", "append_file",
    "exists", "last_error",
    "open_read", "open_write", "open_append",
    "stat", "is_dir", "is_file", "is_symlink",
    "list", "list_ex", "mkdir", "remove", "touch",
    "mount", "unmount", "get_real_dir", "get_mount_point",
    "get_search_path", "get_base_dir", "get_write_dir",
    "set_write_dir", "get_user_dir", "get_pref_dir",
    "get_dir_separator", "permit_symlinks",
    "symlinks_permitted", "set_root",
    -- LFS-compatible high-level wrappers (implemented in Lua on the module):
    "attributes", "symlinkattributes", "currentdir", "chdir",
    "dir", "rmdir",
    -- Stubs (return error messages in PhysFS sandbox):
    "link", "setmode", "lock", "unlock", "lock_dir", "unlock_dir",
}) do
    mfs[name] = wrap_with_hooks("mfs_" .. name, mfs_orig[name])
end

-- File handle methods are wrapped individually so each operation can be
-- observed/blocked from the host.
local FILE_METHODS = {
    "read", "write", "close", "flush", "seek", "tell", "eof", "size"
}

-- We need to intercept construction of mfs file handles so the method
-- table can be installed.  The C module returns a userdata with a metatable;
-- we wrap those constructors and return a tiny Lua proxy that forwards
-- method calls to the wrapped methods.
local function wrap_file_handle(raw)
    if type(raw) ~= "userdata" then return raw end
    local proxy = { __raw = raw }
    for _, method in ipairs(FILE_METHODS) do
        local raw_method = raw[method]
        if raw_method then
            proxy[method] = function(self, ...)
                return wrap_with_hooks("file_" .. method, raw_method)(raw, ...)
            end
        end
    end
    -- Forward __tostring and __gc if needed; keep the proxy table simple.
    return proxy
end

for _, name in ipairs({ "open_read", "open_write", "open_append" }) do
    local raw_fn = mfs[name]
    mfs[name] = function(...)
        local ok, a, b = raw_fn(...)
        if not ok then return ok, a end
        return wrap_file_handle(ok), a
    end
end

-- Replace the global mfs module with the hooked version.
package.loaded.mfs = mfs
_G.mfs = mfs

-- ---------------------------------------------------------------------------
-- File handle wrapper for the io shim
-- ---------------------------------------------------------------------------

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
    local f = { __handle = wrap_file_handle(handle) }
    setmetatable(f, file_mt)
    return f
end

-- ---------------------------------------------------------------------------
-- io shim
-- ---------------------------------------------------------------------------

local io_shim = {}

function io_shim.open(path, mode)
    return wrap_with_hooks("io_open", function(p, m)
        m = m or "r"
        local handle
        if m == "r" or m == "rb" then
            handle = mfs.open_read(p)
        elseif m == "w" or m == "wb" then
            handle = mfs.open_write(p)
        elseif m == "a" or m == "ab" then
            handle = mfs.open_append(p)
        else
            error("unsupported mode: " .. m)
        end
        if not handle then
            return nil, mfs.last_error()
        end
        return new_file(handle)
    end)(path, mode)
end

function io_shim.close(file)
    return wrap_with_hooks("io_close", function(f)
        if f then
            return f:close()
        end
        return true
    end)(file)
end

function io_shim.lines(path, ...)
    return wrap_with_hooks("io_lines", function(p)
        local f, err = io_shim.open(p, "r")
        if not f then return nil, err end
        return f:lines()
    end)(path)
end

function io_shim.type(obj)
    return wrap_with_hooks("io_type", function(o)
        if type(o) ~= "table" then return nil end
        if getmetatable(o) == file_mt then
            return o.__handle and "file" or "closed file"
        end
        return nil
    end)(obj)
end

-- stdin/stdout/stderr are controlled capabilities.  When enabled, the original
-- host-backed handles are exposed.  io.read/io.write without a file argument
-- map to stdin/stdout respectively, and are hookable.
if caps.io_stdin then
    io_shim.stdin = io_orig.stdin
end
if caps.io_stdout then
    io_shim.stdout = io_orig.stdout
end
if caps.io_stderr then
    io_shim.stderr = io_orig.stderr
end

function io_shim.read(...)
    if caps.io_stdin then
        return wrap_with_hooks("io_read", function(...)
            return io_shim.stdin:read(...)
        end)(...)
    end
    return nil, "io.read: stdin is not available in the sandbox"
end

function io_shim.write(...)
    if caps.io_stdout then
        return wrap_with_hooks("io_write", function(...)
            return io_shim.stdout:write(...)
        end)(...)
    end
    return nil, "io.write: stdout is not available in the sandbox"
end

-- Explicitly blocked io functions (not available in PhysFS sandbox).
function io_shim.popen(...)
    return nil, "io.popen is not available in the sandbox"
end
function io_shim.tmpfile(...)
    return nil, "io.tmpfile is not available in the sandbox"
end
function io_shim.input(...)
    return nil, "io.input is not available in the sandbox"
end
function io_shim.output(...)
    return nil, "io.output is not available in the sandbox"
end

-- Also override the default input/output file vars so we don't leak the
-- originals through io.stdout etc.  They're set above via caps.io_std*.  If
-- those caps are off, the fields are nil.

-- ---------------------------------------------------------------------------
-- os shim
-- ---------------------------------------------------------------------------

local os_shim = {
    time = os_orig.time,
    date = os_orig.date,
    clock = os_orig.clock,
    difftime = os_orig.difftime,
}

-- Safe passthrough wrappers (always allowed, but hookable).
for _, name in ipairs({"time", "date", "clock", "difftime"}) do
    local n = "os_" .. name
    local fn = os_shim[name]
    os_shim[name] = function(...) return wrap_with_hooks(n, fn)(...) end
end

-- Controlled capabilities.
local function controlled_os(name, capability, orig_fn)
    if caps[capability] then
        return function(...)
            return wrap_with_hooks(name, orig_fn)(...)
        end
    end
    return nil
end

os_shim.execute   = controlled_os("os_execute",   "os_execute",   os_orig.execute)
os_shim.exit      = controlled_os("os_exit",      "os_exit",      os_orig.exit)
os_shim.remove    = controlled_os("os_remove",    "os_remove",    os_orig.remove)
os_shim.rename    = controlled_os("os_rename",    "os_rename",    os_orig.rename)
os_shim.setlocale = controlled_os("os_setlocale", "os_setlocale", os_orig.setlocale)
os_shim.tmpname   = controlled_os("os_tmpname",   "os_tmpname",   os_orig.tmpname)
os_shim.getenv    = controlled_os("os_getenv",    "os_getenv",    os_orig.getenv)

-- ---------------------------------------------------------------------------
-- debug shim
-- ---------------------------------------------------------------------------

local debug_shim
if caps.debug then
    -- Install the original debug module, but wrap every function for hooks.
    debug_shim = {}
    for k, v in pairs(debug_orig) do
        if type(v) == "function" then
            debug_shim[k] = wrap_with_hooks("debug_" .. k, v)
        else
            debug_shim[k] = v
        end
    end
else
    debug_shim = { traceback = wrap_with_hooks("debug_traceback", debug_orig.traceback) }
end

-- ---------------------------------------------------------------------------
-- loadfile / dofile / safe load
-- ---------------------------------------------------------------------------

local function loadfile_impl(path)
    local src, err = mfs.load_text(path)
    if not src then return nil, err end
    local mode = caps.bytecode and "bt" or "t"
    return load_orig(src, "@" .. path, mode)
end

function loadfile(path)
    return wrap_with_hooks("loadfile", loadfile_impl)(path)
end

function dofile(path)
    return wrap_with_hooks("dofile", function(p)
        local f, err = loadfile(p)
        if not f then error(err) end
        return f()
    end)(path)
end

local function safe_load(chunk, name, mode, env)
    if caps.bytecode then
        return wrap_with_hooks("load", load_orig)(chunk, name, mode or "bt", env)
    else
        return wrap_with_hooks("load", load_orig)(chunk, name, "t", env)
    end
end

-- ---------------------------------------------------------------------------
-- package / require
-- ---------------------------------------------------------------------------

local function module_to_path(name)
    return name:gsub("%.", "/") .. ".lua"
end

local function mfs_searcher(name)
    return wrap_with_hooks("require", function(n)
        local path = module_to_path(n)
        local src, err = mfs.load_text(path)
        if not src then
            return "\n\tno MFS module '" .. path .. "'"
        end
        local mode = caps.bytecode and "bt" or "t"
        local f, err2 = load_orig(src, "@" .. path, mode)
        if not f then
            error("error loading module '" .. n .. "' from '" .. path .. ":\n\t" .. err2)
        end
        return f, path
    end)(name)
end

package.searchers = { mfs_searcher }
package.cpath = ""
package.path = ""

if caps.native_modules then
    -- Re-enable native module loading.  Keep the MFS searcher first, then add
    -- every standard searcher except the host-Lua-file searcher (which would
    -- escape the sandbox).  We identify it by its internal name.
    for i, searcher in ipairs(package_orig.searchers) do
        local info = debug_orig.getinfo(searcher, "n")
        if not info or info.name ~= "searcher_Lua" then
            table.insert(package.searchers, wrap_with_hooks("require_native", searcher))
        end
    end
    package.cpath = package_orig.cpath
end

-- Hookable loadlib (only usable if native_modules is on).
package.loadlib = caps.native_modules
    and wrap_with_hooks("package_loadlib", package_orig.loadlib)
    or nil

-- ---------------------------------------------------------------------------
-- Install globals
-- ---------------------------------------------------------------------------

_G.io = io_shim
_G.os = os_shim
_G.debug = debug_shim
_G.loadfile = loadfile
_G.dofile = dofile
_G.load = safe_load

if caps.loadstring then
    _G.loadstring = caps.bytecode
        and wrap_with_hooks("loadstring", loadstring_orig)
        or wrap_with_hooks("loadstring", function(chunk, name)
            return load_orig(chunk, name, "t")
        end)
else
    _G.loadstring = nil
end

-- Remove anything that could escape the sandbox if not explicitly enabled.
if not caps.native_modules then
    _G.package.loadlib = nil
end

-- CRITICAL: clear package.loaded entries that hold the originals, so a script
-- cannot escape via  package.loaded.io.open("/etc/passwd").
package.loaded.io = nil
package.loaded.os = nil
package.loaded.debug = nil
package.loaded.lfs = nil    -- just in case
package.loaded.mfs = mfs    -- our hooked version
package.loaded.package = package  -- our modified version

-- Clear preload table (could contain native module loaders).
if not caps.native_modules then
    package.preload = {}
end

-- If bytecode is disabled, neuter string.dump so bytecode can't be extracted
-- and fed to a potential future escape.
if not caps.bytecode then
    string.dump = function() error("string.dump is disabled in the sandbox", 2) end
end

-- print() writes through C stdout.  At minimum make it hookable.
local print_orig = print
_G.print = wrap_with_hooks("print", function(...)
    if caps.io_stdout then
        return print_orig(...)
    end
    return nil, "print: stdout is not available in the sandbox"
end)

-- Mark the shim as loaded so main.lua can rely on the environment.
return true
