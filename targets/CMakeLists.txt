add_subdirectory(typescript_build)

add_custom_target(tatzel_runtime ALL
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${Tatzel_RUNTIME_DIRECTORY}/data" "${Tatzel_RUNTIME_DIRECTORY}/www"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${Tatzel_SOURCE_DIR}/data/template.html" "${Tatzel_SOURCE_DIR}/data/drogon_config.json"
		"${Tatzel_RUNTIME_DIRECTORY}/data"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory "${Tatzel_SOURCE_DIR}/www/framework" "${Tatzel_RUNTIME_DIRECTORY}/www/framework"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory "${Tatzel_SOURCE_DIR}/www/app" "${Tatzel_RUNTIME_DIRECTORY}/www/app"
	VERBATIM
)
add_dependencies(tatzel_runtime typescript_build)
add_subdirectory(tatzel)
if(TATZEL_BUILD_DEVELOPMENT)
	add_subdirectory(development)
endif()
if(TATZEL_BUILD_WORKSPACE)
	add_subdirectory(workspace)
endif()
