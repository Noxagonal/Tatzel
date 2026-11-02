#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import UI;

namespace example {

/// @brief Demonstrates table reconciliation with retained control cells and owned records.
class TableDemo : public tatzel::ui::Component
{
public:
	/// @brief Creates independent in-memory demonstration records.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @pre parent belongs to the active page.
	/// @post Three records are owned; no table controls have been created.
	TableDemo(
		std::string_view id,
		tatzel::ui::LogicalElement* parent
	) :
		Component{ id, parent, "table-demo" }
	{}

	/// @brief Builds ordinary logical controls around a native semantic table.
	/// @tparam BuilderT Factory-supplied component builder type.
	/// @param ui Scoped component builder.
	/// @pre Called once after attaching this component.
	/// @post Rows, text/control cells, and application sort/selection callbacks are live.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		auto self = GetHandle();
		ui.Heading( "Logical table" );
		ui.Paragraph( "Rows have stable keys. Edit a draft, sort the headers, update another cell, or toggle loading: surviving controls keep their identity." );
		ui.Link( "← All tests", "/" );
		selection = ui.Paragraph( "No rows selected" )->GetHandle();
		selection.Get()->SetLiveRegion( tatzel::ui::LiveRegion::Polite );
		ui.Row( [ self ]( auto actions ) {
			actions.Button( "Add row", [ self ] { if( auto* owner = self.Get() ) owner->Add(); } );
			actions.Button( "Clear rows", [ self ] { if( auto* owner = self.Get() ) owner->Clear(); } )->SetVariant( tatzel::ui::ButtonVariant::Secondary );
			actions.Button( "Toggle loading", [ self ] {
				if( auto* owner = self.Get() ) if( auto* table = owner->table.Get() ) table->SetLoading( !table->IsLoading() );
			} )->SetVariant( tatzel::ui::ButtonVariant::Secondary );
		}, { .wrap = true } );
		auto* control = ui.Table( "Editable records", {
			{ "name", "Name", true }, { "count", "Count", true }, { "draft", "Draft" }, { "actions", "Actions" }
		}, tatzel::ui::TableSelection::Multiple );
		table = control->GetHandle();
		rows = ui.Scope( *control );
		control->SetEmptyText( "No records. Add a row to begin." );
		control->SetOnSort( [ self ]( tatzel::ui::TableSort sort ) { if( auto* owner = self.Get() ) owner->Reconcile(); } );
		control->SetOnSelectionChanged( [ self ]( std::vector<std::string> keys ) {
			if( auto* owner = self.Get() ) if( auto* text = owner->selection.Get() )
				text->SetText( std::to_string( keys.size() ) + ( keys.size() == 1 ? " row selected" : " rows selected" ) );
		} );
		Reconcile();
	}

private:
	/// @brief Owns one record independently of its logical row/control lifetime.
	struct Record
	{
		std::uint64_t id; ///< Stable application identity.
		std::string name; ///< Sortable plain-text value.
		int count; ///< Sortable numeric value.
		std::string draft; ///< Browser-edited owned draft.
	};

	/// @brief Adds a new stable identity and updates only missing/changed rows.
	/// @pre Called on the owning loop after Build.
	/// @post One new row exists and surviving editor controls retain identity.
	auto Add() -> void
	{
		const auto id = ++next_id;
		records.push_back( { id, "Record " + std::to_string( id ), 0, "" } );
		Reconcile();
	}

	/// @brief Removes all records while retaining the table and headers.
	/// @pre Called on the owning loop.
	/// @post Data rows are removed and empty/loading presentation remains usable.
	auto Clear() -> void
	{
		records.clear();
		if( auto* control = table.Get() ) control->ClearRows();
	}

	/// @brief Updates one owned draft from its logical input callback.
	/// @param id Stable record identity.
	/// @param value Latest owned-or-transient editor value.
	/// @pre Called on the owning loop.
	/// @post An existing record owns the value; removed records are ignored.
	auto Edit(
		std::uint64_t id,
		std::string_view value
	) -> void
	{
		if( auto found = std::ranges::find( records, id, &Record::id ); found != records.end() ) found->draft = value;
	}

