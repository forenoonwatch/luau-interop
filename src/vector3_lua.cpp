#include "vector3_lua.hpp"

#include <lua.h>
#include <lualib.h>

// Public Functions

void vector3_lua_push(lua_State* L, float x, float y, float z) {
	lua_pushvector(L, x, y, z);

	if (luaL_newmetatable(L, "Vector3")) {
		lua_pushstring(L, "Vector3");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, vector3_lua_namecall, "vector3_lua_namecall");
		lua_setfield(L, -2, "__namecall");

		lua_pushcfunction(L, vector3_lua_index, "vector3_lua_index");
		lua_setfield(L, -2, "__index");

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);
}

void vector3_lua_push(lua_State* L, const Vector3& v) {
	vector3_lua_push(L, v.x, v.y, v.z);
}

const Vector3* vector3_lua_get(lua_State* L, int idx) {
	return reinterpret_cast<const Vector3*>(lua_tovector(L, idx));
}

const Vector3* vector3_lua_check(lua_State* L, int idx) {
	if (auto* result = vector3_lua_get(L, idx)) {
		return result;
	}

	luaL_typeerrorL(L, idx, "Vector3");
	return nullptr;
}

