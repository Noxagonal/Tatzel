#include "Showcases.hpp"
#include "../NoteEditor.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <utility>

namespace example {

auto BuildComponentsShowcase(
	tatzel::PageBuilder<> ui,
	trantor::ConcurrentTaskQueue& background
) -> void
{
	ui.Heading( "Reusable components" );
	ui.Paragraph( "Each instance owns its state. Try filtering, cancelling a confirmation, saving two editors, or loading another record while a save is pending." );
	ui.Link( "← All tests", "/" );
	auto status = ui.Paragraph( "No filter changes" )->GetHandle();
	status.Get()->SetLiveRegion( tatzel::ui::LiveRegion::Polite );
	for( auto index = 0; index < 2; ++index )
	{
		auto options = tatzel::ui::SearchFilterOptions{};
		options.search_label = index == 0 ? "Search first list" : "Search second list";
		options.filters = { { "status", index == 0 ? "First status" : "Second status",
			{ { "all", "All statuses" }, { "open", "Open" }, { "done", "Done" } }, "all", "all" } };
		auto* filters = ui.Component<tatzel::ui::SearchFilterBar>( std::move( options ) );
		filters->SetOnChange( [ status, index ]( tatzel::ui::FilterValues values ) {
			if( auto* text = status.Get() ) text->SetText( "List " + std::to_string( index + 1 ) + ": " + values.query + " | " + values.selections.front().second );
		} );
	}
	auto* confirmation = ui.Component<tatzel::ui::ConfirmationDialog>( tatzel::ui::ConfirmationOptions{
		.title = "Remove this example?", .message = "This confirmation reports one result and restores focus when it closes." } );
	auto confirm = confirmation->GetHandle();
	ui.Button( "Ask for confirmation", [ confirm, status ] {
		if( auto* dialog = confirm.Get() ) dialog->Request( [ status ]( bool accepted ) {
			if( auto* text = status.Get() ) text->SetText( accepted ? "Confirmed once" : "Cancelled once" );
		} );
	} )->SetVariant( tatzel::ui::ButtonVariant::Danger );
	auto dispatcher = ui.GetDispatcher();
	auto* editor_grid = ui.Container( [ &background, dispatcher ]( auto editors ) {
		for( auto index = 0; index < 2; ++index )
		{
			editors.Container( [ &background, dispatcher, index ]( auto panel ) {
				auto label = index == 0 ? "First editor" : "Second editor";
				auto record = index == 0 ? "record-a" : "record-b";
				auto* editor = panel.template Component<example::NoteEditor>( label, example::NoteDraft{ record, "A note", "An independent draft." } );
				auto handle = editor->GetHandle();
				editor->SetOnSave( [ &background, dispatcher, handle ](
					example::NoteDraft draft,
					example::NoteSaveToken token
				) {
					background.runTaskInQueue( [ dispatcher, handle, draft = std::move( draft ), token = std::move( token ) ] {
						std::this_thread::sleep_for( std::chrono::milliseconds{ 300 } );
						dispatcher.Post( [ handle, draft, token ] {
							if( auto* editor = handle.Get() ) editor->CompleteSave( token, draft.title == "fail" ? "Example save failed. Your draft is preserved." : "" );
						} );
					} );
				} );
				panel.Button( std::string{ "Load another record in " } + label, [ handle ] {
					if( auto* editor = handle.Get() ) editor->Load( { "replacement", "Loaded title", "A fresh record replaces the old draft." } );
				} )->SetVariant( tatzel::ui::ButtonVariant::Secondary );
				panel.Button( std::string{ "Remove " } + label, [ handle ] { if( auto* editor = handle.Get() ) editor->Remove(); } )->SetVariant( tatzel::ui::ButtonVariant::Secondary );
			} );
		}
	} );
	editor_grid->SetAppearance( "tatzel-editor-grid" );
	auto grid = editor_grid->GetHandle();
	ui.Button( "Switch editor theme", [ grid ] {
		if( auto* parent = grid.Get() ) parent->SetAppearance( parent->GetAppearance() == "tatzel-editor-grid"
			? "tatzel-editor-grid alternate-theme" : "tatzel-editor-grid" );
	} )->SetVariant( tatzel::ui::ButtonVariant::Secondary );
}

}
