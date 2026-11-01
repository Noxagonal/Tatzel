#include <trantor/utils/Logger.h>
#include <algorithm>
#include <barrier>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <source_location>
#include <stacktrace>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

import UI.Diagnostics;
import UI.Logging;
import UI.ClientElementTree;
import UI.Element.Button;
import UI.App;
import UI.Utility;

/// @brief Throws from an application helper to retain more than one useful frame.
/// @pre A button callback scope is active.
/// @post Never returns; the diagnostic captures this source site before unwinding.
/// @throws tatzel::Exception Always, to exercise pre-unwind capture.
auto ThrowDiagnosticHelper() -> void
{
	throw tatzel::Exception{ "Traced button failure" };
}

/// @brief Exercises a removed callback receiver and returns its retained failure.
/// @return Exception captured after callback, callable, and logical receiver cleanup.
/// @pre Native stack capture is available in the test toolchain.
/// @post The tree is empty and the exception no longer depends on callback scope storage.
auto CaptureButtonFailure() -> std::exception_ptr
{
	auto tree = tatzel::ClientElementTree{};
	tatzel::ui::Button* button = nullptr;
	button = tree.root.InsertChild( std::make_unique<tatzel::ui::Button>( "retired", &tree.root, "Throw", [ &button ] {
		button->Remove();
		ThrowDiagnosticHelper();
	} ) );
	auto handle = button->GetHandle();
	try { button->InvokeOnClick(); }
	catch( ... )
	{
		assert( !handle && tree.root.GetChildren().empty() );
		return std::current_exception();
	}
	throw tatzel::Exception{ "Expected a callback exception" };
}

