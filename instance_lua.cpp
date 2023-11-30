#include "instance_lua.hpp"

#include "instance.hpp"

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <unordered_map>

using GetterFunction = void(*)(lua_State*, Instance*);
using SetterFunction = void(*)(lua_State*, Instance*);

// FIXME: Go away
static int g_refInstanceLookup;
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
	lua_newtable(L);
	g_refInstanceLookup = lua_ref(L, -1);
	lua_pop(L, 1);

	luaL_findtable(L, LUA_GLOBALSINDEX, "Instance", 0);
	lua_pushcfunction(L, instance_new, "instance_new");
	lua_setfield(L, -2, "new");
}

void instance_lua_push(lua_State* L, Instance& inst) {
}

void instance_lua_print_refs(lua_State* L) {
	puts("[SYSTEM] DEBUG PRINTING REFS");

	lua_getref(L, g_refInstanceLookup);

	lua_pushnil(L);

	while (lua_next(L, -2) != 0) {
		printf("[SYSTEM]\t%p = %p\n", lua_tolightuserdata(L, -2), lua_touserdata(L, -1));
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
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

	auto* hInst = reinterpret_cast<std::shared_ptr<Instance>*>(lua_newuserdatadtor(L,
				sizeof(std::shared_ptr<Instance>), instance_dtor));
	std::construct_at(hInst, Instance::create(classID));

	lua_getref(L, g_refInstanceLookup);
	lua_pushlightuserdata(L, hInst->get());
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

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
	}

	lua_setmetatable(L, -2);

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

	lua_getref(L, g_refInstanceLookup);

	int index = 1;

	(*hInst)->for_each_child([&](auto& child) {
		lua_pushlightuserdata(L, &child);
		lua_rawget(L, -2);
		lua_rawseti(L, -3, index);

		++index;
	});

	lua_pop(L, 1); // g_refInstanceLookup

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

