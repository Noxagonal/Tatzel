/// @file
/// @brief Semantic tables with stable rows, logical control cells, and application-owned sorting.
module;

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Element.Table;

export import UI.Element.LogicalElement;
export import UI.Element.Checkbox;

namespace tatzel::ui {

/// @brief Immutable semantic column definition; the adapter chooses its representation.
export struct TableColumn
{
	std::string key; ///< Unique nonempty application column key.
	std::string title; ///< Accessible plain-text column heading.
	bool sortable = false; ///< Whether the heading offers a sort request.
};

/// @brief Direction requested for one sortable column.
export enum class TableSortDirection
{
	None, ///< Application-defined natural order.
	Ascending, ///< Ascending application order.
	Descending, ///< Descending application order.
};

/// @brief Owned sort state reported without tying values to a database or query engine.
export struct TableSort
{
	std::string column; ///< Sortable column key, or empty for natural order.
	TableSortDirection direction = TableSortDirection::None; ///< Requested ordering direction.
};

/// @brief Row-selection interaction offered by a logical table.
export enum class TableSelection
{
	None, ///< Ordinary rows without selection controls.
	Single, ///< At most one selected row, using native checkboxes.
	Multiple, ///< Independent selected rows, using native checkboxes.
};

/// @brief Owns a single text cell or a subtree of ordinary logical controls.
/// @details Row/column keys are application identities, separate from generated DOM
/// part identities. Explicit SetText intentionally removes any owned cell controls.
export class TableCell : public LogicalElement
{
public:
	/// @brief Creates a cell before the adapter chooses td or th.
	/// @param id Unique logical identity.
	/// @param parent Owning table row or table for a header.
	/// @param column Stable column key.
	/// @param text Initial plain text.
	/// @param header Whether this is a column header.
	/// @param sortable Whether the header has a logical sort action.
	/// @pre parent is active; header cells belong directly to a Table.
	/// @post Text/key are owned and there are no child controls.
	TableCell(
		std::string_view id,
		LogicalElement* parent,
		std::string_view column,
		std::string_view text = {},
		bool header = false,
		bool sortable = false
	) :
		LogicalElement{ id, parent },
		column{ column },
		text{ text },
		header{ header },
		sortable{ sortable }
	{}

	/// @brief Reads the stable column identity.
	/// @return Borrowed key valid for the cell lifetime.
	/// @pre None.
	/// @post State is unchanged.
	auto GetColumnKey() const noexcept -> std::string_view { return column; }

	/// @brief Reads the plain text stored directly in this cell.
	/// @return Borrowed text, invalidated by SetText or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetText() const noexcept -> std::string_view { return text; }

	/// @brief Intentionally replaces cell content with plain text.
	/// @param text New value, copied before owned child controls are removed.
	/// @pre Called on the owning loop; headers with sort controls are framework-owned.
	/// @post Child handles are invalidated and changed text is offered to the browser.
	auto SetText( std::string_view text ) -> void;

	/// @brief Identifies a column header cell.
	/// @return Whether the adapter maps this cell as a header.
	/// @pre None.
	/// @post State is unchanged.
	auto IsHeader() const noexcept -> bool { return header; }

	/// @brief Reports whether this header has a sort action.
	/// @return Whether aria-sort is meaningful for this cell.
	/// @pre None.
	/// @post State is unchanged.
	auto IsSortable() const noexcept -> bool { return sortable; }

	/// @brief Reads the header's accessible sort state.
	/// @return Current direction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetSortDirection() const noexcept -> TableSortDirection { return sort_direction; }

	/// @brief Synchronizes accessible header direction without changing its button.
	/// @param direction Logical direction for this sortable header.
	/// @pre Called by Table on the owning loop for a sortable header.
	/// @post Direction changes queue an attribute update while retaining child controls.
	auto SetSortDirection( TableSortDirection direction ) -> void;

private:
	std::string column;
	std::string text;
	bool header;
	bool sortable;
	TableSortDirection sort_direction = TableSortDirection::None;
};

/// @brief Owns one stable application row and its logical cells.
export class TableRow : public LogicalElement
{
public:
	/// @brief Creates a row with independent application identity and accessible wording.
	/// @param id Unique logical identity.
	/// @param parent Owning Table.
	/// @param key Stable nonempty application row key.
	/// @param label Accessible selection description, or empty to use key.
	/// @pre The key is unique among active rows of this table.
	/// @post Key/label are owned; the builder creates cells in schema order.
	TableRow(
		std::string_view id,
		LogicalElement* parent,
		std::string_view key,
		std::string_view label = {}
	) :
		LogicalElement{ id, parent },
		key{ key },
		label{ label.empty() ? key : label }
	{}

	/// @brief Reads this row's application identity.
	/// @return Borrowed key valid for this row's lifetime.
	/// @pre None.
	/// @post State is unchanged.
	auto GetKey() const noexcept -> std::string_view { return key; }

	/// @brief Reads accessible row wording.
	/// @return Borrowed label until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetLabel() const noexcept -> std::string_view { return label; }

