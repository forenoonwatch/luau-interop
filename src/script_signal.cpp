#include "script_env.hpp"
#include "script_common.hpp"

#include <lualib.h>

#include <cstring>
#include <cstdio>

struct ScriptConnection {
	ScriptSignal* signal;
};

int script_signal_lua_namecall(lua_State* L);

int script_connection_lua_index(lua_State* L);
int script_connection_lua_namecall(lua_State* L);

static int script_signal_once_wrapper(lua_State* L);

static void push_signal_table(lua_State* L, ScriptSignal* key);

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal* signal);

// Public Functions

ScriptSignal* script_signal_create(lua_State* L) {
	auto* s = reinterpret_cast<ScriptSignal*>(lua_newuserdatatagged(L, 0, LuaTypeTraits<ScriptSignal>::TAG));

	if (luaL_newmetatable(L, "ScriptSignal")) {
		lua_pushstring(L, "ScriptSignal");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, script_signal_lua_namecall, "script_signal_lua_namecall");
		lua_setfield(L, -2, "__namecall");

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, s);
	lua_createtable(L, 0, 0);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return s;
}

void script_signal_destroy(lua_State* L, ScriptSignal* signal) {
	lua_pushlightuserdata(L, signal);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

void script_signal_fire(lua_State* L, ScriptSignal* signal, int argCount) {
	auto* env = ScriptEnvironment::get(L);
	auto top = lua_gettop(L);

	push_signal_table(L, signal);

	lua_pushnil(L);

	while (lua_next(L, -2) != 0) {
		lua_State* T = lua_newthread(L);
		lua_pushvalue(L, -2); // Copy the function to the top of the stack in L

		// Copy the parameters onto the top of the stack
		for (int i = top - argCount + 1; i <= top; ++i) {
			lua_pushvalue(L, i);
		}

		// Move the parameters and the function into T's stack
		lua_xmove(L, T, argCount + 1);

		// Resume T
		lua_resume(T, L, argCount);

		lua_pop(L, 2);
	}

	lua_pop(L, 1);

	env->unpark(signal, L, argCount);

	lua_pop(L, argCount);
}

// Static Functions

// ScriptSignal

int script_signal_connect(lua_State* L) {
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		luaL_error(L, "Invalid number of arguments: %d", nargs);
		return 0;
	}

	auto* s = lua_get<ScriptSignal>(L, 1);

	if (!lua_isfunction(L, 2)) {
		luaL_typeerrorL(L, 2, "function");
		return 0;
	}

	push_signal_table(L, s);

	auto* conn = script_connection_create(L, s);

	// connections[signal] = args[2] (function)
	lua_pushlightuserdata(L, conn);
	lua_pushvalue(L, 2);
	lua_rawset(L, -4);

	return 1;
}

int script_signal_once(lua_State* L) {
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		luaL_error(L, "Invalid number of arguments: %d", nargs);
		return 0;
	}

	auto* s = lua_get<ScriptSignal>(L, 1);

	if (!lua_isfunction(L, 2)) {
		luaL_typeerrorL(L, 2, "function");
		return 0;
	}

	push_signal_table(L, s);

	auto* conn = script_connection_create(L, s);

	// connections[signal] = args[2] (function)
	lua_pushlightuserdata(L, conn);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_pushcclosure(L, script_signal_once_wrapper, "once_wrapper", 2);

	lua_rawset(L, -4);

	return 1;
}

int script_signal_wait(lua_State* L) {
	auto* env = ScriptEnvironment::get(L);
	auto* s = lua_get<ScriptSignal>(L, 1);
	return env->park(L, s);
}

static int script_signal_once_wrapper(lua_State* L) {
	auto argCount = lua_gettop(L);

	auto* conn = lua_get<ScriptConnection>(L, lua_upvalueindex(2));

	push_signal_table(L, conn->signal);
	lua_pushlightuserdata(L, conn);
	lua_pushnil(L);
	lua_rawset(L, -3);

	lua_pushvalue(L, lua_upvalueindex(1));

	for (int i = 1; i <= argCount; ++i) {
		lua_pushvalue(L, i);
	}

	lua_call(L, argCount, 0);
	return 0;
}

static void push_signal_table(lua_State* L, ScriptSignal* key) {
	lua_pushlightuserdata(L, key);
	lua_rawget(L, LUA_REGISTRYINDEX);
}

// ScriptConnection

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal* signal) {
	auto* con = reinterpret_cast<ScriptConnection*>(lua_newuserdatatagged(L, sizeof(ScriptConnection),
			LuaTypeTraits<ScriptConnection>::TAG));
	con->signal = signal;

	if (luaL_newmetatable(L, "ScriptConnection")) {
		lua_pushstring(L, "ScriptConnection");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, script_connection_lua_index, "script_connection_lua_index");
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, script_connection_lua_namecall, "script_connection_lua_namecall");
		lua_setfield(L, -2, "__namecall");
	}

	lua_setmetatable(L, -2);

	return con;
}

int script_connection_connected(lua_State* L) {
	auto* conn = lua_get<ScriptConnection>(L, 1);

	push_signal_table(L, conn->signal);
	lua_pushlightuserdata(L, conn);
	lua_rawget(L, -2);

	lua_pushboolean(L, !lua_isnoneornil(L, -1));
	return 1;
}

int script_connection_disconnect(lua_State* L) {
	auto* conn = lua_get<ScriptConnection>(L, 1);

	push_signal_table(L, conn->signal);

	lua_pushlightuserdata(L, conn);
	lua_pushnil(L);
	lua_rawset(L, -3);

	return 0;
}

