module;

#include <cassert>
#include <source_location>
#include <stacktrace>
#include <string>
#include <string_view>
#include <utility>

module UI.Diagnostics;

namespace {

thread_local const tatzel::CallbackScope* active_callback = nullptr;

}

tatzel::CallbackScope::CallbackScope(
	std::string_view name,
	std::stacktrace boundary
) :
	name{ name },
	boundary{ std::move( boundary ) },
	previous{ active_callback }
{
	active_callback = this;
}

tatzel::CallbackScope::~CallbackScope()
{
	assert( active_callback == this );
	active_callback = previous;
}

tatzel::Exception::Exception(
	std::string_view message,
	std::source_location location,
	std::stacktrace trace
) :
	std::runtime_error{ std::string{ message } },
	location{ location },
	trace{ std::move( trace ) }
{
	if( !active_callback )
	{
		visible_frames = this->trace.size();
		return;
	}

	callback_name = active_callback->name;
	const auto& boundary = active_callback->boundary;
	auto current_end = this->trace.size();
	auto boundary_end = boundary.size();
	while( current_end && boundary_end && this->trace[ current_end - 1 ] == boundary[ boundary_end - 1 ] )
	{
		--current_end;
		--boundary_end;
		boundary_found = boundary_found || static_cast<bool>( this->trace[ current_end ] );
	}
	if( boundary_found ) visible_frames = current_end;
}
