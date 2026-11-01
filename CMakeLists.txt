cmake_minimum_required(VERSION 4.0)

project(Tatzel LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(cmake/Stacktrace.cmake)

# Keep the initial build smaller.
# You can turn these back ON later if you need them.
set(BUILD_CTL OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_ORM OFF CACHE BOOL "" FORCE)
set(BUILD_BROTLI OFF CACHE BOOL "" FORCE)
set(BUILD_YAML_CONFIG OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	drogon
	GIT_REPOSITORY https://github.com/drogonframework/drogon.git
	GIT_TAG v1.9.13
	GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(drogon)

add_subdirectory("targets")

option(TATZEL_BUILD_TESTS "Build logical/DOM core behavior tests" OFF)
if(TATZEL_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests/cpp)
endif()
