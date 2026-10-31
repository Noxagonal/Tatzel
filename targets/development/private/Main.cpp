#include <memory>
#include <string>

import UI;

/// @brief Runs an interactive example using logical controls.
/// @return Zero after the server stops.
/// @pre Runtime configuration, template, and compiled browser assets are available.
/// @post The application server has stopped.
auto main() -> int
{
	auto app = tatzel::App{};
	app.RegisterPage( "/", []( tatzel::PageBuilder<> ui ) {
		auto count = std::make_shared<int>( 0 );
		auto* heading = ui.Heading( "Clicked times: 0" );
		ui.Container( [ heading, count ]( auto children ) {
			children.Button( "Click me", [ heading, count ] {
				heading->SetText( "Clicked times: " + std::to_string( ++*count ) );
			} );
		} );
	} );
	app.Run();
	return 0;
}
