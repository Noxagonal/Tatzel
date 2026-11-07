module;

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

module UI.Element.Table;

import UI.Diagnostics;

namespace tatzel::ui {

auto TableCell::SetText( std::string_view text ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || ( this->text == text && GetChildren().empty() ) ) return;
	auto owned = std::string{ text };
	ClearChildren();
	if( IsRemoved() ) return;
	this->text = std::move( owned );
	NotifyChanged();
}

auto TableCell::SetSortDirection( TableSortDirection direction ) -> void
{
	if( sort_direction == direction ) return;
	sort_direction = direction;
	NotifyChanged();
}

auto TableRow::FindCell( std::string_view column ) const -> TableCell*
{
	for( const auto& child : GetChildren() )
		if( auto* cell = dynamic_cast<TableCell*>( child.get() ); cell && cell->GetColumnKey() == column ) return cell;
	return nullptr;
}

auto TableRow::SetSelectionState( bool selected ) -> void
{
	if( this->selected == selected ) return;
	this->selected = selected;
	NotifyChanged();
}

Table::Table(
	std::string_view id,
	LogicalElement* parent,
	std::string_view caption,
	std::vector<TableColumn> columns,
	TableSelection selection_mode
) :
	LogicalElement{ id, parent },
	caption{ caption },
	columns{ std::move( columns ) },
	selection_mode{ selection_mode }
{
	if( this->caption.empty() || this->columns.empty() ) throw tatzel::Exception{ "Table requires a caption and columns" };
	auto keys = std::unordered_set<std::string>{};
	for( const auto& column : this->columns )
		if( column.key.empty() || column.key == SelectionColumn || !keys.insert( column.key ).second )
			throw tatzel::Exception{ "Table column keys must be unique, nonempty, and nonreserved" };
}

auto Table::SetCaption( std::string_view caption ) -> void
{
	if( caption.empty() ) throw tatzel::Exception{ "Table caption must be nonempty" };
	if( this->caption == caption ) return;
	this->caption = caption;
	NotifyChanged();
}

auto Table::FindRow( std::string_view key ) const -> TableRow*
{
	for( const auto& child : GetChildren() )
		if( auto* row = dynamic_cast<TableRow*>( child.get() ); row && row->GetKey() == key ) return row;
	return nullptr;
}

auto Table::GetRows() const -> std::vector<TableRow*>
{
	auto result = std::vector<TableRow*>{};
	auto remaining = std::unordered_map<std::string_view, TableRow*>{};
	for( const auto& child : GetChildren() )
		if( auto* row = dynamic_cast<TableRow*>( child.get() ) ) remaining.emplace( row->GetKey(), row );
	for( const auto& key : row_order )
		if( auto found = remaining.find( key ); found != remaining.end() )
		{
			result.push_back( found->second );
			remaining.erase( found );
		}
	for( const auto& child : GetChildren() )
		if( auto* row = dynamic_cast<TableRow*>( child.get() ); row && remaining.contains( row->GetKey() ) ) result.push_back( row );
	return result;
}

auto Table::SetRowOrder( std::vector<std::string> keys ) -> void
{
	auto rows = GetRows();
	if( rows.size() != keys.size() ) throw tatzel::Exception{ "Row order must include every active row exactly once" };
	auto seen = std::unordered_set<std::string>{};
	for( const auto& key : keys )
		if( !FindRow( key ) || !seen.insert( key ).second ) throw tatzel::Exception{ "Row order contains a duplicate or unknown key" };
	if( keys == row_order ) return;
	if( order_revision == tatzel::MaximumWireRevision ) throw tatzel::Exception{ "Table order revision exhausted" };
	row_order = std::move( keys );
	++order_revision;
	NotifyChanged();
}

auto Table::RemoveRow( std::string_view key ) -> bool
{
	auto dispatch = BeginDispatch();
	if( auto* row = FindRow( key ) ) return row->Remove();
	return false;
}

