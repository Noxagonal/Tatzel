#include "../../targets/development/private/NoteEditor.hpp"

#include <cassert>
#include <functional>
#include "JsonFrames.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import UI;
import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.PageBuilderCore;
import UI.ElementAdapter.TatzelElementAdapter;

/// @brief Component fixture exposing weak handles before a failing or removing Build.
class ConstructionProbe : public tatzel::ui::Component
{
public:
	/// @brief Binds observable construction/lifetime state.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param destroyed Counter incremented when the component is released.
	/// @param child Handle assigned during Build.
	/// @param fail Whether Build throws after creating its child.
	/// @pre Observers outlive this component.
	/// @post The component is empty and owns no observer storage.
	ConstructionProbe(
		std::string_view id,
		tatzel::ui::LogicalElement* parent,
		int& destroyed,
		tatzel::ui::ElementHandle<tatzel::ui::Text>& child,
		bool fail
	) :
		Component{ id, parent },
		destroyed{ destroyed },
		child{ child },
		fail{ fail }
	{}

	/// @brief Records release after construction cleanup.
	/// @pre Observer storage is alive.
	/// @post The destruction count has advanced once.
	~ConstructionProbe() override { ++destroyed; }

	/// @brief Creates a child then fails or removes its complete subtree.
	/// @tparam BuilderT Factory-supplied scoped builder type.
	/// @param ui Scoped builder for this attached component.
	/// @pre Called once by the component factory.
	/// @post A nonthrowing call removes this component; throwing calls exercise factory cleanup.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		child = ui.Text( "Partial child" )->GetHandle();
		if( fail ) throw tatzel::Exception{ "Expected component construction failure" };
		Remove();
	}

private:
	int& destroyed;
	tatzel::ui::ElementHandle<tatzel::ui::Text>& child;
	bool fail;
};

/// @brief Finds a typed logical descendant satisfying a fixture predicate.
/// @tparam T Requested concrete logical type.
/// @tparam Predicate State predicate for that type.
/// @param root Subtree to inspect.
/// @param predicate Match function.
/// @return Borrowed matching control, or nullptr.
/// @pre The subtree is active on its owning loop.
/// @post No state changes.
template<
	typename T,
	typename Predicate
>
auto Find(
	tatzel::ui::LogicalElement& root,
	Predicate predicate
) -> T*
{
	if( auto* value = dynamic_cast<T*>( &root ); value && predicate( *value ) ) return value;
	for( const auto& child : root.GetChildren() )
		if( auto* found = Find<T>( *child, predicate ) ) return found;
	return nullptr;
}

