/// @file
/// @brief Numeric editor with optional finite values and server-side constraints.
module;

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

export module UI.Element.NumberInput;

import UI.Diagnostics;
export import UI.Element.TextInput;

namespace tatzel::ui {

/// @brief Page-owned numeric field sharing the text editor's form and lifetime rules.
/// @details Empty and invalid drafts both read as nullopt; HasInvalidInput separates
/// them. Numeric callbacks receive value snapshots. Constraints validate without
/// clamping or discarding drafts. Mutations run on the owning page's event loop.
export class NumberInput : public TextInput
{
public:
	/// @brief Creates an optional numeric editor accepting any finite step initially.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible label, copied before returning.
	/// @param value Finite initial value, or nullopt for empty.
	/// @param on_change Optional committed-value callback.
	/// @pre parent is nonnull; callback captures outlive invocation.
	/// @post The value and callback are owned; numeric validation is installed.
	/// @throws tatzel::Exception If value is nonfinite.
	NumberInput(
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

	/// @brief Reads the finite numeric value of the current draft.
	/// @return A numeric copy, or nullopt for empty or invalid input.
	/// @pre None.
	/// @post State is unchanged.
	auto GetValue() const noexcept -> std::optional<double> { return ParseValue( TextInput::GetValue() ); }

	/// @brief Reads the editing representation used by form snapshots and the adapter.
	/// @return Borrowed text, invalidated by value mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	/// @note A native bad-input draft is represented by the invalid token NaN.
	auto GetTextValue() const noexcept -> std::string_view { return TextInput::GetValue(); }

	/// @brief Distinguishes invalid drafts from an intentionally empty field.
	/// @return Whether nonempty editing text cannot be parsed as a finite number.
	/// @pre None.
	/// @post State is unchanged.
	auto HasInvalidInput() const noexcept -> bool { return !GetTextValue().empty() && !GetValue(); }

	/// @brief Replaces the numeric value without invoking edit callbacks.
	/// @param value Finite number, or nullopt to clear the editor.
	/// @param policy Guard newer browser edits or deliberately replace them.
	/// @pre Called on the owning event loop.
	/// @post The value is stored and a changed explicit value queues browser replacement.
	/// @throws tatzel::Exception If value is nonfinite; existing state is unchanged.
	auto SetValue(
		std::optional<double> value,
		ValueUpdatePolicy policy = ValueUpdatePolicy::PreserveNewerInput
	) -> void { TextInput::SetValue( FormatValue( value ), policy ); }

	/// @brief Applies a numeric completion only while its captured field revision is current.
	/// @param value Finite numeric value, or nullopt for empty.
	/// @param revision Token captured before asynchronous work began.
	/// @return Whether the logical field accepted the result.
	/// @pre Called on the live owning loop.
	/// @post Stale results change nothing; accepted results queue guarded browser updates.
	auto TrySetValue(
		std::optional<double> value,
		const EditRevision& revision
	) -> bool { return TextInput::TrySetValue( FormatValue( value ), revision ); }

	/// @brief Reads the inclusive lower bound.
	/// @return Bound copy, or nullopt for unbounded.
	/// @pre None.
	/// @post State is unchanged.
	auto GetMinimum() const noexcept -> std::optional<double> { return minimum; }

	/// @brief Reads the inclusive upper bound.
	/// @return Bound copy, or nullopt for unbounded.
	/// @pre None.
	/// @post State is unchanged.
	auto GetMaximum() const noexcept -> std::optional<double> { return maximum; }

	/// @brief Replaces both inclusive bounds atomically without clamping the draft.
	/// @param minimum Finite lower bound, or nullopt for unbounded.
	/// @param maximum Finite upper bound, or nullopt for unbounded.
	/// @pre Called on the owning event loop.
	/// @post Bounds are stored and their browser attributes are queued.
	/// @throws tatzel::Exception If bounds are nonfinite or reversed; state is unchanged.
	auto SetRange(
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

	/// @brief Reads the permitted increment relative to minimum, or zero when unbounded.
	/// @return Positive step copy, or nullopt for any finite increment.
	/// @pre None.
	/// @post State is unchanged.
	auto GetStep() const noexcept -> std::optional<double> { return step; }

	/// @brief Replaces the numeric increment without rounding the draft.
	/// @param step Positive finite increment, or nullopt for any increment.
	/// @pre Called on the owning event loop.
	/// @post The policy is stored and its browser attribute is queued.
	/// @throws tatzel::Exception If step is nonfinite or nonpositive; state is unchanged.
	auto SetStep( std::optional<double> step ) -> void
	{
		CheckFinite( step );
		if( step && *step <= 0 ) throw tatzel::Exception{ "NumberInput step must be positive" };
		if( this->step == step ) return;

		this->step = step;
		NotifyValueChanged();
	}

	/// @brief Installs numeric custom validation after required and numeric checks.
	/// @param validator Returns an error or empty for success; empty clears custom validation.
	/// @pre Called on the owning event loop; validators do not mutate fields or structure.
	/// @post Future validation uses the callback after built-in checks.
	auto SetValidator( std::function<std::string( std::optional<double> )> validator ) -> void { this->validator = std::move( validator ); }

	/// @brief Replaces the committed numeric-value callback.
	/// @param on_change New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive invocation.
	/// @post Accepted changes invoke the callback with a numeric snapshot.
	auto SetOnChange( std::function<void( std::optional<double> )> on_change ) -> void
	{
		TextInput::SetOnChange( [ callback = std::move( on_change ) ]( std::string_view text ) {
			if( callback ) callback( ParseValue( text ) );
		} );
	}

	/// @brief Replaces the numeric editing callback.
	/// @param on_input New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive invocation.
	/// @post Changed edits invoke the callback with a numeric snapshot.
	auto SetOnInput( std::function<void( std::optional<double> )> on_input ) -> void
	{
		TextInput::SetOnInput( [ callback = std::move( on_input ) ]( std::string_view text ) {
			if( callback ) callback( ParseValue( text ) );
		} );
	}

	/// @brief Replaces the numeric Enter-key callback.
	/// @param on_submit New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive invocation.
	/// @post Standalone submissions invoke the callback with a numeric snapshot.
	auto SetOnSubmit( std::function<void( std::optional<double> )> on_submit ) -> void
	{
		TextInput::SetOnSubmit( [ callback = std::move( on_submit ) ]( std::string_view text ) {
			if( callback ) callback( ParseValue( text ) );
		} );
	}

private:
	/// @brief Parses a complete locale-independent finite decimal draft.
	/// @param text Candidate editing text.
	/// @return Finite numeric copy, or nullopt for empty, malformed, or overflowing text.
	/// @pre None.
	/// @post No state changes.
	static auto ParseValue( std::string_view text ) noexcept -> std::optional<double>
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

	/// @brief Rejects nonfinite application-supplied numbers.
	/// @param value Optional number to check.
	/// @pre None.
	/// @post The candidate is unchanged.
	/// @throws tatzel::Exception If a present number is nonfinite.
	static auto CheckFinite( std::optional<double> value ) -> void
	{
		if( value && !std::isfinite( *value ) ) throw tatzel::Exception{ "NumberInput values must be finite" };
	}

	/// @brief Formats a finite number for native HTML editing.
	/// @param value Finite numeric copy, or nullopt for empty.
	/// @return Owned locale-independent decimal text.
	/// @pre None.
	/// @post No state changes.
	/// @throws tatzel::Exception If value is nonfinite.
	static auto FormatValue( std::optional<double> value ) -> std::string
	{
		CheckFinite( value );
		return value ? std::format( "{}", *value ) : std::string{};
	}

	std::optional<double> minimum;
	std::optional<double> maximum;
	std::optional<double> step;
	std::function<std::string( std::optional<double> )> validator;
};

}
