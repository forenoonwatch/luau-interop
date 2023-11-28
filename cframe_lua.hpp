#pragma once

#include <cframe.hpp>

struct lua_State;

constexpr const int LUA_TAG_CFRAME = 1;

void cframe_lua_load(lua_State* L);
void cframe_lua_push(lua_State* L, const CFrame&);
const CFrame* cframe_lua_get(lua_State* L, int idx);
const CFrame* cframe_lua_check(lua_State* L, int idx);

