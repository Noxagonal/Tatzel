/// @file
/// @brief Application logging and exception reports using Drogon's existing backend.
module;

#include <exception>
#include <source_location>
#include <string_view>

export module UI.Logging;

namespace tatzel {

/// @brief Runtime verbosity ordered from most detailed to most severe.
export enum class LogLevel
{
	Trace, ///< Fine-grained diagnostics.
	Debug, ///< Development and invocation details.
	Info, ///< Startup, shutdown, and normal operational milestones.
	Warning, ///< A recoverable operational problem.
	Error, ///< An operation failed.
};

/// @brief Borrowed event context captured before invoking application code.
/// @details Values identify an operation without copying browser field values into logs.
export struct ErrorContext
{
	std::string_view route; ///< Registered logical route, when available.
	std::string_view operation; ///< Browser operation or framework lifecycle phase.
	std::string_view element_id; ///< Logical owner captured before callback removal.
	std::string_view part_id; ///< Exact browser event source, when available.
};

/// @brief Changes the shared Tatzel/Drogon logger's runtime threshold.
/// @param level Minimum severity to emit, including in release builds.
/// @pre Configure before serving requests; concurrent reconfiguration is unsupported.
/// @post Both Tatzel and Drogon use the selected verbosity.
export auto SetLogLevel( LogLevel level ) noexcept -> void;

/// @brief Tests the shared backend's current severity threshold.
/// @param level Severity of the proposed message.
/// @return Whether a Tatzel message at this severity would be emitted.
/// @pre No concurrent logger reconfiguration occurs.
/// @post No message is formatted or emitted.
export auto IsLogEnabled( LogLevel level ) noexcept -> bool;

/// @brief Writes through the existing logger with the application's source location.
/// @param level Message severity.
/// @param message Borrowed text consumed synchronously, including messages larger than the backend's small buffer.
/// @param location Logging call site, normally supplied by the language.
/// @pre The backend's installed output/flush callbacks do not throw.
/// @post Enabled messages are emitted; reporting failures fall back to a minimal stderr diagnostic.
export auto Log(
	LogLevel level,
	std::string_view message,
	std::source_location location = std::source_location::current()
) noexcept -> void;

/// @brief Logs a caught exception and bounded nested causes without allowing reporting to escape.
/// @param exception Retained exception to inspect; null reports an unknown failure.
/// @param context Borrowed operation metadata captured before invoking application code.
/// @param location Catch/report site, kept distinct from a traced exception's construction site.
/// @pre Context views remain valid for this call; the backend's callbacks do not throw.
/// @post Error-level output includes an error ID, available exception details, and callback-bounded traces.
export auto LogException(
	std::exception_ptr exception,
	const ErrorContext& context = {},
	std::source_location location = std::source_location::current()
) noexcept -> void;

}
