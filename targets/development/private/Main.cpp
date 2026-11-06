#include "showcases/Showcases.hpp"

#include <exception>

/// @brief Runs the development showcase server.
/// @return Zero after normal shutdown, or one after a reported application/startup failure.
/// @pre Runtime configuration, template, and compiled browser assets are available.
/// @post The application server has stopped.
auto main() -> int
try
{
	auto background = tatzel::TaskQueue{ { .worker_count = 2 } };
	auto app = tatzel::App{};

	example::RegisterShowcases( app, background );
	app.Run();

	return 0;
}
catch( ... )
{
	tatzel::LogException( std::current_exception(), { {}, "application" } );
	return 1;
}
