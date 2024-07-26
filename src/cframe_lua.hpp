#pragma once

#include <cframe.hpp>

struct lua_State;

void cframe_lua_load(lua_State* L);
void cframe_lua_push(lua_State* L, const CFrame&);

int cframe_lua_get_components(lua_State* L);

