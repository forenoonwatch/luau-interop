#include "vector3_lua.hpp"

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <cmath>

static int vector3_new(lua_State* L);

static int vector3_namecall(lua_State* L);
static int vector3_index(lua_State* L);

static int vector3_dot(lua_State* L);
static int vector3_cross(lua_State* L);
static int vector3_lerp(lua_State* L);
static int vector3_fuzzy_eq(lua_State* L);
static int vector3_max(lua_State* L);
static int vector3_min(lua_State* L);

static float calc_magnitude(const float* v);
static bool fuzzyeq(float d, float e);

// Public Functions

void vector3_lua_load(lua_State* L) {
	luaL_findtable(L, LUA_GLOBALSINDEX, "Vector3", 0);
	lua_pushcfunction(L, vector3_new, "vector3_new");
	lua_setfield(L, -2, "new");
}

void vector3_lua_push(lua_State* L, float x, float y, float z) {
	lua_pushvector(L, x, y, z);

	if (luaL_newmetatable(L, "Vector3")) {
		lua_pushstring(L, "Vector3");
		lua_setfield(L, -2, "__type");

		lua_pushcfunction(L, vector3_namecall, "vector3_namecall");
		lua_setfield(L, -2, "__namecall");

		lua_pushcfunction(L, vector3_index, "vector3_index");
		lua_setfield(L, -2, "__index");

		lua_setreadonly(L, -1, true);
	}

	lua_setmetatable(L, -2);
}

// Static Functions

static int vector3_new(lua_State* L) {
	auto x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
	auto y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
	auto z = static_cast<float>(luaL_optnumber(L, 3, 0.0));
	vector3_lua_push(L, x, y, z);
	return 1;
}

static int vector3_namecall(lua_State* L) {
	if (const char* name = lua_namecallatom(L, nullptr)) {
		if (strcmp(name, "Dot") == 0) {
			return vector3_dot(L);
		}
		else if (strcmp(name, "Cross") == 0) {
			return vector3_cross(L);
		}
		else if (strcmp(name, "FuzzyEq") == 0) {
			return vector3_fuzzy_eq(L);
		}
		else if (strcmp(name, "Lerp") == 0) {
			return vector3_lerp(L);
		}
		else if (strcmp(name, "Max") == 0) {
			return vector3_max(L);
		}
		else if (strcmp(name, "Min") == 0) {
			return vector3_min(L);
		}
	}

	luaL_error(L, "%s is not a valid method of Vector3", luaL_checkstring(L, 1));
	return 0;
}

static int vector3_index(lua_State* L) {
	const float* v = luaL_checkvector(L, 1);
	const char* name = luaL_checkstring(L, 2);

	if (strcmp(name, "Magnitude") == 0) {
		lua_pushnumber(L, calc_magnitude(v));
		return 1;
	}
	else if (strcmp(name, "Unit") == 0) {
		auto mI = 1.f / calc_magnitude(v);
		vector3_lua_push(L, v[0] * mI, v[1] * mI, v[2] * mI);
		return 1;
	}
	else if (strcmp(name, "Dot") == 0) {
		lua_pushcfunction(L, vector3_dot, "vector3_dot");
		return 1;
	}
	else if (strcmp(name, "Cross") == 0) {
		lua_pushcfunction(L, vector3_cross, "vector3_cross");
		return 1;
	}
	else if (strcmp(name, "FuzzyEq") == 0) {
		lua_pushcfunction(L, vector3_fuzzy_eq, "vector3_fuzzy_eq");
		return 1;
	}
	else if (strcmp(name, "Lerp") == 0) {
		lua_pushcfunction(L, vector3_lerp, "vector3_lerp");
		return 1;
	}
	else if (strcmp(name, "Max") == 0) {
		lua_pushcfunction(L, vector3_max, "vector3_max");
		return 1;
	}
	else if (strcmp(name, "Min") == 0) {
		lua_pushcfunction(L, vector3_min, "vector3_min");
		return 1;
	}

	luaL_error(L, "%s is not a valid member of Vector3", name);
	return 0;
}

static int vector3_dot(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);

	lua_pushnumber(L, a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
	return 1;
}

static int vector3_cross(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);

	auto x = a[1] * b[2] - a[2] * b[1];
	auto y = a[2] * b[0] - a[0] * b[2];
	auto z = a[0] * b[1] - a[1] * b[0];

	vector3_lua_push(L, x, y, z);
	return 1;
}

static int vector3_lerp(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);
	float c = static_cast<float>(luaL_checknumber(L, 3));

	vector3_lua_push(L, std::lerp(a[0], b[0], c), std::lerp(a[1], b[1], c), std::lerp(a[2], b[2], c));
	return 1;
}

static int vector3_fuzzy_eq(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);
	float epsilon = static_cast<float>(luaL_optnumber(L, 3, 1.0e-5));

	lua_pushboolean(L, fuzzyeq(a[0] - b[0], epsilon) && fuzzyeq(a[1] - b[1], epsilon)
			&& fuzzyeq(a[2] - b[2], epsilon));
	return 1;
}

static int vector3_max(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);

	vector3_lua_push(L, fmaxf(a[0], b[0]), fmaxf(a[1], b[1]), fmaxf(a[2], b[2]));
	return 1;
}

static int vector3_min(lua_State* L) {
	const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);

	vector3_lua_push(L, fminf(a[0], b[0]), fminf(a[1], b[1]), fminf(a[2], b[2]));
	return 1;
}

static float calc_magnitude(const float* v) {
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static bool fuzzyeq(float d, float e) {
	return d >= -e && d <= e;
}

