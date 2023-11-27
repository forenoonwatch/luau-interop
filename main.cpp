#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <lua.h>
#include <luacode.h>
#include <lualib.h>

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>

#include <instance.hpp>
#include <vector3_lua.hpp>

void launch_script(lua_State* L, const char* chunkName, const char* data, size_t size) {
	lua_State* T = lua_newthread(L);
	luau_load(T, chunkName, data, size, 0);
	luaL_sandboxthread(T);

	if (auto res = lua_resume(T, L, 0); res != LUA_OK) {
		puts(lua_tostring(T, -1));
	}
}

void load_script(lua_State* L, const char* fileName) {
	FILE* file = fopen(fileName, "rb");
	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char* fileData = reinterpret_cast<char*>(malloc(fileSize));
	fread(fileData, 1, fileSize, file);
	fclose(file);

	size_t bytecodeSize{};
	char* bytecode = luau_compile(fileData, fileSize, nullptr, &bytecodeSize);
	launch_script(L, fileName, bytecode, bytecodeSize);

	free(fileData);
}

void cb_interrupt(lua_State* L, int gc) {
	//printf("Got interrupt, gc = %d, lua_clock = %.2f\n", gc, lua_clock());
}

struct ScriptSignal {};

struct ScriptConnection {
	ScriptSignal* pSignal;
	bool connected;
};

class ScriptScheduler {
	public:
		int wait(lua_State* T, float waitTime);
		void update(lua_State* L, float deltaTime);
	private:
		struct ScheduledScript {
			lua_State* state;
			float timeToRun;
		};

		std::vector<ScheduledScript> m_jobs;
};

ScriptSignal* script_signal_create(lua_State* L);
void script_signal_fire(ScriptSignal* sig, lua_State* L);

int script_scheduler_wait(lua_State* L);

ScriptScheduler g_scriptScheduler{};

int g_refEventList;

void instance_dtor(void* pInst) {
	auto& inst = *reinterpret_cast<Instance*>(pInst);
	inst.~Instance();
}

int instance_tostring(lua_State* L) {
	auto* pInst = reinterpret_cast<Instance*>(luaL_checkudata(L, 1, "MT_Instance"));

	if (pInst) {
		lua_pushstring(L, pInst->get_name().c_str());
		return 1;
	}

	return 0;
}

int g_refInstanceLookup;

using GetterFunction = void(*)(lua_State*, Instance*);
using SetterFunction = void(*)(lua_State*, Instance*);

void instance_get_name(lua_State* L, Instance* self) {
	lua_pushstring(L, self->get_name().c_str());
}

void instance_get_class_name(lua_State* L, Instance* self) {
	switch (self->get_class_id()) {
		case InstanceClass::BASE_PART:
			lua_pushstring(L, "BasePart");
			break;
		case InstanceClass::PART:
			lua_pushstring(L, "Part");
			break;
		case InstanceClass::WEDGE_PART:
			lua_pushstring(L, "WedgePart");
			break;
		default:
			lua_pushstring(L, "Instance");
	}
}

void instance_set_name(lua_State* L, Instance* self) {
	const char* val = luaL_checkstring(L, 3);

	if (!val) {
		return;
	}

	self->set_name(val);
}

void instance_set_parent(lua_State* L, Instance* self) {
	auto* pChild = reinterpret_cast<Instance*>(luaL_checkudata(L, 3, "MT_Instance"));

	if (!pChild) {
		return;
	}

	self->set_parent(pChild);
}

int instance_get_children(lua_State* L) {
	auto* pInst = reinterpret_cast<Instance*>(luaL_checkudata(L, 1, "MT_Instance"));

	lua_newtable(L);

	lua_getref(L, g_refInstanceLookup);

	int index = 1;

	pInst->for_each_child([&](auto& child) {
		lua_pushlightuserdata(L, &child);
		lua_rawget(L, -2);
		lua_rawseti(L, -3, index);

		++index;
	});

	lua_pop(L, 1); // g_refInstanceLookup

	return 1;
}

std::unordered_map<std::string, GetterFunction> g_instanceGetters{};
std::unordered_map<std::string, SetterFunction> g_instanceSetters{};
std::unordered_map<std::string, lua_CFunction> g_instanceMethods{};

void instance_init_getter_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceGetters.emplace(std::make_pair(std::string("Name"), instance_get_name));
		g_instanceGetters.emplace(std::make_pair(std::string("ClassName"),
				instance_get_class_name));
	}
}

