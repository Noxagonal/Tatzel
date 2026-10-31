
include_guard(GLOBAL)


# Collect all files in a directory tree with the given extensions.
#
# Example:
# collect_files_by_extension(_SOURCE_FILES
#     "${CMAKE_CURRENT_SOURCE_DIR}"
#     c cc cpp cxx h hh hpp hxx
# )
#[[!
@brief Collect files with the requested extensions below a base directory.
@param OUTPUT_VAR Name of the variable receiving sorted absolute paths.
@param BASE_DIR Directory searched recursively.
@param ARGN One or more file extensions, with or without a leading dot.
@pre A result variable, base directory, and at least one extension are supplied.
@post OUTPUT_VAR is set in the caller's scope; no files are modified.
]]
function(collect_files_by_extension
	OUTPUT_VAR
	BASE_DIR
)
	if(ARGC LESS 3)
		message(FATAL_ERROR
			"collect_files_by_extension requires an output variable, "
			"a base directory, and at least one extension."
		)
	endif()

	set(_FILES)

	foreach(_CURRENT_EXT IN LISTS ARGN)
		# Permit extensions to be supplied as either "cpp" or ".cpp".
		string(REGEX REPLACE "^\\." "" _CURRENT_EXT "${_CURRENT_EXT}")

		file(GLOB_RECURSE _MATCHING_FILES
			CONFIGURE_DEPENDS
			"${BASE_DIR}/*.${_CURRENT_EXT}"
		)

		list(APPEND _FILES ${_MATCHING_FILES})
	endforeach()

	list(REMOVE_DUPLICATES _FILES)
	list(SORT _FILES)

	set(${OUTPUT_VAR} "${_FILES}" PARENT_SCOPE)
endfunction()


# Collect ordinary C and C++ source/header files from a directory tree.
#[[!
@brief Collect ordinary C/C++ implementation and header files.
@param OUTPUT_VAR Name of the caller-scope result variable.
@param BASE_DIR Directory searched recursively.
@pre BASE_DIR identifies the intended source tree.
@post OUTPUT_VAR contains sorted matching paths; files are unchanged.
]]
function(collect_source_files
	OUTPUT_VAR
	BASE_DIR
)
	collect_files_by_extension(_FILES
		"${BASE_DIR}"
		c cc cpp cxx h hh hpp hxx
	)

	set(${OUTPUT_VAR} "${_FILES}" PARENT_SCOPE)
endfunction()


# Collect ordinary C and C++ source files from a directory tree.
#[[!
@brief Collect ordinary C/C++ implementation files.
@param OUTPUT_VAR Name of the caller-scope result variable.
@param BASE_DIR Directory searched recursively.
@pre BASE_DIR identifies the intended source tree.
@post OUTPUT_VAR contains sorted implementation paths; files are unchanged.
]]
function(collect_implementation_files
	OUTPUT_VAR
	BASE_DIR
)
	collect_files_by_extension(_FILES
		"${BASE_DIR}"
		c cc cpp cxx
	)

	set(${OUTPUT_VAR} "${_FILES}" PARENT_SCOPE)
endfunction()


# Collect ordinary C and C++ header files from a directory tree.
#[[!
@brief Collect ordinary C/C++ header files.
@param OUTPUT_VAR Name of the caller-scope result variable.
@param BASE_DIR Directory searched recursively.
@pre BASE_DIR identifies the intended source tree.
@post OUTPUT_VAR contains sorted header paths; files are unchanged.
]]
function(collect_header_files
	OUTPUT_VAR
	BASE_DIR
)
	collect_files_by_extension(_FILES
		"${BASE_DIR}"
		h hh hpp hxx
	)

	set(${OUTPUT_VAR} "${_FILES}" PARENT_SCOPE)
endfunction()


# Collect C++ module interface files from a directory tree.
#[[!
@brief Collect C++ module interface files.
@param OUTPUT_VAR Name of the caller-scope result variable.
@param BASE_DIR Directory searched recursively.
@pre BASE_DIR identifies the intended source tree.
@post OUTPUT_VAR contains sorted .ixx and .cppm paths; files are unchanged.
]]
function(collect_module_files
	OUTPUT_VAR
	BASE_DIR
)
	collect_files_by_extension(_FILES
		"${BASE_DIR}"
		ixx cppm
	)

	set(${OUTPUT_VAR} "${_FILES}" PARENT_SCOPE)
endfunction()
