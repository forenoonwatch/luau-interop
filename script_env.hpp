#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

class ScriptEnvironment final {
	public:
		static ScriptEnvironment* get(lua_State* L);

		explicit ScriptEnvironment();
		~ScriptEnvironment();

		ScriptEnvironment(ScriptEnvironment&&) = delete;
		void operator=(ScriptEnvironment&&) = delete;
		ScriptEnvironment(const ScriptEnvironment&) = delete;
		void operator=(const ScriptEnvironment&) = delete;

		void update(float deltaTime);

		bool run_script_file(const char* fileName);
		bool run_script_source_code(const char* chunkName, const std::string& fileData);
		bool run_script_bytecode(const char* chunkName, const std::string& bytecode);

		/**
		 * Yields the given thread and delays its execution for `waitTime` seconds.
		 *
		 * @return result from lua_yield, to be returned by the caller.
		 */
		int delay(lua_State* T, float waitTime);

		/**
		 * Yields the given thread and delays its execution until the next call to `update()`.
		 * Equivalent to `delay(T, 0)`.
		 *
		 * @return result from lua_yield, to be returned by the caller.
		 */
		int defer(lua_State* T);

		/**
		 * Yields the given thread T to be resumed when the given address is unparked.
		 *
		 * @return result from lua_yield, to be returned by the caller.
		 */
		int park(lua_State* T, const void* address);

		/**
		 * Resumes all threads waiting on the given address.
		 */
		void unpark(const void* address);

		/**
		 * Resumes all the threads waiting on the given address, with `L` as the resumption source,
		 * and copying `argCount` args off of `L`'s stack as resumption parameters.
		 */
		void unpark(const void* address, lua_State* L, int argCount);

		lua_State* get_state();
	private:
		struct ScheduledScript {
			lua_State* state;
			float timeToRun;
		};

		lua_State* m_L;
		int m_refInstanceLookup;
		std::vector<ScheduledScript> m_timeDelayedJobs;
		std::unordered_map<const void*, std::vector<lua_State*>> m_parkingLot;
};

