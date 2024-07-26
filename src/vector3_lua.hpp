#pragma once

#include <vector3.hpp>

struct lua_State;

void vector3_lua_load(lua_State* L);
void vector3_lua_push(lua_State* L, float x, float y, float z);
void vector3_lua_push(lua_State* L, const Vector3&);

int vector3_lua_index(lua_State* L);
int vector3_lua_namecall(lua_State* L);

