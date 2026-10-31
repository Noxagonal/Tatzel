#include <memory>
#include <string>
#include <string_view>

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

		ui.Paragraph( "Logical controls, browser editing, and compound content areas." );
		auto* status = ui.Text( "Ready" );

		auto* modal = ui.Modal( "Profile preview", []( auto content ) {
			content.Paragraph( "Escape, the backdrop, or the close button dismisses this modal." );
		} );
		modal->SetOnClose( [ status ] { status->SetText( "Preview closed" ); } );

		ui.Card( "Profile", [ status ](
			auto content,
			tatzel::ui::Card* card
		) {
			auto* name = content.TextInput( "Name", "", [ status ]( std::string_view value ) {
				status->SetText( "Committed: " + std::string{ value } );
			} );
			name->SetPlaceholder( "Type your name" );
			name->SetOnInput( [ status ]( std::string_view value ) {
				status->SetText( "Editing: " + std::string{ value } );
			} );
			name->SetOnSubmit( [ status, card ]( std::string_view value ) {
				status->SetText( "Submitted: " + std::string{ value } );
				card->SetTitle( "Profile: " + std::string{ value } );
			} );

			auto* notifications = content.Checkbox( "Enable notifications", false, [ status ]( bool checked ) {
				status->SetText( checked ? "Notifications enabled" : "Notifications disabled" );
			} );
			content.Button( "Reset", [ name, notifications ] {
				name->SetValue( "" );
				notifications->SetChecked( false );
			} );
		}, [ modal ]( auto actions ) {
			actions.Button( "Preview", [ modal ] { modal->Open(); } );
		} );

	} );

	app.Run();

	return 0;
}