	/// @brief Updates selection wording without replacing the checkbox or its value.
	/// @param label New plain-text row description, copied before returning.
	/// @pre Called on the owning loop.
	/// @post Future announcements use the label; selection and edit revisions remain intact.
	auto SetLabel( std::string_view label ) -> void
	{
		this->label = label;
		if( auto* checkbox = selection.Get() ) checkbox->SetLabel( "Select " + this->label );
	}

	/// @brief Finds an existing cell by application column identity.
	/// @param column Stable column key.
	/// @return Borrowed active cell, or nullptr.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto FindCell( std::string_view column ) const -> TableCell*;

	/// @brief Binds the framework-created selection checkbox.
	/// @param selection Non-owning child handle, empty for unselectable rows.
	/// @pre Called by the row builder on the owning loop.
	/// @post Selection updates retain native checkbox identity.
	auto BindSelection( ElementHandle<Checkbox> selection ) -> void { this->selection = selection; }

	/// @brief Obtains the framework selection checkbox without retaining ownership.
	/// @return Checked child handle.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetSelectionControl() const -> ElementHandle<Checkbox> { return selection; }

	/// @brief Reads the row's presentation selection state.
	/// @return Whether its application key is selected.
	/// @pre None.
	/// @post State is unchanged.
	auto IsSelected() const noexcept -> bool { return selected; }

	/// @brief Synchronizes row presentation after Table updates selection.
	/// @param selected Whether this row is selected.
	/// @pre Called by Table on the owning loop.
	/// @post Changed selection appearance is queued without replacing cells.
	auto SetSelectionState( bool selected ) -> void;

private:
	std::string key;
	std::string label;
	ElementHandle<Checkbox> selection;
	bool selected = false;
};

/// @brief Owns an accessible table with immutable columns and stable row subtrees.
/// @details Sorting reports requests; application code owns data/query order and
/// calls SetRowOrder. Mutations stay on the UI loop. Reconciliation can update text
/// or controls in place; selection callbacks receive owned stable keys.
export class Table : public LogicalElement
{
public:
	/// @brief Owns an immutable schema and accessible table caption.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param caption Nonempty accessible table name.
	/// @param columns Unique nonempty keys and plain-text headings.
	/// @param selection_mode Native row-selection policy.
	/// @pre parent is nonnull; enum arguments are defined values.
	/// @post The schema is owned, natural sort is active, and no rows are selected.
	/// @throws tatzel::Exception If the caption/schema is empty or keys are invalid.
	Table(
		std::string_view id,
		LogicalElement* parent,
		std::string_view caption,
		std::vector<TableColumn> columns,
		TableSelection selection_mode = TableSelection::None
	);

	static constexpr std::string_view SelectionColumn = "__selection"; ///< Reserved framework selection-cell identity.

	/// @brief Reads the immutable column schema.
	/// @return Borrowed owned definitions valid for the table lifetime.
	/// @pre None.
	/// @post State is unchanged.
	auto GetColumns() const noexcept -> const std::vector<TableColumn>& { return columns; }

	/// @brief Reads the accessible caption.
	/// @return Borrowed text until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetCaption() const noexcept -> std::string_view { return caption; }

	/// @brief Replaces the accessible caption without rebuilding headers or rows.
	/// @param caption New nonempty plain text.
	/// @pre Called on the owning loop.
	/// @post Changed text is owned and the adapter updates the caption.
	/// @throws tatzel::Exception If caption is empty.
	auto SetCaption( std::string_view caption ) -> void;

	/// @brief Reads the row-selection policy.
	/// @return Immutable selection mode.
	/// @pre None.
	/// @post State is unchanged.
	auto GetSelectionMode() const noexcept -> TableSelection { return selection_mode; }

	/// @brief Finds a stable active row without interpreting its key as a DOM ID.
	/// @param key Application row identity.
	/// @return Borrowed row until removal/destruction, or nullptr.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto FindRow( std::string_view key ) const -> TableRow*;

	/// @brief Copies borrowed active rows in current semantic order.
	/// @return Row pointers valid until their removal; no ownership is retained.
	/// @pre Called on the owning loop.
	/// @post State is unchanged; newly attached rows follow existing ordered rows.
	auto GetRows() const -> std::vector<TableRow*>;

	/// @brief Reads current application row ordering.
	/// @return Borrowed keys until structural/order mutation.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetRowOrder() const noexcept -> const std::vector<std::string>& { return row_order; }

	/// @brief Applies a complete validated order without recreating row subtrees.
	/// @param keys Exactly one occurrence of every active application row key.
	/// @pre Called on the owning loop after rows are created.
	/// @post Semantic order changes and one browser child-order update is queued.
	/// @throws tatzel::Exception If keys are missing/foreign/duplicated or the revision is exhausted.
	auto SetRowOrder( std::vector<std::string> keys ) -> void;

	/// @brief Reads the adapter's explicit reorder revision.
	/// @return Monotonic revision changed only by explicit ordering.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetOrderRevision() const noexcept -> std::uint64_t { return order_revision; }

