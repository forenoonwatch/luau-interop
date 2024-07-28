#pragma once

#include <utility>

#include <lualib.h>

template <typename>
struct LuaTypeTraits;

template <typename T>
struct LuaTypeGetter {
	T* operator()(lua_State* L, int idx) {
		return reinterpret_cast<T*>(lua_touserdatatagged(L, idx, LuaTypeTraits<T>::TAG));
	}
};

template <typename>
struct LuaPusher;

template <typename T>
decltype(auto) lua_get(lua_State* L, int idx) {
	return LuaTypeGetter<T>{}(L, idx);
}

template <typename>
void lua_init_metatable(lua_State*);

template <typename T, typename... Args>
decltype(auto) lua_push(lua_State* L, Args&&... args) {
	return LuaPusher<T>{}(L, std::forward<Args>(args)...);
}

template <typename T>
decltype(auto) lua_check(lua_State* L, int idx) {
    auto* result = LuaTypeGetter<T>{}(L, idx);

	if (!result) {
        luaL_typeerrorL(L, idx, LuaTypeTraits<T>::NAME.data());
	}

	return result;
}

template <typename T>
const T* lua_opt(lua_State* L, int idx, const T* defaultValue) {
    if (auto* result = LuaTypeGetter<T>{}(L, idx)) {
        return result;
    }
    else {
        return defaultValue;
    }
}

