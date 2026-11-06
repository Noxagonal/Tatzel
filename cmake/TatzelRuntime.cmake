include_guard(GLOBAL)

#[[!
@brief Copy the matching framework runtime beside an application on every build.
@param TARGET_NAME Existing non-imported executable linked to Tatzel::Tatzel.
@pre Tatzel::Tatzel exists and its runtime directory is available at build time.
@post Building the executable also populates its adjacent tatzel-runtime directory.
]]
function(tatzel_deploy_runtime TARGET_NAME)
	if(NOT TARGET "${TARGET_NAME}" OR NOT TARGET Tatzel::Tatzel)
		message(FATAL_ERROR "tatzel_deploy_runtime requires an executable and Tatzel::Tatzel")
	endif()
	get_target_property(_TYPE "${TARGET_NAME}" TYPE)
	get_target_property(_IMPORTED "${TARGET_NAME}" IMPORTED)
	if(NOT _TYPE STREQUAL "EXECUTABLE" OR _IMPORTED)
		message(FATAL_ERROR "tatzel_deploy_runtime requires a locally built executable")
	endif()
	if(TARGET "${TARGET_NAME}_tatzel_runtime")
		return()
	endif()
	get_target_property(_RUNTIME_DIRECTORY Tatzel::Tatzel TATZEL_RUNTIME_DIRECTORY)
	# With CMP0112 NEW (CMake 4), TARGET_FILE_DIR adds no reverse target dependency.
	add_custom_target("${TARGET_NAME}_tatzel_runtime"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_RUNTIME_DIRECTORY}"
			"$<TARGET_FILE_DIR:${TARGET_NAME}>/tatzel-runtime"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Tatzel::Tatzel>"
			"$<TARGET_FILE_DIR:${TARGET_NAME}>/$<TARGET_SONAME_FILE_NAME:Tatzel::Tatzel>"
		VERBATIM
	)
	get_target_property(_LIBRARY_IMPORTED Tatzel::Tatzel IMPORTED)
	if(NOT _LIBRARY_IMPORTED)
		add_dependencies("${TARGET_NAME}_tatzel_runtime" Tatzel)
	endif()
	set_property(TARGET "${TARGET_NAME}" APPEND PROPERTY BUILD_RPATH "$ORIGIN")
	set_property(TARGET "${TARGET_NAME}" PROPERTY INSTALL_RPATH "$ORIGIN")
	if(TARGET tatzel_runtime)
		add_dependencies("${TARGET_NAME}_tatzel_runtime" tatzel_runtime)
	endif()
	add_dependencies("${TARGET_NAME}" "${TARGET_NAME}_tatzel_runtime")
endfunction()
