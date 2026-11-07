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

# Backend integration tests need the same native event-loop/logger instance as
# the code under test. The installed shared library deliberately hides that instance.
add_library(TatzelNativeTestBackend STATIC)
set_default_target_files(TatzelNativeTestBackend "${Tatzel_SOURCE_DIR}/targets/tatzel")
target_include_directories(TatzelNativeTestBackend PRIVATE "${Tatzel_BINARY_DIR}/targets/tatzel/tatzel-generated")
target_link_libraries(TatzelNativeTestBackend PUBLIC TatzelStacktrace PRIVATE Drogon::Drogon)

add_executable(TatzelDiagnosticTests Diagnostics.cpp)
target_link_libraries(TatzelDiagnosticTests PRIVATE TatzelNativeTestBackend Drogon::Drogon)
add_test(NAME TatzelDiagnosticTests COMMAND TatzelDiagnosticTests)

add_executable(TatzelDispatcherTests Dispatcher.cpp)
target_link_libraries(TatzelDispatcherTests PRIVATE TatzelNativeTestBackend Drogon::Drogon)
add_test(NAME TatzelDispatcherTests COMMAND TatzelDispatcherTests)
set_tests_properties(TatzelDispatcherTests PROPERTIES TIMEOUT 15)

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

option(TATZEL_BUILD_SANITIZER_TESTS "Instrument logical removal and dispatcher tests with address/undefined sanitizers" OFF)
if(TATZEL_BUILD_SANITIZER_TESTS)
	file(GLOB_RECURSE _LOGICAL_MODULES CONFIGURE_DEPENDS
		"${Tatzel_SOURCE_DIR}/targets/tatzel/public/element/*.ixx"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/public/dom/*.ixx"
	)
	file(GLOB_RECURSE _LOGICAL_IMPLEMENTATIONS CONFIGURE_DEPENDS
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/element/*.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/dom/*.cpp"
	)
	list(APPEND _LOGICAL_IMPLEMENTATIONS
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/ResourceURL.cpp"
	)
	add_executable(TatzelRemovalSanitizerTests
		Removal.cpp
		${_LOGICAL_IMPLEMENTATIONS}
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
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Protocol.ixx"
	)
	target_link_libraries(TatzelRemovalSanitizerTests PRIVATE TatzelStacktrace)
	target_compile_options(TatzelRemovalSanitizerTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(TatzelRemovalSanitizerTests PRIVATE -fsanitize=address,undefined)
	add_test(NAME TatzelRemovalSanitizerTests COMMAND TatzelRemovalSanitizerTests)

	add_executable(TatzelDispatcherSanitizerTests
		Dispatcher.cpp
		${_LOGICAL_IMPLEMENTATIONS}
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/ClientElementTree.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Diagnostics.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Logging.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Dispatcher.cpp"
	)
	target_sources(TatzelDispatcherSanitizerTests PRIVATE
		FILE_SET dispatcher_modules TYPE CXX_MODULES
		BASE_DIRS "${Tatzel_SOURCE_DIR}/targets/tatzel/public"
		FILES
			${_LOGICAL_MODULES}
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/ClientElementTree.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Diagnostics.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Logging.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Dispatcher.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/ResourceURL.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Protocol.ixx"
	)
	target_link_libraries(TatzelDispatcherSanitizerTests PRIVATE TatzelStacktrace Drogon::Drogon)
	target_compile_options(TatzelDispatcherSanitizerTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(TatzelDispatcherSanitizerTests PRIVATE -fsanitize=address,undefined)
	add_test(NAME TatzelDispatcherSanitizerTests COMMAND TatzelDispatcherSanitizerTests)
	set_tests_properties(TatzelDispatcherSanitizerTests PROPERTIES TIMEOUT 20)

	add_executable(TatzelTaskQueueSanitizerTests
		TaskQueue.cpp
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/TaskQueue.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Diagnostics.cpp"
		"${Tatzel_SOURCE_DIR}/targets/tatzel/private/Logging.cpp")
	target_sources(TatzelTaskQueueSanitizerTests PRIVATE
		FILE_SET task_modules TYPE CXX_MODULES
		BASE_DIRS "${Tatzel_SOURCE_DIR}/targets/tatzel/public"
		FILES
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/TaskQueue.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Diagnostics.ixx"
			"${Tatzel_SOURCE_DIR}/targets/tatzel/public/Logging.ixx")
	target_link_libraries(TatzelTaskQueueSanitizerTests PRIVATE TatzelStacktrace Drogon::Drogon)
	target_compile_options(TatzelTaskQueueSanitizerTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(TatzelTaskQueueSanitizerTests PRIVATE -fsanitize=address,undefined)
	add_test(NAME TatzelTaskQueueSanitizerTests COMMAND TatzelTaskQueueSanitizerTests)
	set_tests_properties(TatzelTaskQueueSanitizerTests PROPERTIES TIMEOUT 20)
endif()

add_executable(TatzelElementTests Elements.cpp)
target_link_libraries(TatzelElementTests PRIVATE Tatzel)
add_test(NAME TatzelElementTests COMMAND TatzelElementTests)

add_executable(TatzelProtocolTests Protocol.cpp)
target_link_libraries(TatzelProtocolTests PRIVATE Tatzel Drogon::Drogon)
add_test(NAME TatzelProtocolTests COMMAND TatzelProtocolTests)

add_executable(TatzelComponentTests Components.cpp)
target_link_libraries(TatzelComponentTests PRIVATE Tatzel Drogon::Drogon)
add_test(NAME TatzelComponentTests COMMAND TatzelComponentTests)

add_executable(TatzelTableTests Tables.cpp)
target_link_libraries(TatzelTableTests PRIVATE Tatzel Drogon::Drogon)
add_test(NAME TatzelTableTests COMMAND TatzelTableTests)

add_executable(TatzelTaskQueueTests TaskQueue.cpp)
target_link_libraries(TatzelTaskQueueTests PRIVATE Tatzel)
add_test(NAME TatzelTaskQueueTests COMMAND TatzelTaskQueueTests)
set_tests_properties(TatzelTaskQueueTests PROPERTIES TIMEOUT 20)