void instance_init_setter_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceSetters.emplace(std::make_pair(std::string("Name"), instance_set_name));
		g_instanceSetters.emplace(std::make_pair(std::string("Parent"), instance_set_parent));
	}
}

void instance_init_function_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceMethods.emplace(std::make_pair(std::string("GetChildren"),
					instance_get_children));
	}
}

int instance_index(lua_State* L) {
	instance_init_getter_list();
	instance_init_function_list();

	auto* pInst = reinterpret_cast<Instance*>(luaL_checkudata(L, 1, "MT_Instance"));
	auto* key = luaL_checkstring(L, 2);

	if (!(pInst && key)) {
		return 0;
	}

	if (auto it = g_instanceGetters.find(key); it != g_instanceGetters.end()) {
		it->second(L, pInst);
		return 1;
	}
	else if (auto it = g_instanceMethods.find(key); it != g_instanceMethods.end()) {
		lua_pushcfunction(L, it->second, key);
		return 1;
	}

	return 0;
}

int instance_newindex(lua_State* L) {
	instance_init_setter_list();

	auto* pInst = reinterpret_cast<Instance*>(luaL_checkudata(L, 1, "MT_Instance"));
	auto* key = luaL_checkstring(L, 2);

	if (!(pInst && key)) {
		return 0;
	}

	if (auto it = g_instanceSetters.find(key); it != g_instanceSetters.end()) {
		it->second(L, pInst);
	}

	return 0;
}

int instance_new(lua_State* L) {
	const char* className = luaL_checkstring(L, 1);

	if (!className) {
		return 0;
	}

	auto classID = InstanceClass::INSTANCE;

	if (strcmp(className, "BasePart") == 0) {
		classID = InstanceClass::BASE_PART;
	}
	else if (strcmp(className, "Part") == 0) {
		classID = InstanceClass::PART;
	}

	auto* inst = reinterpret_cast<Instance*>(lua_newuserdatadtor(L, sizeof(Instance), instance_dtor));
	new (inst) Instance(classID);

	lua_getref(L, g_refInstanceLookup);
	lua_pushlightuserdata(L, inst);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	if (luaL_newmetatable(L, "MT_Instance")) {
		lua_pushstring(L, "__tostring");
		lua_pushcfunction(L, instance_tostring, "instance_tostring");
		lua_rawset(L, -3);

		lua_pushstring(L, "__index");
		lua_pushcfunction(L, instance_index, "instance_index");
		lua_rawset(L, -3);

		lua_pushstring(L, "__newindex");
		lua_pushcfunction(L, instance_newindex, "instance_newindex");
		lua_rawset(L, -3);
	}

	lua_setmetatable(L, -2);

	return 1;
}

int main() {
	using namespace std::chrono;

	lua_State* L = luaL_newstate();
	//lua_callbacks(L)->interrupt = cb_interrupt;
	
	lua_newtable(L);
	g_refEventList = lua_ref(L, -1);
	lua_pop(L, 1);

	lua_newtable(L);
	g_refInstanceLookup = lua_ref(L, -1);
	lua_pop(L, 1);

	//lua_setglobal(L, "Callbacks");

	luaL_openlibs(L);

	lua_newtable(L);

	lua_pushstring(L, "new");
	lua_pushcfunction(L, instance_new, "instance_new");
	lua_rawset(L, -3);

	lua_setglobal(L, "Instance");

	lua_pushcfunction(L, script_scheduler_wait, "script_scheduler_wait");
	lua_setglobal(L, "wait");

	vector3_lua_load(L);

	luaL_sandbox(L);
	luaL_sandboxthread(L);

	auto* sig = script_signal_create(L);
	lua_setglobal(L, "event");

	load_script(L, "../test.lua");

	script_signal_fire(sig, L);

	puts("[SYSTEM] DEBUG PRINTING REFS");

	lua_getref(L, g_refInstanceLookup);

	lua_pushnil(L);

	while (lua_next(L, -2) != 0) {
		printf("[SYSTEM]\t%p = %p\n", lua_tolightuserdata(L, -2), lua_touserdata(L, -1));
		lua_pop(L, 1);
	}

	lua_pop(L, 1);

	auto start = high_resolution_clock::now();
	double lastTime = 0.0;

	for (;;) {
		auto currTime = duration_cast<duration<double>>(high_resolution_clock::now() - start).count();
		auto deltaTime = currTime - lastTime;
		lastTime = currTime;

		g_scriptScheduler.update(L, static_cast<float>(deltaTime));
	}

	lua_close(L);

	return 0;
}

