cmake_minimum_required(VERSION 4.0)

add_subdirectory("typescript_build")
add_subdirectory("tatzel")
add_subdirectory("development")

option(TATZEL_BUILD_WORKSPACE "Build the persistent workspace sample" ON)
if(TATZEL_BUILD_WORKSPACE)
	add_subdirectory("workspace")
endif()
