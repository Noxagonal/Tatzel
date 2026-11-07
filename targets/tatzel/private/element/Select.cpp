module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

module UI.Element.Select;

import UI.Diagnostics;

namespace tatzel::ui {

Select::Select(
	std::string_view id,
	LogicalElement* parent,
	std::string_view label,
	std::vector<SelectOption> options,
	std::string_view value,
	std::function<void( std::string_view )> on_change
) :
	FormField{ id, parent, label },
	options{ std::move( options ) },
	value{ value },
	on_change{ std::move( on_change ) }
{
	ValidateOptions( this->options );
	if( !AcceptsChangeValue( value ) ) throw tatzel::Exception{ "Select value is not an enabled option" };
}

auto Select::SetOptions(
	std::vector<SelectOption> options,
	std::string_view fallback
) -> void
{
	ValidateOptions( options );
	if( !fallback.empty() && !std::ranges::any_of( options, [ fallback ]( const auto& option ) { return option.id == fallback && !option.disabled; } ) )
		throw tatzel::Exception{ "Select fallback is not an enabled option" };
	auto owned_fallback = std::string{ fallback };
	RecordChoiceChange();
	this->options = std::move( options );
	if( !AcceptsChangeValue( value ) ) value = std::move( owned_fallback );
	++options_revision;
	NotifyValueChanged();
}

auto Select::SetValue(
	std::string_view value,
	ValueUpdatePolicy policy
) -> void
{
	if( !AcceptsChangeValue( value ) ) throw tatzel::Exception{ "Select value is not an enabled option" };
	if( this->value == value && policy != ValueUpdatePolicy::Replace ) return;

	RecordServerValueChange( policy );

	this->value = value;
	++value_revision;
	NotifyValueChanged();
}

auto Select::TrySetValue(
	std::string_view value,
	const EditRevision& revision
) -> bool
{
	if( !IsEditRevisionCurrent( revision ) ) return false;
	SetValue( value );
	return true;
}

auto Select::SetPlaceholder( std::string_view placeholder ) -> void
{
	if( this->placeholder == placeholder ) return;

	RecordChoiceChange();
	this->placeholder = placeholder;
	++options_revision;
	NotifyChanged();
}

auto Select::AcceptsChangeValue( std::string_view value ) const -> bool
{
	return value.empty() || std::ranges::any_of( options, [ value ]( const auto& option ) {
		return option.id == value && !option.disabled;
	} );
}

auto Select::AcceptFieldValue( const FieldValue& value ) -> void
{
	if( IsDisabled() ) return;

	this->value = std::get<std::string>( value );
	NotifyValueChanged();
}

auto Select::InvokeOnChange( std::string_view value ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;

	if( !AcceptsChangeValue( value ) ) throw tatzel::Exception{ "Select value is not an enabled option" };
	if( IsDisabled() ) return;

	this->value = value;
	NotifyValueChanged();
	if( IsRemoved() ) return;
	auto callback = on_change;
	auto snapshot = this->value;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Select change" };
		callback( snapshot );
	}
}

auto Select::SetValidator( std::function<std::string( std::string_view )> validator ) -> void
{
	SetValidationHandler( [ this, validator = std::move( validator ) ] {
		auto snapshot = value;
		return validator ? validator( snapshot ) : std::string{};
	} );
}

auto Select::ValidateOptions( std::span<const SelectOption> options ) -> void
{
	auto ids = std::unordered_set<std::string_view>{};
	for( const auto& option : options )
	{
		if( option.id.empty() || !ids.insert( option.id ).second )
		{
			throw tatzel::Exception{ "Select option IDs must be unique and nonempty" };
		}
	}
}

}
