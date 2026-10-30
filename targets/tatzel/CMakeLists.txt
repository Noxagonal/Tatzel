cmake_minimum_required(VERSION 4.0)

include("${Tatzel_SOURCE_DIR}/cmake/TargetOps.cmake")


##############################################################
# Tatzel library target
##############################################################
add_library(Tatzel STATIC)
set_default_target_files(Tatzel "${CMAKE_CURRENT_SOURCE_DIR}")

target_precompile_headers(Tatzel
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/private/PCH.hpp"
)

target_link_libraries(Tatzel
	PRIVATE
		Drogon::Drogon
)

add_dependencies(Tatzel
	typescript_build
)
