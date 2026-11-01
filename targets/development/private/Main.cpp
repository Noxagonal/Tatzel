#include <memory>
#include <optional>
#include <cassert>
#include <functional>
#include <exception>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <thread>
#include <utility>
#include <trantor/utils/ConcurrentTaskQueue.h>

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
/// @return Zero after normal shutdown, or one after a reported application/startup failure.
/// @pre Runtime configuration, template, and compiled browser assets are available.
/// @post The application server has stopped.
auto main() -> int
try
{
	auto background = trantor::ConcurrentTaskQueue{ 2, "Tatzel example" };
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

	app.RegisterPage( "/elements", []( tatzel::PageBuilder<> ui ) {
		ui.Heading( "Numbers, choices, and resources" );
		auto* status = ui.Paragraph( "Ready" );
		auto* link = ui.Link( "Tatzel home", "/", tatzel::ui::LinkTarget::NewTab );
		ui.Paragraph( "A local image with an explicit description:" );
		auto* image = ui.Image( "/framework/example-image.svg", "Tatzel mountain landscape" );
		image->SetSize( 320, 160 );
		image->SetLazyLoading( true );

		ui.Form( "Order", [ status, link, image ](
			auto fields,
			tatzel::ui::Form* form
		) {
			auto* amount = fields.NumberInput( "Amount", 1.5 );
			amount->SetRequired( true );
			amount->SetRange( 0, 100 );
			amount->SetStep( 0.5 );
			amount->SetOnInput( [ status ]( std::optional<double> value ) {
				status->SetText( value ? "Editing: " + std::to_string( *value ) : "Editing: empty or invalid" );
			} );
			auto* delivery = fields.RadioGroup( "Delivery", {
				{ "pickup", "Pickup" }, { "courier", "Courier" }, { "post", "Post (unavailable)", true }
			} );
			delivery->SetRequired( true );
			delivery->SetOnChange( [ status ]( std::string_view value ) {
				status->SetText( "Choice: " + std::string{ value } );
			} );
			fields.RadioGroup( "Packaging", { { "box", "Box" }, { "bag", "Bag" } }, "box" );
			form->SetOnSubmit( [ amount, delivery, status ] {
				auto value = amount->GetValue();
				status->SetText( "Ordered: " + ( value ? std::to_string( *value ) : std::string{ "none" } ) + " | " + std::string{ delivery->GetValue() } );
			} );
			form->SetOnInvalid( [ status ] { status->SetText( "Check highlighted fields" ); } );
			fields.SubmitButton( "Place order" );
			fields.Row( [ amount, delivery, link, image ]( auto actions ) {
				actions.Button( "Reset elements", [ amount, delivery, link, image ] {
					amount->SetValue( std::nullopt );
					delivery->SetValue( "" );
					link->SetText( "Form example" );
					link->SetURL( "/forms" );
					link->SetTarget( tatzel::ui::LinkTarget::SameContext );
					image->SetAlternativeText( "" );
					image->SetSize( std::nullopt, std::nullopt );
					image->SetLazyLoading( false );
				} );
				actions.Button( "Refresh radios", [ delivery ] {
					delivery->SetOptions( { { "pickup", "Collect locally" }, { "bike", "Bike courier" } } );
				} );
				actions.Button( "Toggle fields", [ amount, delivery ] {
					amount->SetDisabled( !amount->IsDisabled() );
					delivery->SetDisabled( !delivery->IsDisabled() );
				} );
				actions.Button( "Toggle read only", [ amount ] { amount->SetReadOnly( !amount->IsReadOnly() ); } );
			}, { .gap = tatzel::ui::Spacing::Small, .wrap = true } );
		} );
	});

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
			catch( const tatzel::Exception& )
			{
				tatzel::LogException( std::current_exception(), { "/lifecycle", "retired_builder" } );
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

		ui.Button( "Throw standard exception", [] {
			auto values = std::vector<int>{};
			static_cast<void>( values.at( 0 ) );
		} );
		auto* throwing = ui.Button( "Remove then throw", {} );
		auto throwing_handle = throwing->GetHandle();
		throwing->SetOnClick( [ throwing_handle, status ] {
			if( auto* button = throwing_handle.Get() ) button->Remove();
			if( auto* text = status.Get() ) text->SetText( "Removed before error" );
			FailLifecycleAction();
		} );
	} );

	app.RegisterPage( "/dispatcher", [ &background ]( tatzel::PageBuilder<> ui ) {
		ui.Heading( "Background work" );
		ui.Paragraph( "Submit a background save, remove its result while it is running, or close this page before it completes." );
		auto dispatcher = ui.GetDispatcher();
		auto status = ui.Text( "Ready" )->GetHandle();
		auto result = ui.Text( "No saved result" )->GetHandle();

		ui.Form( "Background save", [ &background, dispatcher, status, result ](
			auto fields,
			tatzel::ui::Form* form
		) {
			auto* name = fields.TextInput( "Name" );
			name->SetRequired( true );
			fields.SubmitButton( "Save in background" );
			auto name_handle = name->GetHandle();
			auto form_handle = form->GetHandle();
			form->SetOnSubmit( [ &background, dispatcher, status, result, name_handle, form_handle ] {
				auto* name = name_handle.Get();
				if( !name ) return;
				auto value = std::string{ name->GetValue() };
				background.runTaskInQueue( [ dispatcher, status, result, form_handle, value = std::move( value ) ] {
					try
					{
						std::this_thread::sleep_for( std::chrono::milliseconds{ 150 } );
						auto message = std::make_unique<std::string>( "Saved: " + value );
						auto posted = dispatcher.Post( [ status, result, form_handle, message = std::move( message ) ] {
							if( auto* form = form_handle.Get() ) form->SetDisabled( false );
							auto* output = result.Get();
							if( output ) output->SetText( *message );
							if( auto* text = status.Get() ) text->SetText( output ? "Save complete" : "Saved after result removal" );
						} );
						if( posted == tatzel::DispatchResult::Closed )
							tatzel::Log( tatzel::LogLevel::Info, "Dispatcher example: completion discarded after page closure" );
						else if( posted == tatzel::DispatchResult::QueueFull )
							tatzel::Log( tatzel::LogLevel::Warning, "Dispatcher example: completion queue full" );
					}
					catch( ... ) { tatzel::LogException( std::current_exception(), { "/dispatcher", "background_save" } ); }
				} );
				if( auto* form = form_handle.Get() ) form->SetDisabled( true );
				if( auto* text = status.Get() ) text->SetText( "Saving" );
			} );
		} );

		ui.Button( "Remove result", [ result ] { if( auto* text = result.Get() ) text->Remove(); } );
		ui.Button( "Throw dispatched exception", [ dispatcher, status ] {
			dispatcher.Post( [] { throw tatzel::Exception{ "Dispatched example failure" }; } );
			dispatcher.Post( [ status ] {
				if( auto* text = status.Get() ) text->SetText( "Recovered after dispatched error" );
			} );
		} );
	} );

	app.Run();

	return 0;
}
catch( ... )
{
	tatzel::LogException( std::current_exception(), { {}, "application" } );
	return 1;
}
