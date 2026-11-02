#include <cassert>
#include <functional>
#include <json/json.h>
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

/// @brief Verifies semantic table mapping, row identity, callbacks, and lifecycle reconciliation.
/// @return Zero when table and sender contracts hold.
/// @pre Assertions are enabled and real framework modules are linked.
/// @post Temporary controls, snapshots, and callbacks are released.
auto main() -> int
{
	auto frames = std::vector<Json::Value>{};
	auto updater = tatzel::ClientUpdater{ [ &frames ]( const Json::Value& frame ) { frames.push_back( frame ); } };
	auto adapter = tatzel::TatzelElementAdapter{ updater };
	auto tree = tatzel::ClientElementTree{};
	tree.SetRemovalHandler( [ &updater ]( auto parts ) { updater.RemoveElementParts( parts ); } );
	auto core = tatzel::PageBuilderCore{ tree, updater, adapter };
	auto ui = tatzel::PageBuilder{ core, tree.root };
	try
	{
		ui.Table( "Invalid", { { "duplicate", "First" }, { "duplicate", "Second" } } );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}
	assert( tree.root.GetChildren().empty() );
	auto* table = ui.Table( "Records", { { "name", "Name", true }, { "draft", "Draft" } }, tatzel::ui::TableSelection::Multiple );
	assert( table->FindPart( "table" )->GetTag() == "table" );
	assert( table->FindPart( "caption" )->GetTextContent() == "Records" );
	assert( table->FindPart( "state" )->GetAttribute( "colspan" ) == "3" );
	assert( table->GetRows().empty() );
	auto rows = ui.Scope( *table );
	auto input = tatzel::ui::ElementHandle<tatzel::ui::TextInput>{};
	auto* first = rows.TableRow( "key | [] α", [ &input ]( auto cells ) {
		cells.TableCell( "name", "First" );
		cells.TableCell( "draft", [ &input ]( auto content ) { input = content.TextInput( "Draft", "Retained" )->GetHandle(); } );
	}, "First record" );
	auto* second = rows.TableRow( "second", []( auto cells ) { cells.TableCell( "name", "Second" ); } );
	const auto first_id = std::string{ first->GetBasePart().GetPartID() };
	const auto cell_id = std::string{ first->FindCell( "draft" )->GetBasePart().GetPartID() };
	const auto input_id = std::string{ input.Get()->FindPart( "editor" )->GetPartID() };
	assert( first->GetChildren().size() == 3 && second->GetChildren().size() == 3 );
	assert( first->FindCell( "name" )->GetBasePart().GetTag() == "td" );
	assert( table->FindPart( "state-row" )->GetAttribute( "hidden" ).has_value() );
	try { rows.TableRow( "second", []( auto cells ) {} ); assert( false ); }
	catch( const tatzel::Exception& ) {}
	try { rows.TableRow( "partial", []( auto cells ) { cells.TableCell( "missing", "Invalid" ); } ); assert( false ); }
	catch( const tatzel::Exception& ) {}
	assert( !table->FindRow( "partial" ) && table->GetRows().size() == 2 );

	input.Get()->InvokeOnInput( "Newer draft" );
	auto revision = input.Get()->GetEditRevision();
	frames.clear();
	table->SetRowOrder( { "second", "key | [] α" } );
	assert( table->GetRows().front() == second && table->GetRows().back() == first );
	assert( frames.size() == 1 && frames.front()[ "data" ][ "operations" ][ 0 ][ "op" ] == "set_child_order" );
	assert( first->GetBasePart().GetPartID() == first_id && first->FindCell( "draft" )->GetBasePart().GetPartID() == cell_id );
	assert( input.Get()->FindPart( "editor" )->GetPartID() == input_id && input.Get()->GetValue() == "Newer draft" );
	assert( input.Get()->GetEditRevision() == revision );
	try { table->SetRowOrder( { "second", "second" } ); assert( false ); }
	catch( const tatzel::Exception& ) {}
	assert( table->GetRows().front() == second );
	table->SetLoading( true );
	assert( table->FindPart( "table" )->GetAttribute( "aria-busy" ) == "true" && table->GetRows().size() == 2 );
	assert( input.Get()->GetValue() == "Newer draft" );
	table->SetLoading( false );
	auto requested = tatzel::ui::TableSort{};
	auto sorts = 0;
	table->SetOnSort( [ &requested, &sorts ]( tatzel::ui::TableSort sort ) { requested = std::move( sort ); ++sorts; } );
	tatzel::ui::Button* sort_button = nullptr;
	tatzel::ui::TableCell* heading = nullptr;
	for( const auto& child : table->GetChildren() )
		if( auto* cell = dynamic_cast<tatzel::ui::TableCell*>( child.get() ); cell && cell->GetColumnKey() == "name" )
		{
			heading = cell;
			sort_button = dynamic_cast<tatzel::ui::Button*>( cell->GetChildren().front().get() );
		}
	assert( sort_button && heading->GetBasePart().GetTag() == "th" );
	sort_button->InvokeOnClick();
	assert( sorts == 1 && requested.column == "name" && requested.direction == tatzel::ui::TableSortDirection::Ascending );
	assert( heading->GetBasePart().GetAttribute( "aria-sort" ) == "ascending" );
	assert( table->GetRows().front() == second );
	sort_button->InvokeOnClick();
	assert( sorts == 2 && requested.direction == tatzel::ui::TableSortDirection::Descending );
	table->SetSort( {} );
	assert( sorts == 2 && heading->GetBasePart().GetAttribute( "aria-sort" ) == "none" );
	try { table->SetSort( { "draft", tatzel::ui::TableSortDirection::Ascending } ); assert( false ); }
	catch( const tatzel::Exception& ) {}

	auto selections = std::vector<std::vector<std::string>>{};
	table->SetOnSelectionChanged( [ &selections ]( std::vector<std::string> keys ) { selections.push_back( std::move( keys ) ); } );
	first->GetSelectionControl().Get()->InvokeOnChange( "true" );
	second->GetSelectionControl().Get()->InvokeOnChange( "true" );
	assert( selections.size() == 2 && table->GetSelectedRows().size() == 2 );
	assert( first->IsSelected() && second->IsSelected() );
	assert( first->GetBasePart().GetAttribute( "data-ui-selected" ) == "true" );
	const auto selected = table->GetSelectedRows();
	try { table->SetSelectedRows( { "second", "unknown" } ); assert( false ); }
	catch( const tatzel::Exception& ) {}
	assert( table->GetSelectedRows() == selected );
	first->SetLabel( "Updated record" );
	assert( first->GetSelectionControl().Get()->GetLabel() == "Select Updated record" );
	assert( first->GetSelectionControl().Get()->IsChecked() );
	auto first_handle = first->GetHandle();
	first->Remove();
	assert( !first_handle.Get() && !input.Get() && table->GetRows().size() == 1 );
	assert( table->GetSelectedRows() == std::vector<std::string>{ "second" } && selections.size() == 3 );
	table->ClearRows();
	assert( table->GetRows().empty() && selections.size() == 4 && selections.back().empty() );
	assert( heading->GetBasePart().GetAttribute( "scope" ) == "col" );
	assert( !table->FindPart( "state-row" )->GetAttribute( "hidden" ) );
	table->SetLoading( true );
	assert( table->FindPart( "state" )->GetTextContent() == "Loading…" );
	table->SetLoading( false );
	assert( table->FindPart( "state" )->GetTextContent() == "No rows to display." );

	auto* replacement = rows.TableRow( "second", []( auto cells ) { cells.TableCell( "name", "Recreated" ); } );
	assert( replacement->GetKey() == "second" && !replacement->IsSelected() );
	auto* single = ui.Table( "Single", { { "value", "Value" } }, tatzel::ui::TableSelection::Single );
	auto single_rows = ui.Scope( *single );
	auto* a = single_rows.TableRow( "a", []( auto cells ) {} );
	auto* b = single_rows.TableRow( "b", []( auto cells ) {} );
	a->GetSelectionControl().Get()->InvokeOnChange( "true" );
	b->GetSelectionControl().Get()->InvokeOnChange( "true" );
	assert( !a->IsSelected() && !a->GetSelectionControl().Get()->IsChecked() && b->IsSelected() );
	assert( a->GetSelectionControl().Get()->GetValueUpdate().replace );
	try { single->SetSelectedRows( { "a", "b" } ); assert( false ); }
	catch( const tatzel::Exception& ) {}
	auto single_handle = single->GetHandle();
	single->SetOnSelectionChanged( [ single_handle ]( std::vector<std::string> keys ) { single_handle.Get()->Remove(); } );
	b->GetSelectionControl().Get()->InvokeOnChange( "false" );
	assert( !single_handle.Get() );
	return 0;
}
