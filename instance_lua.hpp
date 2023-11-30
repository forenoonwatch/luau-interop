#pragma once

class Instance;
struct lua_State;

void instance_lua_load(lua_State* L);
void instance_lua_push(lua_State* L, Instance&);

