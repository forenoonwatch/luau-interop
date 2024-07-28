#pragma once

#include <script_fwd.hpp>

struct ScriptSignal;

struct ScriptConnection {
	ScriptSignal* signal;
};

template <>
struct LuaPusher<ScriptSignal> {
	ScriptSignal* operator()(lua_State* L);
};

void script_signal_destroy(lua_State* L, ScriptSignal*);
void script_signal_fire(lua_State* L, ScriptSignal*, int argCount);

int script_signal_connect(lua_State* L);
int script_signal_once(lua_State* L);
int script_signal_wait(lua_State* L);

int script_connection_disconnect(lua_State* L);
int script_connection_connected(lua_State* L);

