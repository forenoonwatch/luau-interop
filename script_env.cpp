#include "script_env.hpp"

#include <cframe_lua.hpp>
#include <vector3_lua.hpp>

#include <lua.h>
#include <lualib.h>
#include <Luau/Compiler.h>

#include <cstdio>
#include <cstring>
#include <optional>

namespace {

struct ScriptConnection {
	ScriptSignal* pSignal;
	bool connected;
};

}

static constexpr const int LUA_TAG_SCRIPT_SIGNAL = 2;

// Global library functions
static int lua_require(lua_State* L);
static int lua_wait(lua_State* L);
static int lua_collectgarbage(lua_State* L);

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal* pSignal);

static int script_connection_disconnect(lua_State* L);
static int script_connection_index(lua_State* L);
static int script_connection_namecall(lua_State* L);

static int script_signal_connect(lua_State* L);

static void cb_interrupt(lua_State* L, int gc);

static std::optional<std::string> load_file(const char* fileName);

ScriptEnvironment* ScriptEnvironment::get(lua_State* L) {
	return reinterpret_cast<ScriptEnvironment*>(lua_getthreaddata(lua_mainthread(L)));
}

ScriptEnvironment::ScriptEnvironment()
		: m_L(luaL_newstate()) {
	//lua_callbacks(m_L)->interrupt = cb_interrupt;
	
	luaL_Reg funcs[] = {
		{"require", lua_require},
		{"wait", lua_wait},
		{"collectgarbage", lua_collectgarbage},
		{NULL, NULL},
	};

    lua_pushvalue(m_L, LUA_GLOBALSINDEX);
	luaL_register(m_L, NULL, funcs);
	lua_pop(m_L, 1);

	lua_newtable(m_L);
	m_refEventList = lua_ref(m_L, -1);
	lua_pop(m_L, 1);

	luaL_openlibs(m_L);

	vector3_lua_load(m_L);
	cframe_lua_load(m_L);

	luaL_sandbox(m_L);
	luaL_sandboxthread(m_L);

	lua_setthreaddata(m_L, this);
}

ScriptEnvironment::~ScriptEnvironment() {
	lua_close(m_L);
}

void ScriptEnvironment::update(float deltaTime) {
	for (auto it = m_jobs.rbegin(), end = m_jobs.rend(); it != end; ++it) {
		it->timeToRun -= deltaTime;

		if (it->timeToRun <= 0.f) {
			lua_resume(it->state, m_L, 0);

			*it = std::move(m_jobs.back());
			m_jobs.pop_back();
		}
	}
}

ScriptSignal* ScriptEnvironment::script_signal_create(lua_State* L) {
	auto* s = reinterpret_cast<ScriptSignal*>(lua_newuserdatatagged(L, 0, LUA_TAG_SCRIPT_SIGNAL));

	if (luaL_newmetatable(L, "ScriptSignal")) {
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_rawset(L, -3);

		lua_pushstring(L, "Connect");
		lua_pushcfunction(L, script_signal_connect, "script_signal_connect");
		lua_rawset(L, -3);
	}

	lua_setmetatable(L, -2);

	lua_getref(L, m_refEventList);

	lua_pushlightuserdata(L, s);
	lua_newtable(L);
	lua_rawset(L, -3);

	lua_pop(L, 1);

	return s;
}

void ScriptEnvironment::script_signal_fire(ScriptSignal* sig) {
	lua_getref(m_L, m_refEventList);

	lua_pushlightuserdata(m_L, sig);
	lua_rawget(m_L, -2);

	lua_pushnil(m_L);

	while (lua_next(m_L, -2) != 0) {
		lua_State* T = lua_newthread(m_L);
		lua_pushvalue(m_L, -2);
		lua_xmove(m_L, T, 1);
		lua_resume(T, m_L, 0);

		lua_pop(m_L, 2);
	}

	lua_pop(m_L, 1); // lua_getref
}

bool ScriptEnvironment::run_script_file(const char* fileName) {
	if (auto fileData = load_file(fileName)) {
		return run_script_source_code(fileName, *fileData);
	}

	printf("Failed to load script file %s\n", fileName);
	return false;
}

bool ScriptEnvironment::run_script_source_code(const char* chunkName, const std::string& fileData) {
	auto bytecode = Luau::compile(fileData);
	return run_script_bytecode(chunkName, bytecode);
}

bool ScriptEnvironment::run_script_bytecode(const char* chunkName, const std::string& bytecode) {
	lua_State* T = lua_newthread(m_L);
	luaL_sandboxthread(T);

	if (luau_load(T, chunkName, bytecode.data(), bytecode.size(), 0) != 0) {
		printf("Failed to compile chunk %s\n", chunkName);
		return false;
	}

	if (auto res = lua_resume(T, m_L, 0); res != LUA_OK) {
		printf("launch_script(%s): %s\n", chunkName, lua_tostring(T, -1));
		return false;
	}

	return true;
}

int ScriptEnvironment::defer(lua_State* T, float waitTime) {
	m_jobs.push_back({T, waitTime});
	return lua_yield(T, 0);
}

void ScriptEnvironment::get_ref_event_list(lua_State* L) {
	lua_getref(L, m_refEventList);
}

lua_State* ScriptEnvironment::get_state() {
	return m_L;
}

// Static Functions

static int finish_require(lua_State* L) {
	if (lua_isstring(L, -1)) {
		lua_error(L);
	}

	return 1;
}

