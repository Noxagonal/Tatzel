module;

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module UI.Element.NumberInput;

import UI.Diagnostics;

namespace tatzel::ui {

NumberInput::NumberInput(
	std::string_view id,
	LogicalElement* parent,
	std::string_view label,
	std::optional<double> value,
	std::function<void( std::optional<double> )> on_change
) :
	TextInput{ id, parent, label, FormatValue( value ), {} }
{
	SetOnChange( std::move( on_change ) );
	TextInput::SetValidator( [ this ]( std::string_view text ) -> std::string {
		auto number = ParseValue( text );
		if( !text.empty() && !number ) return "Enter a valid finite number.";
		if( number )
		{
			if( minimum && *number < *minimum ) return "Value is below the minimum.";
			if( maximum && *number > *maximum ) return "Value is above the maximum.";
			if( step )
			{
				const auto offset = ( static_cast<long double>( *number ) - minimum.value_or( 0 ) ) / *step;
				const auto tolerance = std::min( 0.000001L, 0.000000001L * std::max( 1.0L, std::abs( offset ) ) );
				if( std::isfinite( offset ) && std::abs( offset - std::round( offset ) ) > tolerance )
					return "Value does not match the step.";
			}
		}
		auto callback = validator;
		return callback ? callback( number ) : std::string{};
	} );
}

auto NumberInput::SetRange(
	std::optional<double> minimum,
	std::optional<double> maximum
) -> void
{
	CheckFinite( minimum );
	CheckFinite( maximum );
	if( minimum && maximum && *minimum > *maximum ) throw tatzel::Exception{ "NumberInput bounds are reversed" };
	if( this->minimum == minimum && this->maximum == maximum ) return;

	this->minimum = minimum;
	this->maximum = maximum;
	NotifyValueChanged();
}

auto NumberInput::SetStep( std::optional<double> step ) -> void
{
	CheckFinite( step );
	if( step && *step <= 0 ) throw tatzel::Exception{ "NumberInput step must be positive" };
	if( this->step == step ) return;

	this->step = step;
	NotifyValueChanged();
}

auto NumberInput::SetOnChange( std::function<void( std::optional<double> )> on_change ) -> void
{
	TextInput::SetOnChange( [ callback = std::move( on_change ) ]( std::string_view text ) {
		if( callback ) callback( ParseValue( text ) );
	} );
}

auto NumberInput::SetOnInput( std::function<void( std::optional<double> )> on_input ) -> void
{
	TextInput::SetOnInput( [ callback = std::move( on_input ) ]( std::string_view text ) {
		if( callback ) callback( ParseValue( text ) );
	} );
}

auto NumberInput::SetOnSubmit( std::function<void( std::optional<double> )> on_submit ) -> void
{
	TextInput::SetOnSubmit( [ callback = std::move( on_submit ) ]( std::string_view text ) {
		if( callback ) callback( ParseValue( text ) );
	} );
}

auto NumberInput::ParseValue( std::string_view text ) noexcept -> std::optional<double>
{
	if( text.empty() ) return std::nullopt;
	// Match the native editor's decimal grammar before numeric conversion.
	auto index = size_t{ text.front() == '-' ? 1u : 0u };
	auto digits = size_t{};
	while( index < text.size() && text[ index ] >= '0' && text[ index ] <= '9' )
	{
		++index;
		++digits;
	}
	if( index < text.size() && text[ index ] == '.' )
	{
		++index;
		const auto start = index;
		while( index < text.size() && text[ index ] >= '0' && text[ index ] <= '9' )
		{
			++index;
			++digits;
		}
		if( index == start ) return std::nullopt;
	}
	if( !digits ) return std::nullopt;
	if( index < text.size() && ( text[ index ] == 'e' || text[ index ] == 'E' ) )
	{
		++index;
		if( index < text.size() && ( text[ index ] == '+' || text[ index ] == '-' ) ) ++index;
		const auto start = index;
		while( index < text.size() && text[ index ] >= '0' && text[ index ] <= '9' ) ++index;
		if( index == start ) return std::nullopt;
	}
	if( index != text.size() ) return std::nullopt;

	auto value = double{};
	const auto result = std::from_chars( text.data(), text.data() + text.size(), value );
	if( result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite( value ) ) return std::nullopt;
	return value;
}

auto NumberInput::CheckFinite( std::optional<double> value ) -> void
{
	if( value && !std::isfinite( *value ) ) throw tatzel::Exception{ "NumberInput values must be finite" };
}

auto NumberInput::FormatValue( std::optional<double> value ) -> std::string
{
	CheckFinite( value );
	return value ? std::format( "{}", *value ) : std::string{};
}

}
