target_sources(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/instance.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/script_env.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/cframe_lua.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/instance_lua.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/script_signal.cpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
