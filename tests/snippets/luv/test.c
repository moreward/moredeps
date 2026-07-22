#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
int luaopen_luv(lua_State *L);
int main(void) {
    lua_State *L = luaL_newstate();
    if (!L) return 1;
    luaL_openlibs(L);
    luaL_requiref(L, "luv", luaopen_luv, 1);
    lua_pop(L, 1);
    lua_getglobal(L, "require");
    lua_pushstring(L, "luv");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) { lua_close(L); return 2; }
    int ok = lua_istable(L, -1);
    lua_close(L);
    return ok ? 0 : 3;
}
