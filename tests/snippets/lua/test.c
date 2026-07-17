#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>

int main(void) {
    lua_State *L = luaL_newstate();
    if (!L) return 1;
    printf("lua %s\n", LUA_VERSION);
    lua_close(L);
    return 0;
}