/// @brief Checks callback trace bounds, nested/thread-local scopes, and backend logging.
/// @return Zero after all assertions pass.
/// @pre Built with assertions and debug symbols; no server is running in this process.
/// @post The logger no longer retains the test output buffer.
auto main() -> int
{
	auto output = std::string{};
	auto writes = 0;
	trantor::Logger::setOutputFunction( [ &output, &writes ](
		const char* message,
		const uint64_t length
	) { ++writes; output.append( message, length ); }, [] {} );
	tatzel::SetLogLevel( tatzel::LogLevel::Info );
	assert( !tatzel::IsLogEnabled( tatzel::LogLevel::Debug ) && tatzel::IsLogEnabled( tatzel::LogLevel::Error ) );
	tatzel::Log( tatzel::LogLevel::Debug, "Filtered detail" );
	assert( output.empty() );
	tatzel::Log( tatzel::LogLevel::Info, "Application logging works" );
	assert( output.contains( "Application logging works" ) && output.contains( "Diagnostics.cpp" ) );
	output.clear();
	tatzel::SetLogLevel( tatzel::LogLevel::Trace );
	tatzel::Log( tatzel::LogLevel::Trace, "Runtime trace detail" );
	assert( output.contains( "Runtime trace detail" ) );
	tatzel::Log( tatzel::LogLevel::Info, std::string( 9000, 'x' ) );
	assert( std::ranges::count( output, 'x' ) >= 9000 );
	output.clear();

	auto failure = CaptureButtonFailure();
	try { std::rethrow_exception( failure ); }
	catch( const tatzel::Exception& error )
	{
		assert( error.GetCallbackName() == "Button click" && error.HasCallbackBoundary() );
		assert( error.GetVisibleFrameCount() > 0 && error.GetVisibleFrameCount() < error.GetStackTrace().size() );
		assert( std::string_view{ error.GetLocation().function_name() }.contains( "ThrowDiagnosticHelper" ) );
		assert( std::string_view{ error.what() } == "Traced button failure" );
	}
	const auto writes_before_report = writes;
	tatzel::LogException( failure, { "/diagnostics", "on_click", "retired", "button-part" } );
	assert( writes == writes_before_report + 1 );
	assert( output.contains( "Traced button failure" ) && output.contains( "tatzel::Exception" ) );
	assert( output.contains( "Route: /diagnostics" ) && output.contains( "Element: retired" ) );
	assert( output.contains( "Thrown at:" ) && output.contains( "Reported at:" ) );
	assert( output.contains( "application callback invocation boundary" ) );
	assert( !output.contains( "__libc_start_main" ) && !output.contains( "_start at" ) );
	assert( !output.contains( "/bits/invoke.h" ) && !output.contains( "/bits/std_function.h" ) );
	output.clear();

	try { tatzel::GetGlobalApp(); assert( false ); }
	catch( const tatzel::Exception& error )
	{
		assert( std::string_view{ error.GetLocation().file_name() }.ends_with( "App.cpp" ) );
	}
	try { tatzel::GetTemplateDocument( "/nonexistent/tatzel/template.html" ); assert( false ); }
	catch( const tatzel::Exception& error )
	{
		assert( std::string_view{ error.GetLocation().file_name() }.ends_with( "Utility.cpp" ) );
	}

	{
		auto outer = tatzel::CallbackScope{ "Outer callback" };
		try
		{
			auto inner = tatzel::CallbackScope{ "Inner callback" };
			throw tatzel::Exception{ "Nested invocation" };
		}
		catch( const tatzel::Exception& error ) { assert( error.GetCallbackName() == "Inner callback" ); }
		auto restored = tatzel::Exception{ "Outer restored" };
		assert( restored.GetCallbackName() == "Outer callback" && restored.HasCallbackBoundary() );
	}
	auto unscoped = tatzel::Exception{ "No callback" };
	assert( unscoped.GetCallbackName().empty() && !unscoped.HasCallbackBoundary() );
	assert( unscoped.GetVisibleFrameCount() == unscoped.GetStackTrace().size() );
	{
		auto scope = tatzel::CallbackScope{ "Unavailable boundary", std::stacktrace{} };
		auto unavailable = tatzel::Exception{ "Source remains useful" };
		assert( !unavailable.HasCallbackBoundary() && unavailable.GetVisibleFrameCount() == 0 );
	}

	auto rendezvous = std::barrier{ 2 };
	auto first_name = std::string{};
	auto second_name = std::string{};
	{
		auto first = std::jthread{ [ & ] {
			auto scope = tatzel::CallbackScope{ "First thread" };
			rendezvous.arrive_and_wait();
			first_name = tatzel::Exception{ "Thread one" }.GetCallbackName();
		} };
		auto second = std::jthread{ [ & ] {
			auto scope = tatzel::CallbackScope{ "Second thread" };
			rendezvous.arrive_and_wait();
			second_name = tatzel::Exception{ "Thread two" }.GetCallbackName();
		} };
	}
	assert( first_name == "First thread" && second_name == "Second thread" );

	try { throw std::logic_error{ "Ordinary failure" }; }
	catch( ... ) { tatzel::LogException( std::current_exception() ); }
	assert( output.contains( "Exception: std::logic_error" ) && output.contains( "Message: Ordinary failure" ) );
	assert( !output.contains( "Thrown at:" ) && !output.contains( "Stack trace:" ) );
	output.clear();
	try { auto values = std::vector<int>{}; static_cast<void>( values.at( 0 ) ); assert( false ); }
	catch( ... ) { tatzel::LogException( std::current_exception(), { "/diagnostics", "stl_test" } ); }
	assert( output.contains( "Exception: std::out_of_range" ) && output.contains( "Message:" ) );
	assert( !output.contains( "Thrown at:" ) && !output.contains( "Stack trace:" ) );
	output.clear();
	try { throw 42; }
	catch( ... ) { tatzel::LogException( std::current_exception() ); }
	assert( output.contains( "nonstandard thrown value" ) );
	output.clear();
	try
	{
		try { throw std::runtime_error{ "Inner cause" }; }
		catch( ... ) { std::throw_with_nested( tatzel::Exception{ "Outer cause" } ); }
	}
	catch( ... ) { tatzel::LogException( std::current_exception() ); }
	assert( output.contains( "Outer cause" ) && output.contains( "Caused by: std::runtime_error" ) && output.contains( "Inner cause" ) );
	trantor::Logger::setOutputFunction( [](
		const char* message,
		const uint64_t length
	) { std::fwrite( message, 1, length, stdout ); }, [] { std::fflush( stdout ); } );
	return 0;
}
