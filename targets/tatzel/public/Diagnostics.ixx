/// @file
/// @brief Exception traces bounded by the innermost application callback.
module;

#include <cstddef>
#include <source_location>
#include <stacktrace>
#include <stdexcept>
#include <string>
#include <string_view>

export module UI.Diagnostics;

namespace tatzel {

/// @brief Marks a synchronous application invocation on the current thread.
/// @details Nested scopes restore their predecessor, including during unwinding.
/// The captured call stack locates the outer boundary without inspecting symbol names.
export class CallbackScope
{
public:
	/// @brief Records where framework code is about to invoke application code.
	/// @param name Borrowed description, which outlives this scope.
	/// @param boundary Call-site stack captured before the application invocation.
	/// @pre Scopes are destroyed in reverse construction order on the same thread.
	/// @post Exceptions constructed inside the scope retain its name and trace boundary.
	CallbackScope(
		std::string_view name,
		std::stacktrace boundary = std::stacktrace::current( 0, 128 )
	);

	/// @brief Prevents duplication of an active thread-local boundary.
	/// @param other Source scope.
	/// @pre None.
	/// @post Copy construction is unavailable.
	CallbackScope( const CallbackScope& other ) = delete;

	/// @brief Keeps the boundary at its original stack address.
	/// @param other Source scope.
	/// @pre None.
	/// @post Move construction is unavailable.
	CallbackScope( CallbackScope&& other ) = delete;

	/// @brief Restores the enclosing callback boundary without throwing.
	/// @pre This is the current scope on its construction thread.
	/// @post The previous callback boundary is active.
	~CallbackScope();

	/// @brief Prevents reassignment of a thread-local boundary.
	/// @param other Source scope.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const CallbackScope& other ) -> CallbackScope& = delete;

	/// @brief Keeps a boundary attached to its lexical lifetime.
	/// @param other Source scope.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( CallbackScope&& other ) -> CallbackScope& = delete;

private:
	friend class Exception;
	std::string_view name;
	std::stacktrace boundary;
	const CallbackScope* previous;
};

/// @brief Runtime error retaining its construction site and pre-unwind standard stacktrace.
/// @details Construct at the throw expression. Default reports stop at the active
/// callback invocation; the bounded raw trace remains available for inspection.
/// Native stack capture and symbol/line resolution are best effort, including in optimized builds.
export class Exception : public std::runtime_error
{
public:
	/// @brief Captures diagnostics while the failing application stack still exists.
	/// @param message Owned copy of the failure description.
	/// @param location Construction site, normally supplied by the language.
	/// @param trace Construction-site stack, captured before unwinding and bounded to 128 entries by default.
	/// @pre Construct on the thread whose callback is failing; supplied trace belongs to that thread.
	/// @post Message, location, trace, and callback metadata survive scope unwinding and rethrow.
	Exception(
		std::string_view message,
		std::source_location location = std::source_location::current(),
		std::stacktrace trace = std::stacktrace::current( 0, 128 )
	);

	/// @brief Reads the original exception construction site.
	/// @return Stable source-location metadata.
	/// @pre None.
	/// @post The exception is unchanged.
	auto GetLocation() const noexcept -> std::source_location { return location; }

	/// @brief Reads the bounded raw trace, including framework frames.
	/// @return Borrowed immutable captured stack.
	/// @pre This exception remains alive while the reference is used.
	/// @post No stack capture or symbol resolution occurs.
	auto GetStackTrace() const noexcept -> const std::stacktrace& { return trace; }

	/// @brief Reads the prefix ending at the application invocation boundary.
	/// @return Displayable frame count; zero when a callback boundary cannot be matched safely.
	/// @pre None.
	/// @post The exception is unchanged.
	auto GetVisibleFrameCount() const noexcept -> size_t { return visible_frames < trace.size() ? visible_frames : trace.size(); }

	/// @brief Reads the innermost application invocation description.
	/// @return Borrowed name, or empty outside an application callback.
	/// @pre This exception remains alive while the view is used.
	/// @post The exception is unchanged.
	auto GetCallbackName() const noexcept -> std::string_view { return callback_name; }

	/// @brief Reports whether native frame identities located the callback boundary.
	/// @return Whether the display prefix was bounded successfully.
	/// @pre None.
	/// @post The exception is unchanged.
	auto HasCallbackBoundary() const noexcept -> bool { return boundary_found; }

private:
	std::source_location location;
	std::stacktrace trace;
	std::string callback_name;
	size_t visible_frames = 0;
	bool boundary_found = false;
};

}
