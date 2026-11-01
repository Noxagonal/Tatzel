#include <memory>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

import UI;

/// @brief Provides an application helper frame for the traced failure example.
/// @pre Called from the lifecycle button callback.
/// @post Never returns; the exception retains the helper and callback stack.
/// @throws tatzel::Exception Always, to exercise diagnostic reporting after removal.
auto FailLifecycleAction() -> void
{
	throw tatzel::Exception{ "Lifecycle example callback failed after removal" };
}

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

	app.RegisterPage( "/lifecycle", []( tatzel::PageBuilder<> ui ) {
		ui.Heading( "Element lifecycle" );
		auto status = ui.Text( "Ready" )->GetHandle();
		auto* list = ui.Column( []( auto children ) {} );
		auto list_handle = list->GetHandle();
		auto items = ui.Scope( *list );
		auto next_item = std::make_shared<int>( 0 );

		ui.Button( "Add item", [ items, next_item, status ]() mutable {
			const auto number = ++*next_item;
			items.Row( [ number, status ](
				auto children,
				tatzel::ui::Row* row
			) {
				children.Text( "Item " + std::to_string( number ) );
				auto item = row->GetHandle();
				children.Button( "Remove item " + std::to_string( number ), [ item, status ] {
					if( auto* row = item.Get() ) row->Remove();
					if( auto* text = status.Get() ) text->SetText( "Item removed" );
				} );
			}, { .alignment = tatzel::ui::Alignment::Center, .wrap = true } );
			if( auto* text = status.Get() ) text->SetText( "Added item " + std::to_string( number ) );
		} );
		ui.Button( "Clear items", [ list_handle, status ] {
			if( auto* list = list_handle.Get() ) list->ClearChildren();
			if( auto* text = status.Get() ) text->SetText( "Items cleared" );
		} );

		ui.Card( "Preserved card", []( auto content ) {
			content.Paragraph( "Disposable card content" );
		}, [](
			auto actions,
			tatzel::ui::Card* card
		) {
			auto handle = card->GetHandle();
			actions.Button( "Clear card content", [ handle ] {
				if( auto* card = handle.Get() ) card->ClearChildren( tatzel::ui::ChildSlot::Content );
			} );
		} );

		auto expired_builder = std::make_shared<std::function<void()>>();
		auto* removed_during_build = ui.Container( [ expired_builder ](
			auto children,
			tatzel::ui::Container* container
		) {
			*expired_builder = [ children ]() mutable { children.Text( "Unreachable" ); };
			container->Remove();
		} );
		assert( removed_during_build == nullptr );
		auto actions_called = false;
		auto* removed_card = ui.Card( "Retired during construction", [](
			auto content,
			tatzel::ui::Card* card
		) { card->Remove(); }, [ &actions_called ]( auto actions ) { actions_called = true; } );
		assert( removed_card == nullptr && !actions_called );
		ui.Button( "Try retired builder", [ expired_builder, status ] {
			try { ( *expired_builder )(); }
			catch( const std::logic_error& )
			{
				if( auto* text = status.Get() ) text->SetText( "Retired builder rejected" );
			}
		} );

		ui.Container( [ status ](
			auto children,
			tatzel::ui::Container* group
		) {
			children.Text( "Disposable group" );
			auto handle = group->GetHandle();
			children.Button( "Remove group", [ handle, status ] {
				if( auto* group = handle.Get() ) group->Remove();
				if( auto* text = status.Get() ) text->SetText( "Group removed" );
			} );
		} );

		ui.Form( "Removable field", [ status ](
			auto fields,
			tatzel::ui::Form* form
		) {
			auto* field = fields.TextInput( "Temporary required field" );
			field->SetRequired( true );
			auto handle = field->GetHandle();
			fields.Button( "Remove field", [ handle ] { if( auto* field = handle.Get() ) field->Remove(); } );
			fields.SubmitButton( "Submit remaining fields" );
			form->SetOnSubmit( [ status, handle ] {
				if( auto* text = status.Get() ) text->SetText( handle ? "Submitted with field" : "Submitted without field" );
			} );
		} );

		auto* modal = ui.Modal( "Disposable modal", [ status ](
			auto content,
			tatzel::ui::Modal* modal
		) {
			auto handle = modal->GetHandle();
			content.Button( "Remove this modal", [ handle, status ] {
				if( auto* modal = handle.Get() ) modal->Remove();
				if( auto* text = status.Get() ) text->SetText( "Modal removed" );
			} );
		} );
		modal->SetOnClose( [ status ] { if( auto* text = status.Get() ) text->SetText( "Modal closed" ); } );
		auto modal_handle = modal->GetHandle();
		ui.Button( "Open disposable modal", [ modal_handle, status ] {
			if( auto* modal = modal_handle.Get() ) modal->Open();
			else if( auto* text = status.Get() ) text->SetText( "Modal already removed" );
		} );

		auto* throwing = ui.Button( "Remove then throw", {} );
		auto throwing_handle = throwing->GetHandle();
		throwing->SetOnClick( [ throwing_handle, status ] {
			if( auto* button = throwing_handle.Get() ) button->Remove();
			if( auto* text = status.Get() ) text->SetText( "Removed before error" );
			FailLifecycleAction();
		} );
	} );

	app.Run();

	return 0;
}
