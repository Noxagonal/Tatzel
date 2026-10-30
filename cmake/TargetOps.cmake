include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/FileOps.cmake")


#[[!
@brief Attach a target's public/private sources, headers, and module interfaces.
@param TARGET_NAME Existing CMake target to configure.
@param BASE_FOLDER Directory containing public and private source folders.
@pre TARGET_NAME exists and the source folders follow the documented layout.
@post Files are attached with their corresponding visibility and module interfaces skip the PCH.
]]
function(set_default_target_files
	TARGET_NAME
	BASE_FOLDER
)
	if(NOT TARGET "${TARGET_NAME}")
		message(FATAL_ERROR
			"set_default_target_files: Target '${TARGET_NAME}' does not exist."
		)
	endif()

	get_filename_component(
		_BASE_FOLDER
		"${BASE_FOLDER}"
		ABSOLUTE
	)

	set(_PUBLIC_FOLDER  "${_BASE_FOLDER}/public")
	set(_PRIVATE_FOLDER "${_BASE_FOLDER}/private")

	collect_implementation_files(
		_PUBLIC_IMPLEMENTATION_FILES
		"${_PUBLIC_FOLDER}"
	)
	collect_implementation_files(
		_PRIVATE_IMPLEMENTATION_FILES
		"${_PRIVATE_FOLDER}"
	)

	collect_header_files(
		_PUBLIC_HEADER_FILES
		"${_PUBLIC_FOLDER}"
	)
	collect_header_files(
		_PRIVATE_HEADER_FILES
		"${_PRIVATE_FOLDER}"
	)

	collect_module_files(
		_PUBLIC_MODULE_FILES
		"${_PUBLIC_FOLDER}"
	)
	collect_module_files(
		_PRIVATE_MODULE_FILES
		"${_PRIVATE_FOLDER}"
	)

	set(_ALL_TARGET_FILES
		${_PUBLIC_IMPLEMENTATION_FILES}
		${_PRIVATE_IMPLEMENTATION_FILES}
		${_PUBLIC_HEADER_FILES}
		${_PRIVATE_HEADER_FILES}
		${_PUBLIC_MODULE_FILES}
		${_PRIVATE_MODULE_FILES}
	)

	if(_ALL_TARGET_FILES)
		source_group(
			TREE "${_BASE_FOLDER}"
			FILES ${_ALL_TARGET_FILES}
		)
	endif()

	# Implementation files are always compiled only as part of this target.
	set(_IMPLEMENTATION_FILES
		${_PUBLIC_IMPLEMENTATION_FILES}
		${_PRIVATE_IMPLEMENTATION_FILES}
	)

	if(_IMPLEMENTATION_FILES)
		target_sources("${TARGET_NAME}"
			PRIVATE
				${_IMPLEMENTATION_FILES}
		)
	endif()

	if(_PUBLIC_HEADER_FILES)
		target_sources("${TARGET_NAME}"
			PUBLIC
				FILE_SET public_headers
					TYPE HEADERS
					BASE_DIRS "${_PUBLIC_FOLDER}"
					FILES ${_PUBLIC_HEADER_FILES}
		)
	endif()

	if(_PRIVATE_HEADER_FILES)
		target_sources("${TARGET_NAME}"
			PRIVATE
				FILE_SET private_headers
					TYPE HEADERS
					BASE_DIRS "${_PRIVATE_FOLDER}"
					FILES ${_PRIVATE_HEADER_FILES}
		)
	endif()

	if(_PUBLIC_MODULE_FILES)
		target_sources("${TARGET_NAME}"
			PUBLIC
				FILE_SET public_modules
					TYPE CXX_MODULES
					BASE_DIRS "${_PUBLIC_FOLDER}"
					FILES ${_PUBLIC_MODULE_FILES}
		)
	endif()

	if(_PRIVATE_MODULE_FILES)
		target_sources("${TARGET_NAME}"
			PRIVATE
				FILE_SET private_modules
					TYPE CXX_MODULES
					BASE_DIRS "${_PRIVATE_FOLDER}"
					FILES ${_PRIVATE_MODULE_FILES}
		)
	endif()

	set_source_files_properties(
		${_PUBLIC_MODULE_FILES}
		PROPERTIES
			SKIP_PRECOMPILE_HEADERS ON
	)

endfunction()
