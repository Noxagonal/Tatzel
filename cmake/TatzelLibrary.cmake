include_guard(GLOBAL)

#[[!
@brief Build the Tatzel module library with the current project's compiler.
@param SOURCE_DIRECTORY Installed or checkout library source directory.
@param RESOURCE_DIRECTORY Matching runtime directory, available before running applications.
@pre Drogon::Drogon exists; a module-capable C++26 toolchain is configured.
@post Tatzel::Tatzel supplies the library, public module sources, and language requirements.
]]
function(tatzel_add_library
	SOURCE_DIRECTORY
	RESOURCE_DIRECTORY
)
	set(CMAKE_CXX_STANDARD 26)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_EXTENSIONS OFF)
	include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Stacktrace.cmake")
	include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/TargetOps.cmake")

	add_library(Tatzel STATIC)
	add_library(Tatzel::Tatzel ALIAS Tatzel)
	set_default_target_files(Tatzel "${SOURCE_DIRECTORY}")
	target_compile_features(Tatzel PUBLIC cxx_std_26)
	target_link_libraries(Tatzel PUBLIC TatzelStacktrace PRIVATE Drogon::Drogon)

	get_filename_component(_RESOURCE_DIRECTORY "${RESOURCE_DIRECTORY}" ABSOLUTE)
	string(REPLACE "\\" "\\\\" TATZEL_RUNTIME_DIRECTORY_LITERAL "${_RESOURCE_DIRECTORY}")
	string(REPLACE "\"" "\\\"" TATZEL_RUNTIME_DIRECTORY_LITERAL "${TATZEL_RUNTIME_DIRECTORY_LITERAL}")
	configure_file("${SOURCE_DIRECTORY}/private/RuntimeConfiguration.hpp.in"
		"${CMAKE_CURRENT_BINARY_DIR}/tatzel-generated/RuntimeConfiguration.hpp" @ONLY)
	target_include_directories(Tatzel PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/tatzel-generated")
	set_property(TARGET Tatzel PROPERTY TATZEL_RUNTIME_DIRECTORY "${_RESOURCE_DIRECTORY}")
endfunction()
