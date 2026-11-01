module;

#include <trantor/utils/Logger.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#if defined( __GNUG__ )
#include <cxxabi.h>
#endif

module UI.Logging;

import UI.Diagnostics;

namespace {

std::atomic<unsigned long long> next_error_id{ 1 };

/// @brief Maps public severities to the existing backend without exposing its headers.
/// @param level Public message severity.
/// @return Corresponding Trantor severity.
/// @pre level is a defined LogLevel value.
/// @post No logger state is changed.
auto BackendLevel( tatzel::LogLevel level ) noexcept -> trantor::Logger::LogLevel
{
	switch( level )
	{
	case tatzel::LogLevel::Trace: return trantor::Logger::kTrace;
	case tatzel::LogLevel::Debug: return trantor::Logger::kDebug;
	case tatzel::LogLevel::Info: return trantor::Logger::kInfo;
	case tatzel::LogLevel::Warning: return trantor::Logger::kWarn;
	case tatzel::LogLevel::Error: return trantor::Logger::kError;
	}
	return trantor::Logger::kError;
}

/// @brief Preserves a minimal diagnostic when richer exception formatting fails.
/// @pre None.
/// @post A best-effort message is written and flushed directly to stderr.
auto ReportingFailed() noexcept -> void
{
	std::fputs( "Tatzel: diagnostic reporting failed\n", stderr );
	std::fflush( stderr );
}

/// @brief Obtains a readable dynamic exception type when the ABI supports demangling.
/// @param error Caught standard exception.
/// @return Owned readable type name, falling back to the ABI name.
/// @pre error remains alive throughout the call.
/// @post No exception state is changed.
auto ExceptionType( const std::exception& error ) -> std::string
{
	if( typeid( error ) == typeid( tatzel::Exception ) ) return "tatzel::Exception";
#if defined( __GNUG__ )
	auto status = 0;
	auto name = std::unique_ptr<char, decltype( &std::free )>{
		abi::__cxa_demangle( typeid( error ).name(), nullptr, nullptr, &status ), &std::free };
	if( status == 0 && name ) return name.get();
#endif
	return typeid( error ).name();
}

/// @brief Keeps compiler-generated function descriptions readable in console reports.
/// @param description Complete native function description.
/// @return Readable description abbreviated to 160 characters, or an unresolved-symbol label.
/// @pre None.
/// @post Raw stack entries remain unchanged and available for inspection.
auto ShortSymbol( std::string_view description ) -> std::string
{
	if( description.starts_with( "_Z" ) ) return "<unresolved C++ symbol>";
	if( description.size() <= 160 ) return std::string{ description };
	return std::string{ description.substr( 0, 157 ) } + "...";
}

/// @brief Recognizes standard-library callable plumbing for presentation only.
/// @param description Resolved native function description.
/// @param file Resolved source path for that same frame.
/// @return Whether the frame is a known libstdc++ or libc++ invocation wrapper.
/// @pre Both views remain valid throughout the call.
/// @post No frame identities or callback boundaries are changed.
auto IsCallableDispatch(
	std::string_view description,
	std::string_view file
) -> bool
{
	if( file.ends_with( "/bits/invoke.h" ) ) return description.contains( "std::__invoke" );
	if( file.ends_with( "/bits/std_function.h" ) )
		return description.contains( "std::_Function_handler<" ) || description.starts_with( "std::function<" );
	if( file.ends_with( "/__functional/invoke.h" ) || file.ends_with( "/__functional/function.h" ) )
		return description.starts_with( "std::__1::" ) || description.contains( " std::__1::" );
	return false;
}

/// @brief Formats one original trace and its application invocation boundary.
/// @param error Traced exception captured before unwinding.
/// @param report Owned output stream receiving source and trace details.
/// @pre error remains alive; symbol resolution is allowed to allocate.
/// @post Only the application prefix is formatted when a callback boundary exists.
auto AppendTrace(
	const tatzel::Exception& error,
	std::ostringstream& report
) -> void
{
	const auto location = error.GetLocation();
	report << "\n  Thrown at: " << location.file_name() << ':' << location.line()
		<< "\n  Function: " << ShortSymbol( location.function_name() );
	if( !error.GetCallbackName().empty() ) report << "\n  Callback: " << error.GetCallbackName();
	if( error.GetStackTrace().empty() )
	{
		report << "\n  Stack trace unavailable";
		return;
	}
	if( !error.GetCallbackName().empty() && !error.HasCallbackBoundary() )
	{
		report << "\n  Callback boundary unavailable; raw framework frames omitted";
		return;
	}
	auto displayed = size_t{ 0 };
	report << "\n  Stack trace:";
	for( size_t index = 0; index < error.GetVisibleFrameCount(); ++index )
	{
		const auto& frame = error.GetStackTrace()[ index ];
		auto description = frame.description();
		auto file = frame.source_file();
		if( IsCallableDispatch( description, file ) ) continue;
		report << "\n    #" << displayed++ << ' ';
		if( description.empty() ) report << "<unresolved frame>";
		else report << ShortSymbol( description );
		if( !file.empty() ) report << "\n       " << file << ':' << frame.source_line();
	}
	if( error.HasCallbackBoundary() ) report << "\n    -- application callback invocation boundary --";
}

/// @brief Formats standard, traced, nested, and nonstandard exceptions with a recursion bound.
/// @param exception Retained exception being reported.
/// @param report Owned output stream receiving details.
/// @param depth Current cause depth, initially zero.
/// @pre report can allocate; callers contain any diagnostic failure.
/// @post Up to eight nested causes are described; ordinary exceptions get no fabricated throw trace.
auto AppendException(
	std::exception_ptr exception,
	std::ostringstream& report,
	unsigned depth
) -> void
{
	if( !exception ) { report << "\n  Exception: unknown failure"; return; }
	if( depth == 8 ) { report << "\n  Further nested causes omitted"; return; }
	try { std::rethrow_exception( exception ); }
	catch( const std::exception& error )
	{
		report << ( depth ? "\n  Caused by: " : "\n  Exception: " ) << ExceptionType( error )
			<< "\n  Message: " << error.what();
		if( auto* traced = dynamic_cast<const tatzel::Exception*>( &error ) ) AppendTrace( *traced, report );
		if( auto* nested = dynamic_cast<const std::nested_exception*>( &error ); nested && nested->nested_ptr() )
			AppendException( nested->nested_ptr(), report, depth + 1 );
	}
	catch( ... ) { report << "\n  Exception: nonstandard thrown value (no standard message)"; }
}

}

