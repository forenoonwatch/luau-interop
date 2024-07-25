cmake_minimum_required(VERSION 3.14)
include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(
	TestLua VERSION 1.0
	LANGUAGES CXX
)

FetchContent_Declare(
	luau
	GIT_REPOSITORY https://github.com/Roblox/luau.git
)

FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
)

FetchContent_MakeAvailable(luau)
FetchContent_MakeAvailable(glm)

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(${PROJECT_NAME} "")
target_link_libraries(${PROJECT_NAME} PRIVATE glm)
target_link_libraries(${PROJECT_NAME} PRIVATE Luau.Compiler)
target_link_libraries(${PROJECT_NAME} PRIVATE Luau.VM)
set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

add_subdirectory(src)

include(GeneratorHelpers)

gen_gather_interfaces(
	"${PROJECT_SOURCE_DIR}/codegen"
	"${PROJECT_SOURCE_DIR}/tools/gen_lua_bindings.py"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/lua_bindings.cpp"
	""
)

target_sources(${PROJECT_NAME} PRIVATE ${INTERFACE_GENERATED_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)
#add_custom_target(generatedfiles DEPENDS ${INTERFACE_GENERATED_SOURCE_FILES} ${INTERFACE_GENERATED_HEADER_FILES})
#add_dependencies(${PROJECT_NAME} generatedfiles)
