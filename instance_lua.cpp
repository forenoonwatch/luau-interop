#include "instance_lua.hpp"

#include "instance.hpp"

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <unordered_map>

using GetterFunction = void(*)(lua_State*, Instance*);
using SetterFunction = void(*)(lua_State*, Instance*);

static std::unordered_map<std::string, GetterFunction> g_instanceGetters{};
static std::unordered_map<std::string, SetterFunction> g_instanceSetters{};
static std::unordered_map<std::string, lua_CFunction> g_instanceMethods{};

static int instance_new(lua_State* L);

static int instance_index(lua_State* L);
static int instance_newindex(lua_State* L);
static int instance_tostring(lua_State* L);

static void instance_dtor(void* pInst);

static void instance_get_name(lua_State* L, Instance* self);
static void instance_get_class_name(lua_State* L, Instance* self);
static void instance_set_name(lua_State* L, Instance* self);
static void instance_set_parent(lua_State* L, Instance* self);

static void instance_init_getter_list();
static void instance_init_setter_list();
static void instance_init_function_list();

// Public Functions

void instance_lua_load(lua_State* L) {
	luaL_findtable(L, LUA_GLOBALSINDEX, "Instance", 0);
	lua_pushcfunction(L, instance_new, "instance_new");
	lua_setfield(L, -2, "new");
	lua_pop(L, 1);
}

void instance_lua_push(lua_State* L, Instance& inst) {
	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(lua_newuserdatadtor(L,
			sizeof(std::shared_ptr<Instance>), instance_dtor));
	std::construct_at(hInst, inst.shared_from_this());

	if (luaL_newmetatable(L, "Instance")) {
		lua_pushstring(L, "__tostring");
		lua_pushcfunction(L, instance_tostring, "instance_tostring");
		lua_rawset(L, -3);

		lua_pushstring(L, "__index");
		lua_pushcfunction(L, instance_index, "instance_index");
		lua_rawset(L, -3);

		lua_pushstring(L, "__newindex");
		lua_pushcfunction(L, instance_newindex, "instance_newindex");
		lua_rawset(L, -3);

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);
}

// Static Functions

static int instance_new(lua_State* L) {
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

	auto inst = Instance::create(classID);
	instance_lua_push(L, *inst);
	return 1;
}

static int instance_index(lua_State* L) {
	instance_init_getter_list();
	instance_init_function_list();

	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(luaL_checkudata(L, 1, "Instance"));
	auto* key = luaL_checkstring(L, 2);

	if (!(hInst && hInst->get() && key)) {
		return 0;
	}

	if (auto it = g_instanceGetters.find(key); it != g_instanceGetters.end()) {
		it->second(L, hInst->get());
		return 1;
	}
	else if (auto it = g_instanceMethods.find(key); it != g_instanceMethods.end()) {
		lua_pushcfunction(L, it->second, key);
		return 1;
	}

	return 0;
}

static int instance_newindex(lua_State* L) {
	instance_init_setter_list();

	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(luaL_checkudata(L, 1, "Instance"));
	auto* key = luaL_checkstring(L, 2);

	if (!(hInst && hInst->get() && key)) {
		return 0;
	}

	if (auto it = g_instanceSetters.find(key); it != g_instanceSetters.end()) {
		it->second(L, hInst->get());
	}

	return 0;
}

static int instance_tostring(lua_State* L) {
	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(luaL_checkudata(L, 1, "Instance"));

	if (hInst) {
		lua_pushstring(L, (*hInst)->get_name().c_str());
		return 1;
	}

	return 0;
}

static void instance_dtor(void* pInst) {
	auto& hInst = *reinterpret_cast<std::shared_ptr<Instance>*>(pInst);
	hInst.~shared_ptr();
}

static void instance_get_name(lua_State* L, Instance* self) {
	lua_pushstring(L, self->get_name().c_str());
}

static void instance_get_class_name(lua_State* L, Instance* self) {
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

static void instance_set_name(lua_State* L, Instance* self) {
	const char* val = luaL_checkstring(L, 3);

	if (!val) {
		return;
	}

	self->set_name(val);
}

static void instance_set_parent(lua_State* L, Instance* self) {
	auto* hChild = reinterpret_cast<std::shared_ptr<Instance>*>(luaL_checkudata(L, 3, "Instance"));

	if (!hChild) {
		return;
	}

	self->set_parent(hChild->get());
}

static int instance_get_children(lua_State* L) {
	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(luaL_checkudata(L, 1, "Instance"));

	lua_newtable(L);

	int index = 1;

	(*hInst)->for_each_child([&](auto& child) {
		instance_lua_push(L, child);
		lua_rawseti(L, -2, index);

		++index;
	});

	return 1;
}

static void instance_init_getter_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceGetters.emplace(std::make_pair(std::string("Name"), instance_get_name));
		g_instanceGetters.emplace(std::make_pair(std::string("ClassName"), instance_get_class_name));
	}
}

static void instance_init_setter_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceSetters.emplace(std::make_pair(std::string("Name"), instance_set_name));
		g_instanceSetters.emplace(std::make_pair(std::string("Parent"), instance_set_parent));
	}
}

static void instance_init_function_list() {
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		g_instanceMethods.emplace(std::make_pair(std::string("GetChildren"), instance_get_children));
	}
}

