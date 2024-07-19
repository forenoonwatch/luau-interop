#include "cframe_lua.hpp"
#include "type_tags.hpp"

#include <vector3_lua.hpp>

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <cstdio>

static int cframe_new(lua_State* L);
static int cframe_look_at(lua_State* L);
static int cframe_from_euler_angles_xyz(lua_State* L);
static int cframe_from_axis_angle(lua_State* L);
static int cframe_from_matrix(lua_State* L);

static int cframe_index(lua_State* L);
static int cframe_newindex(lua_State* L);
static int cframe_namecall(lua_State* L);
static int cframe_tostring(lua_State* L);
static int cframe_mul(lua_State* L);
static int cframe_add(lua_State* L);
static int cframe_sub(lua_State* L);

static int cframe_inverse(lua_State* L);
static int cframe_lerp(lua_State* L);
static int cframe_get_components(lua_State* L);

// Public Functions

void cframe_lua_load(lua_State* L) {
	luaL_findtable(L, LUA_GLOBALSINDEX, "CFrame", 0);

	lua_pushcfunction(L, cframe_new, "new");
	lua_setfield(L, -2, "new");

	lua_pushcfunction(L, cframe_look_at, "lookAt");
	lua_setfield(L, -2, "lookAt");

	lua_pushcfunction(L, cframe_from_euler_angles_xyz, "Angles");
	lua_setfield(L, -2, "Angles");

	lua_pushcfunction(L, cframe_from_euler_angles_xyz, "fromEulerAnglesXYZ");
	lua_setfield(L, -2, "fromEulerAnglesXYZ");

	lua_pushcfunction(L, cframe_from_euler_angles_xyz, "fromOrientation");
	lua_setfield(L, -2, "fromOrientation");

	lua_pushcfunction(L, cframe_from_axis_angle, "fromAxisAngle");
	lua_setfield(L, -2, "fromAxisAngle");

	lua_pushcfunction(L, cframe_from_matrix, "fromMatrix");
	lua_setfield(L, -2, "fromMatrix");

	lua_pop(L, 1);
}

void cframe_lua_push(lua_State* L, const CFrame& cf) {
	auto* pCF = reinterpret_cast<CFrame*>(lua_newuserdatatagged(L, sizeof(CFrame), LUA_TAG_CFRAME));
	*pCF = cf;

	if (luaL_newmetatable(L, "CFrame")) {
		lua_pushstring(L, "CFrame");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, cframe_index, "__index");
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, cframe_newindex, "__newindex");
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, cframe_namecall, "__namecall");
		lua_setfield(L, -2, "__namecall");

		lua_pushcfunction(L, cframe_tostring, "__tostring");
		lua_setfield(L, -2, "__tostring");

		lua_pushcfunction(L, cframe_mul, "__mul");
		lua_setfield(L, -2, "__mul");

		lua_pushcfunction(L, cframe_add, "__add");
		lua_setfield(L, -2, "__add");

		lua_pushcfunction(L, cframe_sub, "__sub");
		lua_setfield(L, -2, "__sub");

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);
}

const CFrame* cframe_lua_get(lua_State* L, int idx) {
	return reinterpret_cast<const CFrame*>(lua_touserdatatagged(L, idx, LUA_TAG_CFRAME));
}

const CFrame* cframe_lua_check(lua_State* L, int idx) {
	if (auto* result = cframe_lua_get(L, idx)) {
		return result;
	}

	luaL_typeerrorL(L, idx, "CFrame");
	return nullptr;
}

// Static Functions

