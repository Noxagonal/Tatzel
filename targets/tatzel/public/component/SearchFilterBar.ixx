/// @file
/// @brief Reusable search and selection component with owned change snapshots.
module;

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Component.SearchFilterBar;

export import UI.PageBuilder;
import UI.Diagnostics;

namespace tatzel::ui {

/// @brief One named selection in a reusable filter bar.
export struct FilterDefinition
{
	std::string key; ///< Unique application key, independent of DOM identity.
	std::string label; ///< Accessible native selector label.
	std::vector<SelectOption> choices; ///< Owned enabled/disabled choices.
	std::string value; ///< Initial selection.
	std::string reset_value; ///< Authoritative selection used by Reset.
};

/// @brief Owned configuration for one independent filter-bar instance.
export struct SearchFilterOptions
{
	std::string search_label = "Search"; ///< Accessible search field label.
	std::string placeholder = "Search…"; ///< Search prompt.
	std::string query; ///< Initial search draft.
	std::vector<FilterDefinition> filters; ///< Ordered named selections.
	std::string reset_label = "Reset filters"; ///< Empty omits the built-in reset action.
};

/// @brief Owned coherent values delivered by a filter-bar change callback.
export struct FilterValues
{
	std::string query; ///< Current search text.
	std::vector<std::pair<std::string, std::string>> selections; ///< Application key/value pairs in display order.
};

/// @brief Composes a search field, named selectors, and an optional reset action.
/// @details The logical tree owns children. Callbacks receive copied snapshots;
/// setters and callbacks run on the UI loop. Programmatic choice refreshes are
/// silent, while browser edits and Reset notify once with the complete state.
export class SearchFilterBar : public Component
{
public:
	/// @brief Stores filter definitions before attaching and building children.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param options Owned labels, choices, initial values, and reset policy.
	/// @pre parent is nonnull and all selection/reset values name enabled choices or empty.
	/// @post Configuration is owned; invalid or duplicate keys throw before Build.
	/// @throws tatzel::Exception If a key is empty or duplicated.
	SearchFilterBar(
		std::string_view id,
		LogicalElement* parent,
		SearchFilterOptions options = {}
	) :
		Component{ id, parent, "search-filter-bar" },
		options{ std::move( options ) }
	{
		auto keys = std::vector<std::string>{};
		for( const auto& filter : this->options.filters )
		{
			if( filter.key.empty() || std::ranges::find( keys, filter.key ) != keys.end() )
				throw tatzel::Exception{ "Filter keys must be nonempty and unique" };
			keys.push_back( filter.key );
			if( !Available( filter.choices, filter.reset_value ) )
				throw tatzel::Exception{ "Filter reset value is unavailable" };
		}
		SetAppearance( "tatzel-filter-bar" );
	}

	/// @brief Builds children after this component is attached to its page.
	/// @tparam BuilderT Factory-supplied scoped builder type.
	/// @param ui Scoped builder supplied by PageBuilder::Component.
	/// @pre Called once by the component factory on the owning loop.
	/// @post Editors and optional reset action belong to this logical subtree.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		auto self = GetHandle();
		auto* search = ui.TextInput( options.search_label, options.query );
		search->SetPlaceholder( options.placeholder );
		search->SetOnInput( [ self ]( std::string_view value ) { if( auto* owner = self.Get() ) owner->EmitChanged(); } );
		query = search->GetHandle();
		for( const auto& filter : options.filters )
		{
			auto* select = ui.Select( filter.label, filter.choices, filter.value,
				[ self ]( std::string_view value ) { if( auto* owner = self.Get() ) owner->EmitChanged(); } );
			fields.push_back( { filter.key, filter.reset_value, select->GetHandle() } );
		}
		if( !options.reset_label.empty() )
			ui.Button( options.reset_label, [ self ] { if( auto* owner = self.Get() ) owner->Reset(); } )->SetVariant( ButtonVariant::Secondary );
	}

