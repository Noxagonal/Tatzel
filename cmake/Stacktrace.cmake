include_guard(GLOBAL)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

# Verify the standard-library implementation as well as the compiler language mode.
set(_STACKTRACE_PROBE "#include <stacktrace>
#include <string>
int main() { return std::to_string(std::stacktrace::current()).empty(); }")

add_library(TatzelStacktrace INTERFACE)
cmake_push_check_state(RESET)
check_cxx_source_compiles("${_STACKTRACE_PROBE}" TATZEL_STACKTRACE_NATIVE)
if(NOT TATZEL_STACKTRACE_NATIVE)
	set(CMAKE_REQUIRED_LIBRARIES stdc++exp)
	check_cxx_source_compiles("${_STACKTRACE_PROBE}" TATZEL_STACKTRACE_EXP)
	if(TATZEL_STACKTRACE_EXP)
		target_link_libraries(TatzelStacktrace INTERFACE stdc++exp)
	else()
		set(CMAKE_REQUIRED_LIBRARIES stdc++_libbacktrace)
		check_cxx_source_compiles("${_STACKTRACE_PROBE}" TATZEL_STACKTRACE_BACKTRACE)
		if(TATZEL_STACKTRACE_BACKTRACE)
			target_link_libraries(TatzelStacktrace INTERFACE stdc++_libbacktrace)
		else()
			message(FATAL_ERROR "Tatzel requires a working std::stacktrace implementation and its matching standard-library support library. Select a compatible compiler and standard library.")
		endif()
	endif()
endif()
cmake_pop_check_state()
