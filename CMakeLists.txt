cmake_minimum_required(VERSION 3.14)
include(FetchContent)

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