	/// @brief Copies the current coherent search and selection state.
	/// @return Owned values safe to retain after the callback or send to a worker.
	/// @pre Called on the owning loop while all built fields are active.
	/// @post No values or callbacks change.
	auto GetValues() const -> FilterValues
	{
		auto result = FilterValues{};
		if( auto* search = query.Get() ) result.query = search->GetValue();
		for( const auto& field : fields )
			if( auto* select = field.editor.Get() ) result.selections.emplace_back( field.key, select->GetValue() );
		return result;
	}

	/// @brief Replaces the owned coherent-value callback.
	/// @param on_change Callback receiving copied values; empty clears it.
	/// @pre Called on the owning loop; captures outlive invocation.
	/// @post Future browser edits and Reset use the new callback.
	auto SetOnChange( std::function<void( FilterValues )> on_change ) -> void { this->on_change = std::move( on_change ); }

	/// @brief Intentionally clears search and restores configured reset selections.
	/// @pre Called on the owning loop after Build; reset selections remain available.
	/// @post Every field begins a fresh edit generation before one coherent callback runs.
	auto Reset() -> void
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() ) return;
		for( const auto& field : fields )
			if( auto* select = field.editor.Get(); select && !select->AcceptsChangeValue( field.reset_value ) )
				throw tatzel::Exception{ "Filter reset value is unavailable" };
		if( auto* search = query.Get() ) search->SetValue( "", ValueUpdatePolicy::Replace );
		for( const auto& field : fields )
			if( auto* select = field.editor.Get() ) select->SetValue( field.reset_value, ValueUpdatePolicy::Replace );
		EmitChanged();
	}

	/// @brief Refreshes one selector without rebuilding the filter bar or search draft.
	/// @param key Application key of the selector.
	/// @param choices New owned choices, including its configured reset value.
	/// @param fallback Enabled fallback when the current selection disappears.
	/// @return Current logical selection after the refresh.
	/// @pre Called on the owning loop after Build.
	/// @post Available selections survive; unavailable selections use fallback; no application callback runs.
	/// @throws tatzel::Exception If the key, fallback, reset value, or choices are invalid.
	auto SetChoices(
		std::string_view key,
		std::vector<SelectOption> choices,
		std::string_view fallback
	) -> std::string
	{
		auto found = std::ranges::find( fields, key, &Field::key );
		if( found == fields.end() || !found->editor.Get() ) throw tatzel::Exception{ "Unknown or inactive filter key" };
		if( !Available( choices, fallback ) || !Available( choices, found->reset_value ) )
			throw tatzel::Exception{ "Filter fallback or reset value is unavailable" };
		auto* select = found->editor.Get();
		select->SetOptions( std::move( choices ), fallback );
		return std::string{ select->GetValue() };
	}

private:
	/// @brief Borrowed child identity and reset policy for one selection.
	struct Field
	{
		std::string key; ///< Application selection key.
		std::string reset_value; ///< Authoritative reset choice.
		ElementHandle<Select> editor; ///< Weak logical child handle.
	};

	/// @brief Checks an enabled selection without changing native choices.
	/// @param choices Candidate choices.
	/// @param value Selected or reset ID.
	/// @return Whether value is empty or names an enabled choice.
	/// @pre None.
	/// @post No state changes.
	static auto Available(
		const std::vector<SelectOption>& choices,
		std::string_view value
	) -> bool
	{
		return value.empty() || std::ranges::any_of( choices, [ value ]( const auto& option ) { return option.id == value && !option.disabled; } );
	}

	/// @brief Delivers one copied snapshot under callback and removal boundaries.
	/// @pre Called on the owning loop after a complete state change.
	/// @post An installed callback runs once and may safely remove this component.
	auto EmitChanged() -> void
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() ) return;
		auto callback = on_change;
		if( callback )
		{
			auto values = GetValues();
			auto boundary = tatzel::CallbackScope{ "Filter change" };
			callback( std::move( values ) );
		}
	}

	SearchFilterOptions options;
	ElementHandle<TextInput> query;
	std::vector<Field> fields;
	std::function<void( FilterValues )> on_change;
};

}
