#pragma once

struct lua_State;

struct ScriptSignal_T;
using ScriptSignal = ScriptSignal_T*;

ScriptSignal script_signal_create(lua_State* L);
void script_signal_destroy(lua_State* L, ScriptSignal);
void script_signal_fire(lua_State* L, ScriptSignal, int argCount);

