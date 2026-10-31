/// @file
/// @brief Logical single-choice selection with owned stable option IDs.
module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module UI.Element.Select;

export import UI.Element.FormField;

namespace tatzel::ui {

/// @brief An owned selection choice; IDs remain independent of visible labels.
export struct SelectOption
{
	std::string id; ///< Unique nonempty value used by application code.
	std::string label; ///< Plain text shown to the user; labels may repeat.
	bool disabled = false; ///< Whether this choice is unavailable for selection.
};

/// @brief Page-owned single-choice field with validated IDs and replaceable options.
/// @details Empty value means no selection. Strings, choices, and callbacks are
/// owned. Mutations and callbacks run on the owning page's event loop.
export class Select : public FormField
{
public:
	/// @brief Creates a logical selector with an optional initial value.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible field label, copied before returning.
	/// @param options Choices whose owned strings are transferred to this field.
	/// @param value Initial enabled choice ID, or empty for no selection.
	/// @param on_change Optional callback receiving accepted choice IDs.
	/// @pre parent is nonnull; callback captures outlive their invocation.
	/// @post Valid choices, value, and callback are owned; DOM parts are not yet assigned.
	/// @throws std::invalid_argument If IDs are empty/duplicated or value is unavailable.
	Select(
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
		if( !AcceptsChangeValue( value ) ) throw std::invalid_argument{ "Select value is not an enabled option" };
	}

	/// @brief Reads the available choices.
	/// @return Borrowed choices, invalidated by SetOptions or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetOptions() const noexcept -> std::span<const SelectOption> { return options; }

	/// @brief Replaces choices while preserving an available selected ID.
	/// @param options New choices with unique nonempty IDs.
	/// @pre Called on the owning event loop.
	/// @post Choices are owned; a removed/disabled selection becomes empty; the adapter queues one options update.
	/// @throws std::invalid_argument If an ID is empty or duplicated; existing state is unchanged.
	auto SetOptions( std::vector<SelectOption> options ) -> void
	{
		ValidateOptions( options );
		this->options = std::move( options );
		if( !AcceptsChangeValue( value ) ) value.clear();
		++options_revision;
		NotifyValueChanged();
	}

	/// @brief Reads the selected stable ID.
	/// @return Borrowed ID, or empty for no selection; value changes invalidate the view.
	/// @pre None.
	/// @post State is unchanged.
	auto GetValue() const noexcept -> std::string_view { return value; }

	/// @brief Explicitly selects an available ID in the browser.
	/// @param value Enabled choice ID, or empty for no selection.
	/// @pre Called on the owning event loop.
	/// @post Changed selection is stored and the adapter queues a value replacement.
	/// @throws std::invalid_argument If value is not an enabled choice; existing state is unchanged.
	auto SetValue( std::string_view value ) -> void
	{
		if( !AcceptsChangeValue( value ) ) throw std::invalid_argument{ "Select value is not an enabled option" };
		if( this->value == value ) return;

		this->value = value;
		++value_revision;
		NotifyValueChanged();
	}

	/// @brief Reads the visible no-selection prompt.
	/// @return Borrowed prompt, invalidated by placeholder mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetPlaceholder() const noexcept -> std::string_view { return placeholder; }

	/// @brief Replaces the no-selection prompt without changing the selected ID.
	/// @param placeholder New plain text prompt, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post The prompt is owned and a bound adapter queues an options update.
	auto SetPlaceholder( std::string_view placeholder ) -> void
	{
		if( this->placeholder == placeholder ) return;

		this->placeholder = placeholder;
		++options_revision;
		NotifyChanged();
	}

	/// @brief Reports the accepted wire-value type.
	/// @return Text because selection values are stable string IDs.
	/// @pre None.
	/// @post State is unchanged.
	auto GetChangeValueType() const noexcept -> interface::ChangeValueType override { return interface::ChangeValueType::Text; }

