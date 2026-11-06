#include "Showcases.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace example {

auto BuildProtocolShowcase(
	tatzel::PageBuilder<> ui,
	trantor::ConcurrentTaskQueue& background
) -> void
{
	ui.Heading( "Protocol reliability" );
	ui.Paragraph( "Type while normalization runs, reset a pending draft, or fail an action after a page update." );
	auto status = ui.Text( "Ready" )->GetHandle();
	auto* input = ui.TextInput( "Draft", "initial" );
	auto input_handle = input->GetHandle();
	auto dispatcher = ui.GetDispatcher();
	input->SetOnInput( [ status ]( std::string_view value ) {
		if( auto* text = status.Get() ) text->SetText( "Draft: " + std::string{ value } );
	} );
	ui.Button( "Normalize later", [ &background, input_handle, status, dispatcher ] {
		auto* input = input_handle.Get();
		if( !input ) return;
		auto revision = input->GetEditRevision();
		auto value = std::string{ input->GetValue() };
		background.runTaskInQueue( [ input_handle, status, dispatcher, revision, value = std::move( value ) ] {
			std::this_thread::sleep_for( std::chrono::milliseconds{ 150 } );
			dispatcher.Post( [ input_handle, status, revision, value ] {
				auto* input = input_handle.Get();
				const auto applied = input && input->TrySetValue( "Normalized: " + value, revision );
				if( auto* text = status.Get() ) text->SetText( applied ? "Normalization applied" : "Stale result ignored" );
			} );
		} );
		if( auto* text = status.Get() ) text->SetText( "Normalizing" );
	} );
	ui.Button( "Reset draft", [ input_handle ] {
		if( auto* input = input_handle.Get() ) input->SetValue( "initial", tatzel::ui::ValueUpdatePolicy::Replace );
	} );
	ui.Button( "Update then fail", [ status ] {
		if( auto* text = status.Get() ) text->SetText( "Changed before failure" );
		throw tatzel::Exception{ "Expected protocol example failure" };
	} );
	ui.Button( "Recover", [ status ] {
		if( auto* text = status.Get() ) text->SetText( "Recovered" );
	} );
	ui.Column( []( auto rows ) {
		for( auto index = 0; index < 300; ++index ) rows.Text( "Batch row " + std::to_string( index ) );
	} );
}

}
