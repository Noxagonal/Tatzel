cmake_minimum_required(VERSION 4.0)
project(TatzelInstalledExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Tatzel 0.1 CONFIG REQUIRED)

add_executable(TatzelInstalledExample Main.cpp)
target_link_libraries(TatzelInstalledExample PRIVATE Tatzel::Tatzel)
tatzel_deploy_runtime(TatzelInstalledExample)
