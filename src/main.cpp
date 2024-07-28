#include <cstdio>
#include <cstring>
#include <csignal>

#include <lua.h>
#include <lualib.h>

#include <atomic>
#include <chrono>

#include <script_env.hpp>
#include <instance_lua.hpp>
#include <script_signal.hpp>

static std::atomic_flag g_finished = ATOMIC_FLAG_INIT;

static void handle_signal(int sig);

int main() {
	using namespace std::chrono;

	std::signal(SIGINT, handle_signal);

	ScriptEnvironment env;
	auto* L = env.get_state();

	instance_lua_load(L);

	ScriptSignal* sig = lua_push<ScriptSignal>(L);
	lua_setglobal(L, "event");

	env.run_script_file("../test_signal.lua");

	lua_pushinteger(L, 7);
	lua_pushinteger(L, 8);
	lua_pushstring(L, "Hey there");
	script_signal_fire(L, sig, 3);

	lua_pushinteger(L, 2);
	lua_pushinteger(L, 1);
	lua_pushstring(L, "Hello thar");
	script_signal_fire(L, sig, 3);

	//env.run_script_file("../test.lua");

	auto start = high_resolution_clock::now();
	double lastTime = 0.0;

	while (!g_finished.test()) {
		auto currTime = duration_cast<duration<double>>(high_resolution_clock::now() - start).count();
		auto deltaTime = currTime - lastTime;
		lastTime = currTime;

		env.update(static_cast<float>(deltaTime));
	}

	return 0;
}

static void handle_signal(int) {
	g_finished.test_and_set();
}

