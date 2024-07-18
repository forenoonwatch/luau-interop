#include "script_signal.hpp"
#include "type_tags.hpp"
#include "script_env.hpp"

#include <lualib.h>

#include <cstring>
#include <cstdio>

namespace {

struct ScriptConnection {
	ScriptSignal signal;
};

}

static int script_signal_namecall(lua_State* L);
static int script_signal_connect(lua_State* L);
static int script_signal_once(lua_State* L);
static int script_signal_wait(lua_State* L);

static int script_signal_once_wrapper(lua_State* L);

static void push_signal_table(lua_State* L, ScriptSignal key);

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal signal);
static int script_connection_disconnect(lua_State* L);
static int script_connection_index(lua_State* L);
static int script_connection_namecall(lua_State* L);

// Public Functions

ScriptSignal script_signal_create(lua_State* L) {
	auto* s = reinterpret_cast<ScriptSignal>(lua_newuserdatatagged(L, 0, LUA_TAG_SCRIPT_SIGNAL));

	if (luaL_newmetatable(L, "ScriptSignal")) {
		lua_pushstring(L, "ScriptSignal");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, script_signal_namecall, "script_signal_namecall");
		lua_setfield(L, -2, "__namecall");

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, s);
	lua_createtable(L, 0, 0);
	lua_rawset(L, LUA_REGISTRYINDEX);

	//lua_pop(L, 1);

	return s;
}

void script_signal_destroy(lua_State* L, ScriptSignal signal) {
	lua_pushlightuserdata(L, signal);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

void script_signal_fire(lua_State* L, ScriptSignal signal, int argCount) {
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

static int script_signal_namecall(lua_State* L) {
	if (auto* name = lua_namecallatom(L, nullptr)) {
		if (strcmp(name, "Connect") == 0) {
			return script_signal_connect(L);
		}
		else if (strcmp(name, "Once") == 0) {
			return script_signal_once(L);
		}
		else if (strcmp(name, "Wait") == 0) {
			return script_signal_wait(L);
		}
	}

	luaL_error(L, "%s is not a valid method of ScriptSignal", luaL_checkstring(L, 1));
	return 0;
}

static int script_signal_connect(lua_State* L) {
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		luaL_error(L, "Invalid number of arguments: %d", nargs);
		return 0;
	}

	auto* s = reinterpret_cast<ScriptSignal>(lua_touserdatatagged(L, 1, LUA_TAG_SCRIPT_SIGNAL));

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

static int script_signal_once(lua_State* L) {
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		luaL_error(L, "Invalid number of arguments: %d", nargs);
		return 0;
	}

	auto* s = reinterpret_cast<ScriptSignal>(lua_touserdatatagged(L, 1, LUA_TAG_SCRIPT_SIGNAL));

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

static int script_signal_wait(lua_State* L) {
	auto* env = ScriptEnvironment::get(L);
	auto* s = reinterpret_cast<ScriptSignal>(lua_touserdatatagged(L, 1, LUA_TAG_SCRIPT_SIGNAL));
	return env->park(L, s);
}

static int script_signal_once_wrapper(lua_State* L) {
	auto argCount = lua_gettop(L);

	auto* conn = reinterpret_cast<ScriptConnection*>(lua_touserdatatagged(L, lua_upvalueindex(2),
			LUA_TAG_SCRIPT_CONNECTION));

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

static void push_signal_table(lua_State* L, ScriptSignal key) {
	lua_pushlightuserdata(L, key);
	lua_rawget(L, LUA_REGISTRYINDEX);
}

// ScriptConnection

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal signal) {
	auto* con = reinterpret_cast<ScriptConnection*>(lua_newuserdatatagged(L, sizeof(ScriptConnection),
			LUA_TAG_SCRIPT_CONNECTION));
	con->signal = signal;

	if (luaL_newmetatable(L, "ScriptConnection")) {
		lua_pushstring(L, "ScriptConnection");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, script_connection_index, "script_connection_index");
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, script_connection_namecall, "script_connection_namecall");
		lua_setfield(L, -2, "__namecall");
	}

	lua_setmetatable(L, -2);

	return con;
}

static int script_connection_index(lua_State* L) {
	auto* conn = reinterpret_cast<ScriptConnection*>(lua_touserdatatagged(L, 1, LUA_TAG_SCRIPT_CONNECTION));
	const char* k = luaL_checkstring(L, 2);

	if (!conn || !k) {
		return 0;
	}

	if (strcmp("Connected", k) == 0) {
		push_signal_table(L, conn->signal);
		lua_pushlightuserdata(L, conn);
		lua_rawget(L, -2);

		lua_pushboolean(L, !lua_isnoneornil(L, -1));
		return 1;
	}

	luaL_error(L, "%s is not a valid member of ScriptConnection", k);
	return 0;
}

static int script_connection_namecall(lua_State* L) {
	if (auto* name = lua_namecallatom(L, nullptr)) {
		if (strcmp(name, "Disconnect") == 0) {
			return script_connection_disconnect(L);
		}
	}

	luaL_error(L, "%s is not a valid method of ScriptConnection", luaL_checkstring(L, 1));
	return 0;
}

static int script_connection_disconnect(lua_State* L) {
	auto* conn = reinterpret_cast<ScriptConnection*>(lua_touserdatatagged(L, 1, LUA_TAG_SCRIPT_CONNECTION));

	push_signal_table(L, conn->signal);

	lua_pushlightuserdata(L, conn);
	lua_pushnil(L);
	lua_rawset(L, -3);

	return 0;
}