auto Table::ClearRows() -> void
{
	auto dispatch = BeginDispatch();
	const auto previous = selected;
	const auto previous_suppression = suppress_selection;
	suppress_selection = true;
	try
	{
		for( auto* row : GetRows() ) row->Remove();
	}
	catch( ... )
	{
		suppress_selection = previous_suppression;
		throw;
	}
	suppress_selection = previous_suppression;
	if( !IsRemoved() && !suppress_selection && selected != previous ) EmitSelection();
}

auto Table::NotifyChildrenChanged() -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;
	auto rows = GetRows();
	row_order.clear();
	for( auto* row : rows ) row_order.emplace_back( row->GetKey() );
	const auto previous = selected;
	std::erase_if( selected, [ this ]( const auto& key ) { return !FindRow( key ); } );
	NotifyChanged();
	if( !suppress_selection && selected != previous ) EmitSelection();
}

auto Table::SetLoading( bool loading ) -> void
{
	if( this->loading == loading ) return;
	this->loading = loading;
	NotifyChanged();
}

auto Table::SetSort( TableSort sort ) -> void
{
	if( sort.direction == TableSortDirection::None )
	{
		if( !sort.column.empty() ) throw tatzel::Exception{ "Natural table order has no column key" };
	}
	else if( !std::ranges::any_of( columns, [ &sort ]( const auto& column ) { return column.key == sort.column && column.sortable; } ) )
		throw tatzel::Exception{ "Table sort column is unavailable" };
	this->sort = std::move( sort );
	for( const auto& child : GetChildren() )
		if( auto* cell = dynamic_cast<TableCell*>( child.get() ); cell && cell->IsSortable() )
			cell->SetSortDirection( cell->GetColumnKey() == this->sort.column ? this->sort.direction : TableSortDirection::None );
}

auto Table::RequestSort( std::string_view column ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;
	auto direction = sort.column == column && sort.direction == TableSortDirection::Ascending
		? TableSortDirection::Descending : TableSortDirection::Ascending;
	SetSort( { std::string{ column }, direction } );
	auto callback = on_sort;
	if( callback )
	{
		auto boundary = tatzel::CallbackScope{ "Table sort" };
		callback( sort );
	}
}

auto Table::SetSelectedRows( std::vector<std::string> keys ) -> void
{
	if( ( selection_mode == TableSelection::None && !keys.empty() ) || ( selection_mode == TableSelection::Single && keys.size() > 1 ) )
		throw tatzel::Exception{ "Selection violates the table mode" };
	auto seen = std::unordered_set<std::string>{};
	for( const auto& key : keys )
		if( !FindRow( key ) || !seen.insert( key ).second )
			throw tatzel::Exception{ "Selection contains a duplicate or unknown row key" };
	selected = std::move( keys );
	for( auto* row : GetRows() )
	{
		const auto checked = std::ranges::find( selected, row->GetKey() ) != selected.end();
		row->SetSelectionState( checked );
		if( auto* control = row->GetSelectionControl().Get() ) control->SetChecked( checked, ValueUpdatePolicy::Replace );
	}
}

auto Table::AcceptRowSelection(
	std::string_view key,
	bool checked
) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || selection_mode == TableSelection::None || !FindRow( key ) ) return;
	const auto previous = selected;
	if( checked && selection_mode == TableSelection::Single ) selected.clear();
	std::erase( selected, key );
	if( checked ) selected.emplace_back( key );
	for( auto* row : GetRows() )
	{
		const auto active = std::ranges::find( selected, row->GetKey() ) != selected.end();
		row->SetSelectionState( active );
		if( row->GetKey() != key )
			if( auto* control = row->GetSelectionControl().Get(); control && control->IsChecked() != active )
				control->SetChecked( active, ValueUpdatePolicy::Replace );
	}
	if( selected != previous ) EmitSelection();
}

auto Table::EmitSelection() -> void
{
	if( IsRemoved() ) return;
	auto callback = on_selection;
	if( callback )
	{
		auto values = selected;
		auto boundary = tatzel::CallbackScope{ "Table selection" };
		callback( std::move( values ) );
	}
}

}
