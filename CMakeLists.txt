cmake_minimum_required(VERSION 4.0)
project(Tatzel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TATZEL_BUILD_TESTS "Build logical/DOM core behavior tests" OFF)
option(TATZEL_BUILD_DEVELOPMENT "Build the development showcases" ${PROJECT_IS_TOP_LEVEL})
option(TATZEL_BUILD_WORKSPACE "Build the persistent workspace sample" ${PROJECT_IS_TOP_LEVEL})
option(TATZEL_USE_SYSTEM_DROGON "Use an installed Drogon package instead of fetching it" OFF)

if(NOT TARGET Drogon::Drogon)
	if(TATZEL_USE_SYSTEM_DROGON)
		find_package(Drogon 1.9.13 CONFIG REQUIRED)
	else()
		include(FetchContent)
		# Defaults for the fetched dependency; respect an embedding project's choices.
		set(BUILD_CTL OFF CACHE BOOL "Build drogon_ctl")
		set(BUILD_EXAMPLES OFF CACHE BOOL "Build Drogon examples")
		set(BUILD_ORM OFF CACHE BOOL "Build Drogon ORM")
		set(BUILD_BROTLI OFF CACHE BOOL "Build Brotli support")
		set(BUILD_YAML_CONFIG OFF CACHE BOOL "Build YAML configuration")
		FetchContent_Declare(drogon
			GIT_REPOSITORY https://github.com/drogonframework/drogon.git
			GIT_TAG v1.9.13 GIT_SHALLOW TRUE)
		FetchContent_MakeAvailable(drogon)
	endif()
endif()

if(TATZEL_BUILD_TESTS)
	enable_testing()
endif()

set(Tatzel_RUNTIME_DIRECTORY "${Tatzel_BINARY_DIR}/runtime")
include(cmake/TatzelRuntime.cmake)
add_subdirectory(targets)
if(TATZEL_BUILD_TESTS)
	add_subdirectory(tests/cpp)
endif()
include(cmake/InstallPackage.cmake)