	/// @brief Changes numeric application data without rebuilding its editor cell.
	/// @param id Stable record identity.
	/// @pre Called on the owning loop.
	/// @post The count cell and any sort order update in place.
	auto Increment( std::uint64_t id ) -> void
	{
		if( auto found = std::ranges::find( records, id, &Record::id ); found != records.end() ) ++found->count;
		Reconcile();
	}

	/// @brief Removes one owned record and its complete logical row.
	/// @param id Stable record identity.
	/// @pre Called on the owning loop.
	/// @post Remaining rows keep identity; selected removed keys are pruned.
	auto Delete( std::uint64_t id ) -> void
	{
		std::erase_if( records, [ id ]( const auto& record ) { return record.id == id; } );
		Reconcile();
	}

	/// @brief Reconciles owned records and applies application-defined sorting.
	/// @pre Called on the owning loop with an active built table.
	/// @post Text changes, row additions/removals, and DOM ordering preserve surviving control cells.
	auto Reconcile() -> void
	{
		auto* control = table.Get();
		if( !control || !rows ) return;
		auto dispatch = control->BeginDispatch();
		auto sort = control->GetSort();
		auto ordered = records;
		std::ranges::stable_sort( ordered, [ &sort ](
			const auto& left,
			const auto& right
		) {
			if( sort.direction == tatzel::ui::TableSortDirection::None ) return left.id < right.id;
			if( sort.column == "count" ) return sort.direction == tatzel::ui::TableSortDirection::Ascending ? left.count < right.count : left.count > right.count;
			return sort.direction == tatzel::ui::TableSortDirection::Ascending ? left.name < right.name : left.name > right.name;
		} );
		for( auto* row : control->GetRows() )
			if( std::ranges::none_of( ordered, [ row ]( const auto& record ) { return "r" + std::to_string( record.id ) == row->GetKey(); } ) ) control->RemoveRow( row->GetKey() );
		auto self = GetHandle();
		auto order = std::vector<std::string>{};
		for( const auto& record : ordered )
		{
			auto key = "r" + std::to_string( record.id );
			order.push_back( key );
			if( auto* row = control->FindRow( key ) )
			{
				row->FindCell( "name" )->SetText( record.name );
				row->FindCell( "count" )->SetText( std::to_string( record.count ) );
				continue;
			}
			rows->TableRow( key, [ self, record ]( auto cells ) {
				cells.TableCell( "name", record.name );
				cells.TableCell( "count", std::to_string( record.count ) );
				cells.TableCell( "draft", [ self, record ]( auto content ) {
					auto* input = content.TextInput( "Draft " + record.name, record.draft );
					input->SetOnInput( [ self, id = record.id ]( std::string_view value ) { if( auto* owner = self.Get() ) owner->Edit( id, value ); } );
				} );
				cells.TableCell( "actions", [ self, id = record.id ]( auto content ) {
					content.Row( [ self, id ]( auto actions ) {
						actions.Button( "Increment", [ self, id ] { if( auto* owner = self.Get() ) owner->Increment( id ); } )->SetSize( tatzel::ui::ControlSize::Small );
						auto* remove = actions.Button( "Delete", [ self, id ] { if( auto* owner = self.Get() ) owner->Delete( id ); } );
						remove->SetVariant( tatzel::ui::ButtonVariant::Danger );
						remove->SetSize( tatzel::ui::ControlSize::Small );
					}, { .wrap = true } );
				} );
			}, record.name );
		}
		control->SetRowOrder( std::move( order ) );
	}

	std::vector<Record> records{ { 1, "Alpha", 3, "First draft" }, { 2, "Bravo", 1, "Second draft" }, { 3, "Charlie", 2, "Third draft" } };
	std::uint64_t next_id = 3;
	tatzel::ui::ElementHandle<tatzel::ui::Table> table;
	tatzel::ui::ElementHandle<tatzel::ui::Paragraph> selection;
	std::optional<tatzel::PageBuilder<tatzel::ui::Table>> rows;
};

}
