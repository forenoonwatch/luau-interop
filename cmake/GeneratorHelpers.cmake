find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

macro(gen_per_interface script input source header)
	add_custom_command(
		OUTPUT
			${source}
			${header}
		COMMAND
			Python3::Interpreter ${script} ${input} ${CMAKE_CURRENT_BINARY_DIR}
		DEPENDS
			${input}
			${script}
	)
	
	list(APPEND INTERFACE_GENERATED_SOURCE_FILES ${source})
	list(APPEND INTERFACE_GENERATED_HEADER_FILES ${header})
endmacro()

function(fn_gen_gather_interface interface_dir script source header)
	file(GLOB_RECURSE INTERFACE_DEFINITION_FILES
		"${interface_dir}/*.json"
	)

	add_custom_command(
		OUTPUT
			${source}
			${header}
		COMMAND
			Python3::Interpreter ${script} ${interface_dir} ${CMAKE_CURRENT_BINARY_DIR}
		DEPENDS
			${INTERFACE_DEFINITION_FILES}
			${script}
	)
endfunction()

macro(gen_gather_interfaces interface_dir script source header)
	fn_gen_gather_interface(${interface_dir} ${script} "${source}" "${header}")
	list(APPEND INTERFACE_GENERATED_SOURCE_FILES ${source})
	list(APPEND INTERFACE_GENERATED_HEADER_FILES ${header})
endmacro()

