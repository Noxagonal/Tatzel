cmake_minimum_required(VERSION 4.0)
project(Tatzel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TATZEL_BUILD_TESTS "Build logical/DOM core behavior tests" OFF)
option(TATZEL_BUILD_DEVELOPMENT "Build the development showcases" ${PROJECT_IS_TOP_LEVEL})
option(TATZEL_BUILD_WORKSPACE "Build the persistent workspace sample" ${PROJECT_IS_TOP_LEVEL})
option(TATZEL_USE_SYSTEM_DROGON "Use an installed Drogon package instead of fetching it" OFF)

include(cmake/Dependencies.cmake)
tatzel_prepare_dependencies()

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