/// @brief Verifies component construction, isolation, outcomes, styling, and asynchronous editor identities.
/// @return Zero when the composed-control contracts hold.
/// @pre Assertions are enabled and real framework modules are linked.
/// @post Temporary trees, callbacks, and frame snapshots are released.
auto main() -> int
{
	auto frames = std::vector<Json::Value>{};
	auto updater = tatzel::ClientUpdater{ [ &frames ]( std::string_view frame ) { frames.push_back( ParseFrame( frame ) ); } };
	auto adapter = tatzel::TatzelElementAdapter{ updater };
	auto tree = tatzel::ClientElementTree{};
	tree.SetRemovalHandler( [ &updater ]( auto parts ) { updater.RemoveElementParts( parts ); } );
	auto core = tatzel::PageBuilderCore{ tree, updater, adapter };
	auto ui = tatzel::PageBuilder{ core, tree.root };
	auto destroyed = 0;
	auto child = tatzel::ui::ElementHandle<tatzel::ui::Text>{};
	try
	{
		ui.Component<ConstructionProbe>( destroyed, child, true );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}
	assert( destroyed == 1 && !child.Get() && tree.root.GetChildren().empty() );
	assert( frames.size() == 1 && frames.front()[ "op" ] == "update_batch" );
	assert( frames.front()[ "data" ][ "operations" ][ frames.front()[ "data" ][ "operations" ].size() - 1 ][ "op" ] == "remove_element_part" );
	assert( ui.Component<ConstructionProbe>( destroyed, child, false ) == nullptr );
	assert( destroyed == 2 && !child.Get() && tree.root.GetChildren().empty() );
	auto* subsequent = ui.Text( "Page remains usable" );
	assert( subsequent );

	auto options = tatzel::ui::SearchFilterOptions{};
	options.filters = { { "status", "Status", { { "all", "All" }, { "open", "Open" } }, "all", "all" } };
	auto* first = ui.Component<tatzel::ui::SearchFilterBar>( options );
	auto* second = ui.Component<tatzel::ui::SearchFilterBar>( options );
	first->SetAppearance( "application-filters" );
	assert( first->GetBasePart().GetAttribute( "data-ui-component" ) == "search-filter-bar" );
	auto changes = 0;
	auto snapshot = tatzel::ui::FilterValues{};
	first->SetOnChange( [ &changes, &snapshot ]( tatzel::ui::FilterValues values ) { ++changes; snapshot = std::move( values ); } );
	auto* search = Find<tatzel::ui::TextInput>( *first, []( auto& field ) { return field.GetLabel() == "Search"; } );
	search->InvokeOnInput( "owned draft" );
	assert( changes == 1 && snapshot.query == "owned draft" && second->GetValues().query.empty() );
	first->Reset();
	assert( changes == 2 && snapshot.query.empty() && snapshot.selections.front().second == "all" );
	assert( search->GetValueUpdate().replace );
	assert( first->SetChoices( "status", { { "all", "Everything" }, { "done", "Done" } }, "all" ) == "all" );
	assert( changes == 2 );
	auto* selection = Find<tatzel::ui::Select>( *first, []( auto& field ) { return true; } );
	selection->InvokeOnChange( "done" );
	assert( first->SetChoices( "status", { { "all", "All" }, { "new", "New" } }, "all" ) == "all" );
	assert( !selection->GetValueUpdate().replace );
	try
	{
		first->SetChoices( "status", { { "done", "Done" } }, "done" );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}
	assert( first->GetValues().selections.front().second == "all" );
	auto second_handle = second->GetHandle();
	auto* second_child = Find<tatzel::ui::TextInput>( *second, []( auto& field ) { return true; } );
	auto second_child_handle = second_child->GetHandle();
	second->Remove();
	assert( !second_handle.Get() && !second_child_handle.Get() && first->GetValues().query.empty() );

	auto* button = ui.Button( "Styled", {} );
	button->SetAppearance( "application-action" );
	button->SetVariant( tatzel::ui::ButtonVariant::Danger );
	button->SetSize( tatzel::ui::ControlSize::Small );
	assert( button->GetAppearance() == "application-action" );
	assert( button->GetBasePart().GetAttribute( "data-ui-variant" ) == "danger" );
	assert( button->GetBasePart().GetAttribute( "data-ui-size" ) == "small" );
	auto clicks = 0;
	button->SetOnClick( [ &clicks ] { ++clicks; } );
	button->SetDisabled( true );
	button->InvokeOnClick();
	assert( clicks == 0 && button->GetBasePart().GetAttribute( "disabled" ).has_value() );
	button->SetDisabled( false );
	button->InvokeOnClick();
	assert( clicks == 1 );
	search->SetSize( tatzel::ui::ControlSize::Large );
	assert( search->FindPart( "editor" )->GetAttribute( "data-ui-size" ) == "large" );
	auto* feedback = ui.Paragraph( "Ready" );
	feedback->SetLiveRegion( tatzel::ui::LiveRegion::Polite );
	assert( feedback->GetBasePart().GetAttribute( "aria-live" ) == "polite" );
	feedback->SetLiveRegion( tatzel::ui::LiveRegion::Off );
	assert( !feedback->GetBasePart().GetAttribute( "aria-live" ) && !feedback->GetBasePart().GetAttribute( "aria-atomic" ) );

	auto* confirmation = ui.Component<tatzel::ui::ConfirmationDialog>();
	auto* modal = Find<tatzel::ui::Modal>( *confirmation, []( auto& field ) { return true; } );
	auto results = std::vector<bool>{};
	confirmation->Request( [ &results ]( bool accepted ) { results.push_back( accepted ); } );
	try
	{
		confirmation->Request( []( bool accepted ) {} );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}
	modal->InvokeOnDismiss();
	confirmation->Cancel();
	assert( results.size() == 1 && !results.front() && !confirmation->IsPending() );
	confirmation->Request( [ &results ]( bool accepted ) { results.push_back( accepted ); } );
	auto* accept = Find<tatzel::ui::Button>( *confirmation, []( auto& field ) { return field.GetText() == "Confirm"; } );
	accept->InvokeOnClick();
	accept->InvokeOnClick();
	assert( results.size() == 2 && results.back() && !modal->IsOpen() );
	confirmation->Request( [ &results ]( bool accepted ) { results.push_back( accepted ); } );
	confirmation->Remove();
	assert( results.size() == 2 );
	auto* reentrant = ui.Component<tatzel::ui::ConfirmationDialog>();
	auto reentrant_handle = reentrant->GetHandle();
	auto reentrant_results = 0;
	reentrant->Request( [ reentrant_handle, &reentrant_results ]( bool accepted ) {
		++reentrant_results;
		reentrant_handle.Get()->Request( [ &reentrant_results ]( bool next ) { ++reentrant_results; } );
	} );
	reentrant->Cancel();
	assert( reentrant_results == 1 && reentrant->IsPending() );
	reentrant->Cancel();
	assert( reentrant_results == 2 && !reentrant->IsPending() );
	auto* removing = ui.Component<tatzel::ui::ConfirmationDialog>();
	auto removing_handle = removing->GetHandle();
	removing->Request( [ removing_handle ]( bool accepted ) { removing_handle.Get()->Remove(); } );
	removing->Cancel();
	assert( !removing_handle.Get() );

	auto* editor = ui.Component<example::NoteEditor>( "Editor", example::NoteDraft{ "old", "Title", "Notes" } );
	auto* form = Find<tatzel::ui::Form>( *editor, []( auto& field ) { return true; } );
	auto* title = Find<tatzel::ui::TextInput>( *editor, []( auto& field ) { return field.GetLabel() == "Title"; } );
	auto token = example::NoteSaveToken{};
	editor->SetOnSave( [ &token ](
		example::NoteDraft draft,
		example::NoteSaveToken request
	) { token = std::move( request ); } );
	assert( form->Submit() && form->IsDisabled() );
	editor->Load( { "new", "Loaded", "Fresh" } );
	assert( !editor->CompleteSave( token ) && title->GetValue() == "Loaded" && !form->IsDisabled() );
	assert( form->Submit() );
	assert( editor->CompleteSave( token, "Save failed" ) && title->GetValue() == "Loaded" && !form->IsDisabled() );
	assert( !editor->CompleteSave( token ) );
	assert( form->Submit() && editor->CompleteSave( token ) );
	editor->SetOnSave( [](
		example::NoteDraft draft,
		example::NoteSaveToken request
	) { throw tatzel::Exception{ "Expected save start failure" }; } );
	try { form->Submit(); assert( false ); }
	catch( const tatzel::Exception& ) {}
	assert( !form->IsDisabled() && title->GetValue() == "Loaded" );
	auto editor_handle = editor->GetHandle();
	editor->SetOnSave( [ editor_handle ](
		example::NoteDraft draft,
		example::NoteSaveToken request
	) {
		editor_handle.Get()->Load( { "newer", "Newer title", "Newer body" } );
		throw tatzel::Exception{ "Expected failure after loading a different record" };
	} );
	try { form->Submit(); assert( false ); }
	catch( const tatzel::Exception& ) {}
	assert( !form->IsDisabled() && title->GetValue() == "Newer title" );
	auto* message = Find<tatzel::ui::Paragraph>( *editor, []( auto& text ) { return text.GetText() == "Loaded newer"; } );
	assert( message );
	return 0;
}