int script_connection_disconnect(lua_State* L) {
	auto* conn = reinterpret_cast<ScriptConnection*>(luaL_checkudata(L, 1, "MT_ScriptConnection"));

	if (!conn->connected) {
		return 0;
	}

	conn->connected = false;

	//lua_getglobal(L, "Callbacks");
	lua_getref(L, g_refEventList);
	lua_pushlightuserdata(L, conn->pSignal);
	lua_rawget(L, -2);

	lua_pushvalue(L, 1);
	lua_pushnil(L);
	lua_rawset(L, -3);

	return 0;
}

int script_connection_index(lua_State* L) {
	auto* conn = reinterpret_cast<ScriptConnection*>(luaL_checkudata(L, 1, "MT_ScriptConnection"));
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

ScriptConnection* script_connection_create(lua_State* L, ScriptSignal* pSignal) {
	auto* conn = reinterpret_cast<ScriptConnection*>(lua_newuserdata(L, sizeof(ScriptConnection)));
	conn->pSignal = pSignal;
	conn->connected = true;

	if (luaL_newmetatable(L, "MT_ScriptConnection")) {
		lua_pushstring(L, "__index");
		lua_pushcfunction(L, script_connection_index, "script_connection_index");
		lua_rawset(L, -3);
	}

	lua_setmetatable(L, -2);

	return conn;
}

int script_signal_connect(lua_State* L) {
	int nargs = lua_gettop(L);

	if (nargs != 2) {
		// FIXME: figure out how to emit errors like the check* funcs
		puts("Number of args is not correct");
		return 0;
	}

	auto* sig = reinterpret_cast<ScriptSignal*>(luaL_checkudata(L, 1, "MT_ScriptSignal"));

	if (!lua_isfunction(L, 2)) {
		// FIXME: figure out how to emit errors like the check* funcs
		puts("Passed parameter is not a function");
		return 0;
	}

	//lua_getglobal(L, "Callbacks");
	lua_getref(L, g_refEventList);

	lua_pushlightuserdata(L, sig);
	lua_rawget(L, -2);

	script_connection_create(L, sig);

	lua_pushvalue(L, -1);
	lua_pushvalue(L, 2);
	lua_rawset(L, -4);

	return 1;
}

ScriptSignal* script_signal_create(lua_State* L) {
	auto* s = reinterpret_cast<ScriptSignal*>(lua_newuserdata(L, sizeof(ScriptSignal)));

	if (luaL_newmetatable(L, "MT_ScriptSignal")) {
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_rawset(L, -3);

		lua_pushstring(L, "Connect");
		lua_pushcfunction(L, script_signal_connect, "script_signal_connect");
		lua_rawset(L, -3);
	}

	lua_setmetatable(L, -2);

	//lua_getglobal(L, "Callbacks");
	lua_getref(L, g_refEventList);

	lua_pushlightuserdata(L, s);
	lua_newtable(L);
	lua_rawset(L, -3);

	lua_pop(L, 1);

	return s;
}

void script_signal_fire(ScriptSignal* sig, lua_State* L) {
	//lua_getglobal(L, "Callbacks");
	lua_getref(L, g_refEventList);

	lua_pushlightuserdata(L, sig);
	lua_rawget(L, -2);

	lua_pushnil(L);

	while (lua_next(L, -2) != 0) {
		lua_State* T = lua_newthread(L);
		lua_pushvalue(L, -2);
		lua_xmove(L, T, 1);
		lua_resume(T, L, 0);

		lua_pop(L, 2);
	}

	lua_pop(L, 1); // lua_getglobal
}

int ScriptScheduler::wait(lua_State* T, float waitTime) {
	m_jobs.push_back({T, waitTime});
	return lua_yield(T, 0);
}

void ScriptScheduler::update(lua_State* L, float deltaTime) {
	for (auto it = m_jobs.rbegin(), end = m_jobs.rend(); it != end; ++it) {
		it->timeToRun -= deltaTime;

		if (it->timeToRun <= 0.f) {
			lua_resume(it->state, L, 0);

			*it = std::move(m_jobs.back());
			m_jobs.pop_back();
		}
	}
}

int script_scheduler_wait(lua_State* L) {
	int nArgs = lua_gettop(L);
	float waitTime = 0.f;

	if (nArgs > 0) {
		waitTime = static_cast<float>(luaL_checknumber(L, 1));
	}

	return g_scriptScheduler.wait(L, waitTime);
}

