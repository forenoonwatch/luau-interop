#pragma once

struct lua_State;

struct ScriptConnection;
struct ScriptSignal;

ScriptSignal* script_signal_create(lua_State* L);
void script_signal_destroy(lua_State* L, ScriptSignal*);
void script_signal_fire(lua_State* L, ScriptSignal*, int argCount);

int script_signal_connect(lua_State* L);
int script_signal_once(lua_State* L);
int script_signal_wait(lua_State* L);

int script_connection_disconnect(lua_State* L);
int script_connection_connected(lua_State* L);

