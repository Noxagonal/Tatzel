add_executable(TatzelCoreTests LogicalCore.cpp)
target_link_libraries(TatzelCoreTests PRIVATE Tatzel)
add_test(NAME TatzelCoreTests COMMAND TatzelCoreTests)

add_executable(TatzelControlTests Controls.cpp)
target_link_libraries(TatzelControlTests PRIVATE Tatzel)
add_test(NAME TatzelControlTests COMMAND TatzelControlTests)

add_executable(TatzelFormTests Forms.cpp)
target_link_libraries(TatzelFormTests PRIVATE Tatzel)
add_test(NAME TatzelFormTests COMMAND TatzelFormTests)

add_executable(TatzelRemovalTests Removal.cpp)
target_link_libraries(TatzelRemovalTests PRIVATE Tatzel)
add_test(NAME TatzelRemovalTests COMMAND TatzelRemovalTests)

add_executable(TatzelDiagnosticTests Diagnostics.cpp)
target_link_libraries(TatzelDiagnosticTests PRIVATE Tatzel Drogon::Drogon)
add_test(NAME TatzelDiagnosticTests COMMAND TatzelDiagnosticTests)

add_executable(TatzelOptimizedTraceTests
	OptimizedTrace.cpp
	"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Diagnostics.cpp"
)
target_sources(TatzelOptimizedTraceTests PRIVATE
	FILE_SET diagnostic_modules TYPE CXX_MODULES
	BASE_DIRS "${Tatzel_SOURCE_DIR}/targets/tatzel/public"
	FILES "${Tatzel_SOURCE_DIR}/targets/tatzel/public/Diagnostics.ixx"
)
target_compile_definitions(TatzelOptimizedTraceTests PRIVATE NDEBUG)
if(MSVC)
	target_compile_options(TatzelOptimizedTraceTests PRIVATE /O2 /Zi)
else()
	target_compile_options(TatzelOptimizedTraceTests PRIVATE -O2 -g)
endif()
target_link_libraries(TatzelOptimizedTraceTests PRIVATE TatzelStacktrace)
add_test(NAME TatzelOptimizedTraceTests COMMAND TatzelOptimizedTraceTests)

option(TATZEL_BUILD_SANITIZER_TESTS "Instrument logical removal tests with address/undefined sanitizers" OFF)
if(TATZEL_BUILD_SANITIZER_TESTS)
	file(GLOB_RECURSE _LOGICAL_MODULES CONFIGURE_DEPENDS
		"${Tatzel_SOURCE_DIR}/targets/tatzel/public/element/*.ixx"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/public/dom/*.ixx"
	)
	add_executable(TatzelRemovalSanitizerTests
		Removal.cpp
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/ClientElementTree.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Diagnostics.cpp"
	)
	target_sources(TatzelRemovalSanitizerTests PRIVATE
		FILE_SET logical_modules TYPE CXX_MODULES
		BASE_DIRS "${Tatzel_SOURCE_DIR}/targets/tatzel/public"
		FILES
			${_LOGICAL_MODULES}
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/ClientElementTree.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Diagnostics.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/ResourceURL.ixx"
	)
	target_link_libraries(TatzelRemovalSanitizerTests PRIVATE TatzelStacktrace)
	target_compile_options(TatzelRemovalSanitizerTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(TatzelRemovalSanitizerTests PRIVATE -fsanitize=address,undefined)
	add_test(NAME TatzelRemovalSanitizerTests COMMAND TatzelRemovalSanitizerTests)
endif()

add_executable(TatzelElementTests Elements.cpp)
target_link_libraries(TatzelElementTests PRIVATE Tatzel)
add_test(NAME TatzelElementTests COMMAND TatzelElementTests)