auto tatzel::SetLogLevel( LogLevel level ) noexcept -> void
{
	trantor::Logger::setLogLevel( BackendLevel( level ) );
}

auto tatzel::IsLogEnabled( LogLevel level ) noexcept -> bool
{
	return trantor::Logger::logLevel() <= BackendLevel( level );
}

auto tatzel::Log(
	LogLevel level,
	std::string_view message,
	std::source_location location
) noexcept -> void
{
	if( !IsLogEnabled( level ) ) return;
	try
	{
		{
			auto logger = trantor::Logger{ trantor::Logger::SourceFile{ location.file_name() },
				static_cast<int>( location.line() ), BackendLevel( level ) };
			if( !message.empty() ) logger.stream().append( message.data(), message.size() );
		}
		// The default sink uses stdout, which is block-buffered when captured by a service or test.
		if( level >= LogLevel::Info ) std::fflush( stdout );
	}
	catch( ... ) { ReportingFailed(); }
}

auto tatzel::LogException(
	std::exception_ptr exception,
	const ErrorContext& context,
	std::source_location location
) noexcept -> void
{
	if( !IsLogEnabled( LogLevel::Error ) ) return;
	try
	{
		const auto error_id = next_error_id.fetch_add( 1, std::memory_order_relaxed );
		auto report = std::ostringstream{};
		report << "Tatzel error #" << error_id;
		if( !context.route.empty() ) report << "\n  Route: " << context.route;
		if( !context.operation.empty() ) report << "\n  Operation: " << context.operation;
		if( !context.element_id.empty() ) report << "\n  Element: " << context.element_id;
		if( !context.part_id.empty() ) report << "\n  Part: " << context.part_id;
		report << "\n  Reported at: " << location.file_name() << ':' << location.line();
		AppendException( exception, report, 0 );
		Log( LogLevel::Error, report.str(), location );
	}
	catch( ... ) { ReportingFailed(); }
}