static int cframe_new(lua_State* L) {
	auto argc = lua_gettop(L);

	switch (argc) {
		case 0:
			cframe_lua_push(L, CFrame{1.f});
			return 1;
		case 1:
			if (auto* pV = vector3_lua_check(L, 1)) {
				cframe_lua_push(L, CFrame{*pV});
				return 1;
			}
			break;
		case 2:
		{
			auto* pPos = vector3_lua_check(L, 1);
			auto* pLook = vector3_lua_check(L, 2);

			if (pPos && pLook) {
				cframe_lua_push(L, CFrame::look_at(*pPos, *pLook));
				return 1;
			}
		}
			break;
		case 3:
			cframe_lua_push(L, CFrame{static_cast<float>(lua_tonumber(L, 1)),
					static_cast<float>(lua_tonumber(L, 2)),
					static_cast<float>(lua_tonumber(L, 3))});
			return 1;
		case 7:
		{
			Vector3 pos{static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)),
					static_cast<float>(lua_tonumber(L, 3))};
			Quaternion rot(static_cast<float>(lua_tonumber(L, 7)), static_cast<float>(lua_tonumber(L, 4)),
					static_cast<float>(lua_tonumber(L, 5)), static_cast<float>(lua_tonumber(L, 6)));
			cframe_lua_push(L, CFrame{std::move(pos), std::move(rot)});
			return 1;
		}
		case 12:
			cframe_lua_push(L, CFrame{
				static_cast<float>(lua_tonumber(L, 1)),
				static_cast<float>(lua_tonumber(L, 2)),
				static_cast<float>(lua_tonumber(L, 3)),
				static_cast<float>(lua_tonumber(L, 4)),
				static_cast<float>(lua_tonumber(L, 5)),
				static_cast<float>(lua_tonumber(L, 6)),
				static_cast<float>(lua_tonumber(L, 7)),
				static_cast<float>(lua_tonumber(L, 8)),
				static_cast<float>(lua_tonumber(L, 9)),
				static_cast<float>(lua_tonumber(L, 10)),
				static_cast<float>(lua_tonumber(L, 11)),
				static_cast<float>(lua_tonumber(L, 12)),
			});
			return 1;
		default:
			luaL_error(L, "Invalid number of arguments: %d", argc);
	}

	return 0;
}

static int cframe_look_at(lua_State* L) {
	auto* at = vector3_lua_check(L, 1);
	auto* lookAt = vector3_lua_check(L, 2);

	if (!at || !lookAt) [[unlikely]] {
		return 0;
	}

	if (auto* pUp = vector3_lua_get(L, 3)) {
		cframe_lua_push(L, CFrame::look_at(*at, *lookAt, *pUp));
	}
	else {
		cframe_lua_push(L, CFrame::look_at(*at, *lookAt));
	}

	return 1;
}

static int cframe_from_euler_angles_xyz(lua_State* L) {
	auto rx = luaL_checknumber(L, 1);
	auto ry = luaL_checknumber(L, 2);
	auto rz = luaL_checknumber(L, 3);

	cframe_lua_push(L, CFrame::from_euler_angles_xyz(rx, ry, rz));
	return 1;
}

static int cframe_from_axis_angle(lua_State* L) {
	auto* axis = vector3_lua_check(L, 1);
	auto angle = static_cast<float>(luaL_checknumber(L, 2));

	if (!axis) [[unlikely]] {
		return 0;
	}

	cframe_lua_push(L, CFrame::from_axis_angle(*axis, angle));
	return 1;
}

static int cframe_from_matrix(lua_State* L) {
	auto* pos = vector3_lua_check(L, 1);
	auto* vX = vector3_lua_check(L, 2);
	auto* vY = vector3_lua_check(L, 3);

	if (!pos || !vX || !vY) [[unlikely]] {
		return 0;
	}

	if (auto* vZ = vector3_lua_get(L, 4)) {
		cframe_lua_push(L, CFrame{*pos, *vX, *vY, *vZ});
	}
	else {
		auto z = glm::normalize(glm::cross(*vX, *vY));
		cframe_lua_push(L, CFrame{*pos, *vX, *vY, std::move(z)});
	}

	return 1;
}

