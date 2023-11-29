#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <lua.h>
#include <lualib.h>

#include <chrono>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>

#include <script_env.hpp>
#include <instance.hpp>

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

	ScriptEnvironment env;
	auto* L = env.get_state();

	lua_newtable(L);
	g_refInstanceLookup = lua_ref(L, -1);
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushstring(L, "new");
	lua_pushcfunction(L, instance_new, "instance_new");
	lua_rawset(L, -3);

	lua_setglobal(L, "Instance");

	auto* sig = env.script_signal_create();
	lua_setglobal(L, "event");

	env.run_script_file("../test.lua");
	env.script_signal_fire(sig);

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

		env.update(static_cast<float>(deltaTime));
	}

	return 0;
}