static int lua_require(lua_State* L) {
	puts("Calling lua_require");
	std::string name{luaL_checkstring(L, 1)};
	std::string chunkName{"=" + name};

	luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);

	// return the module from the cache
	lua_getfield(L, -1, name.c_str());
	if (!lua_isnil(L, -1)) {
		// L stack: _MODULES result
		return finish_require(L);
	}

	lua_pop(L, 1);

	auto path = std::string("../") + name + ".lua";
	auto source = load_file(path.c_str());

	if (!source) {
		luaL_argerrorL(L, 1, ("error loading " + name).c_str());
	}

	// module needs to run in a new thread, isolated from the rest
	// note: we create ML on main thread so that it doesn't inherit environment of L
	lua_State* GL = lua_mainthread(L);
	lua_State* ML = lua_newthread(GL);
	lua_xmove(GL, L, 1);

	// new thread needs to have the globals sandboxed
	luaL_sandboxthread(ML);

	auto bytecode = Luau::compile(*source);
	if (luau_load(ML, chunkName.c_str(), bytecode.data(), bytecode.size(), 0) == 0) {
		int status = lua_resume(ML, L, 0);

		if (status == 0) {
			if (lua_gettop(ML) == 0) {
				lua_pushstring(ML, "module must return a value");
			}
			else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1)) {
				lua_pushstring(ML, "module must return a table or function");
			}
		}
		else if (status == LUA_YIELD) {
			lua_pushstring(ML, "module can not yield");
		}
		else if (!lua_isstring(ML, -1)) {
			lua_pushstring(ML, "unknown error while running module");
		}
	}

	// there's now a return value on top of ML; L stack: _MODULES ML
	lua_xmove(ML, L, 1);
	lua_pushvalue(L, -1);
	lua_setfield(L, -4, name.c_str());

	// L stack: _MODULES ML result
	return finish_require(L);
}

static int lua_wait(lua_State* L) {
	auto* env = ScriptEnvironment::get(L);

	int nArgs = lua_gettop(L);
	float waitTime = 0.f;

	if (nArgs > 0) {
		waitTime = static_cast<float>(luaL_checknumber(L, 1));
	}

	return env->defer(L, waitTime);
}

static int lua_collectgarbage(lua_State* L) {
	auto* option = luaL_optstring(L, 1, "collect");

	if (strcmp(option, "collect") == 0) {
		lua_gc(L, LUA_GCCOLLECT, 0);
		return 0;
	}
	else if (strcmp(option, "count") == 0) {
		lua_pushnumber(L, lua_gc(L, LUA_GCCOUNT, 0));
		return 1;
	}
	
	luaL_error(L, "collectgarbage must be called with 'count' or 'collect'");
	return 0;
}

static ScriptConnection* script_connection_create(lua_State* L, ScriptSignal* pSignal) {
	auto* conn = reinterpret_cast<ScriptConnection*>(lua_newuserdata(L, sizeof(ScriptConnection)));
	conn->pSignal = pSignal;
	conn->connected = true;

	if (luaL_newmetatable(L, "ScriptConnection")) {
		lua_pushstring(L, "__index");
		lua_pushcfunction(L, script_connection_index, "script_connection_index");
		lua_rawset(L, -3);
	}

	lua_setmetatable(L, -2);

	return conn;
}

static int script_connection_index(lua_State* L) {
	auto* conn = reinterpret_cast<ScriptConnection*>(luaL_checkudata(L, 1, "ScriptConnection"));
	const char* k = luaL_checkstring(L, 2);

	if (!conn || !k) {
		return 0;
	}

	if (strcmp("Connected", k) == 0) {
		lua_pushboolean(L, conn->connected);
		return 1;
	}
	else if (strcmp("Disconnect", k) == 0) {
		lua_pushcfunction(L, script_connection_disconnect, "script_connection_disconnect");
		return 1;
	}

	return 0;
}

static int script_connection_disconnect(lua_State* L) {
	auto* env = ScriptEnvironment::get(L);
	auto* conn = reinterpret_cast<ScriptConnection*>(luaL_checkudata(L, 1, "ScriptConnection"));

	if (!(conn && conn->connected)) {
		return 0;
	}

	conn->connected = false;

	env->get_ref_event_list(L);
	lua_pushlightuserdata(L, conn->pSignal);
	lua_rawget(L, -2);

	lua_pushvalue(L, 1);
	lua_pushnil(L);
	lua_rawset(L, -3);

	return 0;
}

static int script_signal_connect(lua_State* L) {
	auto* env = ScriptEnvironment::get(L);
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		luaL_error(L, "Invalid number of arguments: %d", nargs);
		return 0;
	}

	auto* sig = reinterpret_cast<ScriptSignal*>(luaL_checkudata(L, 1, "ScriptSignal"));

	if (!lua_isfunction(L, 2)) {
		luaL_typeerrorL(L, 2, "function");
		return 0;
	}

	env->get_ref_event_list(L);

	lua_pushlightuserdata(L, sig);
	lua_rawget(L, -2);

	script_connection_create(L, sig);

	lua_pushvalue(L, -1);
	lua_pushvalue(L, 2);
	lua_rawset(L, -4);

	return 1;
}

static void cb_interrupt(lua_State* L, int gc) {
	//printf("Got interrupt, gc = %d, lua_clock = %.2f\n", gc, lua_clock());
}

static std::optional<std::string> load_file(const char* fileName) {
	FILE* file = fopen(fileName, "rb");

	if (!file) {
		return std::nullopt;
	}

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	if (fileSize < 0) {
		fclose(file);
		return std::nullopt;
	}
	rewind(file);

	std::string result(fileSize, 0);
	size_t read = fread(result.data(), 1, fileSize, file);
	fclose(file);

	if (read != static_cast<size_t>(fileSize)) {
		return std::nullopt;
	}

	return result;
}

