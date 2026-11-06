include_guard(GLOBAL)

#[[!
@brief Build the shared Tatzel module library with the selected compiler.
@param SOURCE_DIRECTORY Checkout library source directory.
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

	add_library(Tatzel SHARED)
	add_library(Tatzel::Tatzel ALIAS Tatzel)
	set_target_properties(Tatzel PROPERTIES VERSION "${Tatzel_VERSION}" SOVERSION "${Tatzel_VERSION_MAJOR}")
	set_default_target_files(Tatzel "${SOURCE_DIRECTORY}")
	target_compile_features(Tatzel PUBLIC cxx_std_26)
	target_link_libraries(Tatzel PUBLIC TatzelStacktrace PRIVATE "$<BUILD_INTERFACE:Drogon::Drogon>")
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_options(Tatzel PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:-z,defs"
			"LINKER:--version-script=${Tatzel_SOURCE_DIR}/cmake/Tatzel.exports")
		set_property(TARGET Tatzel APPEND PROPERTY LINK_DEPENDS "${Tatzel_SOURCE_DIR}/cmake/Tatzel.exports")
	endif()

	get_filename_component(_RESOURCE_DIRECTORY "${RESOURCE_DIRECTORY}" ABSOLUTE)
	string(REPLACE "\\" "\\\\" TATZEL_RUNTIME_DIRECTORY_LITERAL "${_RESOURCE_DIRECTORY}")
	string(REPLACE "\"" "\\\"" TATZEL_RUNTIME_DIRECTORY_LITERAL "${TATZEL_RUNTIME_DIRECTORY_LITERAL}")
	configure_file("${SOURCE_DIRECTORY}/private/RuntimeConfiguration.hpp.in"
		"${CMAKE_CURRENT_BINARY_DIR}/tatzel-generated/RuntimeConfiguration.hpp" @ONLY)
	target_include_directories(Tatzel PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/tatzel-generated>")
	set_property(TARGET Tatzel PROPERTY TATZEL_RUNTIME_DIRECTORY "${_RESOURCE_DIRECTORY}")
endfunction()
