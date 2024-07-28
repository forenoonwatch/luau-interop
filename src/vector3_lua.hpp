#pragma once

#include <vector3.hpp>

#include "script_fwd.hpp"

struct lua_State;

template <>
struct LuaTypeGetter<Vector3> {
	const Vector3* operator()(lua_State* L, int idx) {
		return reinterpret_cast<const Vector3*>(lua_tovector(L, idx));
	}
};

void vector3_lua_load(lua_State* L);
