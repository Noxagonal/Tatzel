#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/tatzel-release-check.XXXXXXXX")
trap 'rm -rf -- "$work_dir"' EXIT

export CXX="${CXX:-clang++}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

cmake -S "$source_dir" -B "$work_dir/framework" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DTATZEL_BUILD_DEVELOPMENT=OFF \
	-DTATZEL_BUILD_WORKSPACE=OFF
cmake --build "$work_dir/framework"
cmake --install "$work_dir/framework" --prefix "$work_dir/install"

mkdir "$work_dir/consumer"
cat > "$work_dir/consumer/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 4.0)
project(TatzelReleaseConsumer LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Tatzel CONFIG REQUIRED PATHS "${TATZEL_TEST_PREFIX}" NO_DEFAULT_PATH)
add_executable(TatzelReleaseConsumer Main.cpp)
target_link_libraries(TatzelReleaseConsumer PRIVATE Tatzel::Tatzel)
CMAKE

cat > "$work_dir/consumer/Main.cpp" <<'CPP'
import UI;

/// @brief References an exported library function to verify consumer linkage.
/// @return Zero after the logging call.
/// @pre None.
/// @post The logging call has completed.
auto main() -> int
{
	tatzel::Log( tatzel::LogLevel::Info, "Release package consumer" );
	return 0;
}
CPP

cmake -S "$work_dir/consumer" -B "$work_dir/consumer-build" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	"-DTATZEL_TEST_PREFIX=$work_dir/install"
cmake --build "$work_dir/consumer-build"

printf 'Tatzel Release package compiled and linked from a fresh consumer project.\n'