	/// @brief Checks whether an ID is available for selection.
	/// @param value Choice ID, or empty for no selection.
	/// @return Whether the ID is empty or names an enabled choice.
	/// @pre None.
	/// @post State is unchanged.
	auto AcceptsChangeValue( std::string_view value ) const -> bool override
	{
		return value.empty() || std::ranges::any_of( options, [ value ]( const auto& option ) {
			return option.id == value && !option.disabled;
		} );
	}

	/// @brief Copies the selected ID for form processing.
	/// @return An owned text value.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged.
	auto GetFieldValue() const -> FieldValue override { return value; }

	/// @brief Applies a validated form selection without application change callbacks.
	/// @param value Correctly typed and permitted selected ID.
	/// @pre Called on the owning event loop; CanAcceptFieldValue returns true.
	/// @post An enabled field stores selection without a browser value echo.
	auto AcceptFieldValue( const FieldValue& value ) -> void override
	{
		if( IsDisabled() ) return;

		this->value = std::get<std::string>( value );
		NotifyValueChanged();
	}

	/// @brief Accepts a browser selection and invokes the change callback.
	/// @param value Enabled option ID, or empty for no selection.
	/// @pre Called on the owning event loop for this selector's editor source.
	/// @post An enabled control stores the ID before calling application code, without value echoes.
	/// @throws std::invalid_argument If the ID is unavailable.
	auto InvokeOnChange( std::string_view value ) -> void override
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() ) return;

		if( !AcceptsChangeValue( value ) ) throw std::invalid_argument{ "Select value is not an enabled option" };
		if( IsDisabled() ) return;

		this->value = value;
		NotifyValueChanged();
		if( IsRemoved() ) return;
		auto callback = on_change;
		auto snapshot = this->value;
		if( callback ) callback( snapshot );
	}

	/// @brief Replaces the accepted-selection callback.
	/// @param on_change New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive their invocation.
	/// @post Future accepted changes invoke the new callback.
	auto SetOnChange( std::function<void( std::string_view )> on_change ) -> void { this->on_change = std::move( on_change ); }

	/// @brief Installs custom validation for the selected ID.
	/// @param validator Returns an error message or empty for success; may be empty.
	/// @pre Called on the owning event loop; validators do not mutate fields or structure.
	/// @post Future validation uses this callback after required-field checks.
	auto SetValidator( std::function<std::string( std::string_view )> validator ) -> void
	{
		SetValidationHandler( [ this, validator = std::move( validator ) ] {
			auto snapshot = value;
			return validator ? validator( snapshot ) : std::string{};
		} );
	}

	/// @brief Reads the adapter's explicit selection revision.
	/// @return Counter incremented only by changed explicit SetValue calls.
	/// @pre None.
	/// @post State is unchanged.
	auto GetValueRevision() const noexcept -> size_t { return value_revision; }

	/// @brief Reads the adapter's options/prompt revision.
	/// @return Counter incremented by SetOptions or a changed SetPlaceholder call.
	/// @pre None.
	/// @post State is unchanged.
	auto GetOptionsRevision() const noexcept -> size_t { return options_revision; }

private:
	/// @brief Checks option identity before replacing any owned state.
	/// @param options Candidate choices.
	/// @pre None.
	/// @post Candidate choices are unchanged.
	/// @throws std::invalid_argument If an ID is empty or duplicated.
	static auto ValidateOptions( std::span<const SelectOption> options ) -> void
	{
		auto ids = std::unordered_set<std::string_view>{};
		for( const auto& option : options )
		{
			if( option.id.empty() || !ids.insert( option.id ).second )
			{
				throw std::invalid_argument{ "Select option IDs must be unique and nonempty" };
			}
		}
	}

	std::vector<SelectOption> options;
	std::string value;
	std::string placeholder = "Choose an option";
	size_t value_revision = 0;
	size_t options_revision = 0;
	std::function<void( std::string_view )> on_change;
};

}
