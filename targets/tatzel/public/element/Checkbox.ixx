/// @file
/// @brief Logical labeled checkbox and boolean change events.
module;

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module UI.Element.Checkbox;

export import UI.Element.FormField;

namespace tatzel::ui {

/// @brief Page-owned boolean control with an owned accessible label.
/// @details Browser changes are accepted before callbacks without value echoes.
/// All mutations run on the owning page's event loop.
export class Checkbox : public FormField
{
public:
	/// @brief Creates a labeled checkbox.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible label, copied before returning.
	/// @param checked Initial boolean value.
	/// @param on_change Callback receiving accepted boolean changes; may be empty.
	/// @pre parent is nonnull; callback captures outlive their invocation.
	/// @post Logical state is stored; the builder assigns its representation.
	Checkbox(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label,
		bool checked,
		std::function<void( bool )> on_change
	) :
		FormField{ id, parent, label },
		checked{ checked },
		on_change{ std::move( on_change ) }
	{}

	/// @brief Reads the checked state.
	/// @return The checked state.
	/// @pre None.
	/// @post State is unchanged.
	auto IsChecked() const noexcept -> bool { return checked; }

	/// @brief Replaces the checked state and notifies the adapter.
	/// @param checked New checked state.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetChecked( bool checked ) -> void
	{
		if( this->checked == checked ) return;

		this->checked = checked;
		++value_revision;
		NotifyValueChanged();
	}

	/// @brief Reads the explicit server-value revision for the adapter.
	/// @return Revision incremented only by a changed SetChecked call.
	/// @pre None.
	/// @post State is unchanged.
	/// @note Framework integration API.
	auto GetValueRevision() const noexcept -> size_t { return value_revision; }

	/// @brief Replaces the boolean-change callback.
	/// @param on_change New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captured objects outlive callback invocations.
	/// @post Future accepted events use the new callback.
	auto SetOnChange( std::function<void( bool )> on_change ) -> void { this->on_change = std::move( on_change ); }

	/// @brief Reports that this checkbox accepts boolean change values.
	/// @return The boolean wire-value type.
	/// @pre None.
	/// @post State is unchanged.
	auto GetChangeValueType() const noexcept -> interface::ChangeValueType override { return interface::ChangeValueType::Boolean; }

	/// @brief Accepts a normalized browser boolean and invokes the change callback.
	/// @param value Exactly true or false, normalized by transport routing.
	/// @pre Called on the owning event loop for this checkbox's editor part.
	/// @post An enabled checkbox stores the accepted value before its callback runs.
	/// @throws std::invalid_argument If value is not a normalized boolean string.
	auto InvokeOnChange( std::string_view value ) -> void override
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() ) return;

		if( value != "true" && value != "false" )
		{
			throw std::invalid_argument{ "Checkbox changes require a boolean" };
		}
		if( IsDisabled() ) return;

		checked = value == "true";
		NotifyValueChanged();
		if( IsRemoved() ) return;

		auto callback = on_change;
		if( callback ) callback( checked );
	}

	/// @brief Installs custom validation for this field's current value.
	/// @param validator Returns an error message or an empty string; empty clears validation.
	/// @pre Called on the owning event loop; the validator does not mutate field values or structure.
	/// @post Future Validate calls use this validator after required-field checks.
	auto SetValidator( std::function<std::string( bool )> validator ) -> void
	{
		SetValidationHandler( [ this, validator = std::move( validator ) ] {
			auto snapshot = checked;
			return validator ? validator( snapshot ) : std::string{};
		} );
	}

	/// @brief Copies this field's value for coordinated form submission.
	/// @return The owned current value.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged.
	auto GetFieldValue() const -> FieldValue override { return checked; }

	/// @brief Accepts a validated form value without invoking application edit callbacks.
	/// @param value Correctly typed value from the form snapshot.
	/// @pre Called on the owning event loop; CanAcceptFieldValue returns true.
	/// @post Editable enabled state is stored, stale errors are removed, and no value echo is sent.
	auto AcceptFieldValue( const FieldValue& value ) -> void override
	{
		if( IsDisabled() || IsReadOnly() ) return;

		this->checked = std::get<bool>( value );
		NotifyValueChanged();
	}

private:
	bool checked;
	size_t value_revision = 0;
	std::function<void( bool )> on_change;
};

}