	/// @brief Removes one row and prunes its selected key through normal lifecycle services.
	/// @param key Application row identity.
	/// @return Whether an active row was removed.
	/// @pre Called on the owning loop.
	/// @post Removed row/cell/control handles are invalid and remaining rows retain identity.
	auto RemoveRow( std::string_view key ) -> bool;

	/// @brief Clears data rows while retaining headers and table identity.
	/// @pre Called on the owning loop.
	/// @post Surviving table is empty; successful clearing reports selection change at most once.
	auto ClearRows() -> void;

	/// @brief Updates row order and selection membership after framework ownership changes.
	/// @pre Called by the builder/tree after insertion or removal is complete.
	/// @post Removed keys are pruned; table status and any changed selection are synchronized.
	auto NotifyChildrenChanged() -> void override;

	/// @brief Reads loading presentation without discarding current rows.
	/// @return Whether the table is waiting for application data.
	/// @pre None.
	/// @post State is unchanged.
	auto IsLoading() const noexcept -> bool { return loading; }

	/// @brief Changes loading feedback while retaining existing rows and controls.
	/// @param loading Whether data is being loaded.
	/// @pre Called on the owning loop.
	/// @post Busy/live feedback and an empty table's state row are synchronized.
	auto SetLoading( bool loading ) -> void;

	/// @brief Reads empty-state wording.
	/// @return Borrowed plain text until mutation/destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetEmptyText() const noexcept -> std::string_view { return empty_text; }

	/// @brief Replaces wording shown when there are no rows.
	/// @param text Plain-text empty-state description.
	/// @pre Called on the owning loop.
	/// @post New wording is owned and synchronized without changing rows.
	auto SetEmptyText( std::string_view text ) -> void { empty_text = text; NotifyChanged(); }

	/// @brief Reads current semantic sort state.
	/// @return Owned column key and direction.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetSort() const -> TableSort { return sort; }

	/// @brief Updates accessible sort state without sorting application data or invoking callbacks.
	/// @param sort Natural order or a valid sortable column/direction.
	/// @pre Called on the owning loop.
	/// @post Header indicators are synchronized; row order and application callbacks are unchanged.
	/// @throws tatzel::Exception If the column is not sortable or natural-order metadata is inconsistent.
	auto SetSort( TableSort sort ) -> void;

	/// @brief Replaces the application's sort-request callback.
	/// @param on_sort Callback receiving copied sort state, or empty.
	/// @pre Captures outlive invocation on the owning loop.
	/// @post Future header actions use this callback.
	auto SetOnSort( std::function<void( TableSort )> on_sort ) -> void { this->on_sort = std::move( on_sort ); }

	/// @brief Accepts a logical header action and reports its next requested direction.
	/// @param column Valid sortable column key captured by the builder's logical button.
	/// @pre Called on the owning loop.
	/// @post Header state changes before a copied application callback runs; data sorting remains application-owned.
	auto RequestSort( std::string_view column ) -> void;

	/// @brief Copies selected stable application keys.
	/// @return Owned selection values independent of DOM identities.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetSelectedRows() const -> std::vector<std::string> { return selected; }

	/// @brief Authoritatively replaces row selection without invoking the application callback.
	/// @param keys Unique active row keys allowed by the selection mode.
	/// @pre Called on the owning loop.
	/// @post Native checkboxes receive replacement generations; surviving cell controls remain intact.
	/// @throws tatzel::Exception If selection is disabled or keys violate membership/cardinality.
	auto SetSelectedRows( std::vector<std::string> keys ) -> void;

	/// @brief Replaces the application's selection-change callback.
	/// @param on_selection Callback receiving copied keys, or empty.
	/// @pre Captures outlive invocation on the owning loop.
	/// @post Future browser changes and selected-row removals use the callback.
	auto SetOnSelectionChanged( std::function<void( std::vector<std::string> )> on_selection ) -> void { this->on_selection = std::move( on_selection ); }

	/// @brief Accepts the native selection checkbox belonging to an active stable row.
	/// @param key Application row key captured by the builder.
	/// @param checked Current accepted browser checkbox state.
	/// @pre Called by a registered logical checkbox on the owning loop.
	/// @post Selection mode is enforced and a changed coherent key snapshot is reported once.
	auto AcceptRowSelection(
		std::string_view key,
		bool checked
	) -> void;

private:
	/// @brief Delivers owned selection state under the existing callback/removal boundary.
	/// @pre Called on the owning loop inside an active dispatch scope.
	/// @post A copied callback may remove this table without invalidating its argument.
	auto EmitSelection() -> void;

	std::string caption;
	std::vector<TableColumn> columns;
	TableSelection selection_mode;
	std::vector<std::string> row_order;
	std::vector<std::string> selected;
	std::uint64_t order_revision = 0;
	TableSort sort;
	bool loading = false;
	bool suppress_selection = false;
	std::string empty_text = "No rows to display.";
	std::function<void( TableSort )> on_sort;
	std::function<void( std::vector<std::string> )> on_selection;
};

}
