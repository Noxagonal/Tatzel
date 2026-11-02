#include "Showcases.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace example {

auto BuildDispatcherShowcase(
	tatzel::PageBuilder<> ui,
	trantor::ConcurrentTaskQueue& background
) -> void
{
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
}

}
