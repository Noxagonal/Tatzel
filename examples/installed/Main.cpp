#include <exception>
#include <memory>
#include <string>
#include <utility>

import UI;

/// @brief Runs a standalone application using the installed Tatzel package.
/// @param argc Process argument count; an optional first argument selects the runtime root.
/// @param argv Process arguments, borrowed for startup.
/// @return Zero on normal shutdown, or one after a reported failure.
/// @pre The matching runtime is deployed beside this executable or explicitly configured.
/// @post The server has stopped and its per-client controls are released.
auto main(
	int argc,
	char** argv
) -> int
try
{
	if( argc > 2 ) throw tatzel::Exception{ "Usage: TatzelInstalledExample [runtime-directory]" };
	auto options = tatzel::AppOptions{};
	if( argc == 2 ) options.resource_directory = argv[ 1 ];
	auto background = tatzel::TaskQueue{};
	auto app = tatzel::App{ std::move( options ) };
	app.RegisterPage( "/", [ &background ]( tatzel::PageBuilder<> ui ) {
		ui.Heading( "Tatzel package sample" );
		auto count = std::make_shared<int>( 0 );
		auto counter = ui.Paragraph( "Count: 0" )->GetHandle();
		ui.Button( "Increment", [ count, counter ] {
			if( auto* text = counter.Get() ) text->SetText( "Count: " + std::to_string( ++*count ) );
		} );

		auto name = ui.TextInput( "Your name" )->GetHandle();
		auto greeting = ui.Paragraph( "Enter your name." )->GetHandle();
		ui.Button( "Greet", [ name, greeting ] {
			auto* input = name.Get();
			auto* output = greeting.Get();
			if( input && output ) output->SetText( "Hello, " + std::string{ input->GetValue() } + "!" );
		} );

		auto status = ui.Paragraph( "Background queue ready." )->GetHandle();
		auto dispatcher = ui.GetDispatcher();
		ui.Button( "Run background task", [ &background, status, dispatcher ] {
			const auto submitted = background.Post( [ status, dispatcher ] {
				dispatcher.Post( [ status ] {
					if( auto* output = status.Get() ) output->SetText( "Background task complete." );
				} );
			} );
			if( submitted != tatzel::TaskQueueResult::Accepted )
				if( auto* output = status.Get() ) output->SetText( "Background queue busy; retry." );
		} );
	} );
	app.Run();
	return 0;
}
catch( ... )
{
	tatzel::LogException( std::current_exception(), { {}, "installed_sample" } );
	return 1;
}
