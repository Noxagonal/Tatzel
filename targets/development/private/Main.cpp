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

	app.RegisterPage( "/forms", []( tatzel::PageBuilder<> ui ) {
		ui.Heading( "Contact form" );
		auto* status = ui.Text( "Ready" );

		auto* form = ui.Form( "Contact details", [ status ](
			auto content,
			tatzel::ui::Form* form
		) {
			content.Column( [ form, status ]( auto fields ) {
				auto* name = fields.TextInput( "Name" );
				name->SetRequired( true );
				auto* delivery = fields.Select( "Delivery", {
					{ "email", "Email" }, { "phone", "Phone" }, { "postal", "Mail", true }
				} );
				delivery->SetRequired( true );

				auto* notes = fields.TextArea( "Notes" );
				notes->SetRows( 3 );
				notes->SetPlaceholder( "Add a message" );
				notes->SetValidator( []( std::string_view value ) -> std::string {
					return value.size() > 80 ? "Notes are too long." : "";
				} );
				auto* consent = fields.Checkbox( "I agree to be contacted" );
				consent->SetRequired( true );

				auto* reference = fields.TextInput( "Reference", "TATZEL-001" );
				reference->SetReadOnly( true );
				auto* internal = fields.TextInput( "Internal", "server-only" );
				internal->SetDisabled( true );

				fields.Row( [ delivery ](
					auto actions,
					tatzel::ui::Row* row
				) {
					actions.SubmitButton( "Send" );
					actions.Button( "Use phone", [ delivery ] { delivery->SetValue( "phone" ); } );
					actions.Button( "Refresh choices", [ delivery ] {
						delivery->SetOptions( { { "phone", "Call me" }, { "pickup", "In person" }, { "postal", "Mail", true } } );
					} );
					actions.Button( "Compact layout", [ row ] {
						row->SetGap( tatzel::ui::Spacing::Small );
						row->SetJustification( tatzel::ui::Justification::SpaceBetween );
					} );
				}, { .alignment = tatzel::ui::Alignment::Center, .wrap = true } );

				form->SetOnInvalid( [ status ] { status->SetText( "Check highlighted fields" ); } );
				form->SetOnSubmit( [ status, name, delivery, notes, reference ] {
					status->SetText( "Sent: " + std::string{ name->GetValue() } + " | "
						+ std::string{ delivery->GetValue() } + " | " + std::string{ notes->GetValue() }
						+ " | " + std::string{ reference->GetValue() } );
				} );
			} );
		} );

		ui.Button( "Toggle form", [ form ] { form->SetDisabled( !form->IsDisabled() ); } );
	} );

	app.Run();

	return 0;
}