static int cframe_index(lua_State* L) {
	auto* pCF = cframe_lua_get(L, 1);
	const char* name = luaL_checkstring(L, 2);

	if (strcmp(name, "X") == 0) {
		lua_pushnumber(L, pCF->get_position().x);
		return 1;
	}
	else if (strcmp(name, "Y") == 0) {
		lua_pushnumber(L, pCF->get_position().y);
		return 1;
	}
	else if (strcmp(name, "Z") == 0) {
		lua_pushnumber(L, pCF->get_position().z);
		return 1;
	}
	else if (strcmp(name, "Position") == 0) {
		vector3_lua_push(L, pCF->get_position());
		return 1;
	}
	else if (strcmp(name, "Rotation") == 0) {
		cframe_lua_push(L, CFrame{Vector3{}, (*pCF)[0], (*pCF)[1], (*pCF)[2]});
		return 1;
	}
	else if (strcmp(name, "LookVector") == 0) {
		vector3_lua_push(L, pCF->look_vector());
		return 1;
	}
	else if (strcmp(name, "RightVector") == 0 || strcmp(name, "XVector") == 0) {
		vector3_lua_push(L, pCF->right_vector());
		return 1;
	}
	else if (strcmp(name, "UpVector") == 0 || strcmp(name, "YVector") == 0) {
		vector3_lua_push(L, pCF->up_vector());
		return 1;
	}
	else if (strcmp(name, "ZVector") == 0) {
		vector3_lua_push(L, pCF->z_vector());
		return 1;
	}

	luaL_error(L, "%s is not a valid member of CFrame", name);
	return 0;
}

static int cframe_newindex(lua_State* L) {
	luaL_error(L, "%s cannot be assigned to", luaL_checkstring(L, 2));
	return 0;
}

static int cframe_namecall(lua_State* L) {
	if (const char* name = lua_namecallatom(L, nullptr)) {
		if (strcmp(name, "Inverse") == 0) {
			return cframe_inverse(L);
		}
		else if (strcmp(name, "Lerp") == 0) {
			return cframe_lerp(L);
		}
		else if (strcmp(name, "GetComponents") == 0 || strcmp(name, "components") == 0) {
			return cframe_get_components(L);
		}
	}

	luaL_error(L, "%s is not a valid method of CFrame", luaL_checkstring(L, 1));
	return 0;
}

static int cframe_tostring(lua_State* L) {
	auto& cf = *cframe_lua_get(L, 1);

	char buffer[128];
	auto len = std::snprintf(buffer, 128, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f", cf[3][0], cf[3][1],
			cf[3][2], cf[0][0], cf[0][1], cf[0][2], cf[1][0], cf[1][1], cf[1][2], cf[2][0], cf[2][1], cf[2][2]);

	lua_pushlstring(L, buffer, len);
	return 1;
}

static int cframe_mul(lua_State* L) {
	auto* self = cframe_lua_check(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = cframe_lua_get(L, 2)) {
		cframe_lua_push(L, *self * *other);
		return 1;
	}
	else if (auto* other = vector3_lua_get(L, 2)) {
		vector3_lua_push(L, *self * *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "CFrame");
	return 0;
}

static int cframe_add(lua_State* L) {
	auto* self = cframe_lua_check(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = vector3_lua_get(L, 2)) {
		cframe_lua_push(L, *self + *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "Vector3");
	return 0;
}

static int cframe_sub(lua_State* L) {
	auto* self = cframe_lua_check(L, 1);

	if (!self) [[unlikely]] {
		return 0;
	}

	if (auto* other = vector3_lua_get(L, 2)) {
		cframe_lua_push(L, *self - *other);
		return 1;
	}

	luaL_typeerrorL(L, 2, "Vector3");
	return 0;
}

static int cframe_inverse(lua_State* L) {
	if (auto* cf = cframe_lua_check(L, 1)) [[likely]] {
		cframe_lua_push(L, cf->inverse());
	}

	return 1;
}

static int cframe_lerp(lua_State* L) {
	auto* cf = cframe_lua_check(L, 1);
	auto* goal = cframe_lua_check(L, 2);
	auto alpha = luaL_checknumber(L, 3);

	if (!cf || !goal) [[unlikely]] {
		return 0;
	}

	cframe_lua_push(L, cf->lerp(*goal, alpha));
	return 1;
}

static int cframe_get_components(lua_State* L) {
	auto* cf = reinterpret_cast<const float*>(lua_touserdatatagged(L, 1, LUA_TAG_CFRAME));

	if (!cf) [[unlikely]] {
		luaL_typeerrorL(L, 1, "CFrame");
		return 0;
	}

	for (int i = 0; i < 12; ++i) {
		lua_pushnumber(L, cf[i]);
	}

	return 12;
}

