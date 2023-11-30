#pragma once

#include <string>
#include <vector>

struct lua_State;

struct ScriptSignal;

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

		ScriptSignal* script_signal_create(lua_State* L);
		void script_signal_fire(ScriptSignal*);

		bool run_script_file(const char* fileName);
		bool run_script_source_code(const char* chunkName, const std::string& fileData);
		bool run_script_bytecode(const char* chunkName, const std::string& bytecode);

		int defer(lua_State* T, float waitTime);

		void get_ref_event_list(lua_State* L);
		lua_State* get_state();
	private:
		struct ScheduledScript {
			lua_State* state;
			float timeToRun;
		};

		lua_State* m_L;
		int m_refEventList;
		int m_refInstanceLookup;
		std::vector<ScheduledScript> m_jobs;
};

