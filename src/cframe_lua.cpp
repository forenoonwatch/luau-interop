#include "cframe_lua.hpp"
#include "script_common.hpp"

#include <cstring>
#include <cstdio>

// Static Functions

int cframe_lua_from_matrix(lua_State* L) {
	auto* pos = lua_check<Vector3>(L, 1);
	auto* vX = lua_check<Vector3>(L, 2);
	auto* vY = lua_check<Vector3>(L, 3);

	if (!pos || !vX || !vY) [[unlikely]] {
		return 0;
	}

	if (auto* vZ = lua_get<Vector3>(L, 4)) {
		lua_push<CFrame>(L, CFrame{*pos, *vX, *vY, *vZ});
	}
	else {
		auto z = glm::normalize(glm::cross(*vX, *vY));
		lua_push<CFrame>(L, CFrame{*pos, *vX, *vY, std::move(z)});
	}

	return 1;
}

int cframe_lua_tostring(lua_State* L) {
	auto& cf = *lua_get<CFrame>(L, 1);

	char buffer[128];
	auto len = std::snprintf(buffer, 128, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", cf[3][0], cf[3][1],
			cf[3][2], cf[0][0], cf[0][1], cf[0][2], cf[1][0], cf[1][1], cf[1][2], cf[2][0], cf[2][1], cf[2][2]);

	lua_pushlstring(L, buffer, len);
	return 1;
}

int cframe_lua_mul(lua_State* L) {
	auto* self = lua_check<CFrame>(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = lua_get<CFrame>(L, 2)) {
		lua_push<CFrame>(L, *self * *other);
		return 1;
	}
	else if (auto* other = lua_get<Vector3>(L, 2)) {
		lua_push<Vector3>(L, *self * *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "CFrame");
	return 0;
}

int cframe_lua_add(lua_State* L) {
	auto* self = lua_check<CFrame>(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = lua_get<Vector3>(L, 2)) {
		lua_push<CFrame>(L, *self + *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "Vector3");
	return 0;
}

int cframe_lua_sub(lua_State* L) {
	auto* self = lua_check<CFrame>(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = lua_get<Vector3>(L, 2)) {
		lua_push<CFrame>(L, *self - *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "Vector3");
	return 0;
}

int cframe_lua_get_components(lua_State* L) {
	auto* cf = reinterpret_cast<const float*>(lua_touserdatatagged(L, 1, LuaTypeTraits<CFrame>::TAG));

	if (!cf) [[unlikely]] {
		luaL_typeerrorL(L, 1, "CFrame");
		return 0;
	}

	for (int i = 0; i < 12; ++i) {
		lua_pushnumber(L, cf[i]);
	}

	return 12;
}

