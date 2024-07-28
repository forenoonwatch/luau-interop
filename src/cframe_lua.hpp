#pragma once

#include <cframe.hpp>

struct lua_State;

void cframe_lua_load(lua_State* L);

int cframe_lua_from_matrix(lua_State* L);
int cframe_lua_get_components(lua_State* L);

int cframe_lua_tostring(lua_State* L);
int cframe_lua_mul(lua_State* L);
int cframe_lua_add(lua_State* L);
int cframe_lua_sub(lua_State* L);

