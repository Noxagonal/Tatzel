module;

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module UI.Component.SearchFilterBar;

import UI.Diagnostics;

namespace tatzel::ui {

SearchFilterBar::SearchFilterBar(
	std::string_view id,
	LogicalElement* parent,
	SearchFilterOptions options
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

auto SearchFilterBar::GetValues() const -> FilterValues
{
	auto result = FilterValues{};
	if( auto* search = query.Get() ) result.query = search->GetValue();
	for( const auto& field : fields )
		if( auto* select = field.editor.Get() ) result.selections.emplace_back( field.key, select->GetValue() );
	return result;
}

auto SearchFilterBar::Reset() -> void
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

auto SearchFilterBar::SetFilterValue(
	std::string_view key,
	std::string_view value,
	ValueUpdatePolicy policy
) -> void
{
	auto found = std::ranges::find( fields, key, &Field::key );
	if( found == fields.end() || !found->editor.Get() ) throw tatzel::Exception{ "Unknown or inactive filter key" };
	found->editor.Get()->SetValue( value, policy );
}

auto SearchFilterBar::SetChoices(
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

auto SearchFilterBar::Available(
	const std::vector<SelectOption>& choices,
	std::string_view value
) -> bool
{
	return value.empty() || std::ranges::any_of( choices, [ value ]( const auto& option ) { return option.id == value && !option.disabled; } );
}

auto SearchFilterBar::EmitChanged() -> void
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

}
