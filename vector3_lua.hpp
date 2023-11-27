#pragma once

struct lua_State;

void vector3_lua_load(lua_State* L);
void vector3_lua_push(lua_State* L, float x, float y, float z);

