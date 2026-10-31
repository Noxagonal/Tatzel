/// @file
/// @brief Shared logical field state, validation, and form-snapshot integration.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module UI.Element.FormField;

export import UI.Element.LogicalElement;
export import UI.Element.Interface.Changeable;

namespace tatzel::ui {

/// @brief An owned field value used at the framework's form-submission boundary.
export using FieldValue = std::variant<std::string, bool>;

/// @brief Page-owned base for labeled fields with required and custom validation.
/// @details Strings and validation handlers are owned. Mutations and validation
/// run on the owning page's event loop. Validators must not mutate form values
/// or structure; they return an error message or an empty string for success.
export class FormField : public LogicalElement, public interface::Changeable
{
public:
	/// @brief Creates common logical field state.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible label, copied before returning.
	/// @pre parent is nonnull and outlives this control.
	/// @post The field is enabled, optional, and has no validation error.
	FormField(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label
	) :
		LogicalElement{ id, parent },
		label{ label }
	{}

	/// @brief Reads the accessible label.
	/// @return Borrowed label, invalidated by label mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetLabel() const noexcept -> std::string_view { return label; }

	/// @brief Replaces the accessible label.
	/// @param label New label, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Changed text is owned and the adapter queues its label update.
	auto SetLabel( std::string_view label ) -> void
	{
		if( this->label == label ) return;

		this->label = label;
		NotifyChanged();
	}

	/// @brief Reads the field's own disabled state.
	/// @return Whether the field is disabled independently of its form.
	/// @pre None.
	/// @post State is unchanged.
	auto IsDisabled() const noexcept -> bool { return disabled; }

	/// @brief Changes the field's disabled state.
	/// @param disabled Whether editing and submission should omit this field.
	/// @pre Called on the owning event loop.
	/// @post State is stored; disabling removes any field error and queues presentation updates.
	auto SetDisabled( bool disabled ) -> void
	{
		if( this->disabled == disabled ) return;

		this->disabled = disabled;
		if( disabled ) error.clear();
		NotifyChanged();
	}

	/// @brief Reads the required-field policy.
	/// @return Whether an empty value fails validation.
	/// @pre None.
	/// @post State is unchanged.
	auto IsRequired() const noexcept -> bool { return required; }

	/// @brief Changes the required-field policy.
	/// @param required Whether a value is required; checkboxes must be checked.
	/// @pre Called on the owning event loop.
	/// @post Policy and presentation are updated; validation runs when explicitly requested.
	auto SetRequired( bool required ) -> void
	{
		if( this->required == required ) return;

		this->required = required;
		NotifyChanged();
	}

	/// @brief Reads the current field error.
	/// @return Borrowed message, invalidated by error mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetError() const noexcept -> std::string_view { return error; }

	/// @brief Displays or removes an application-supplied field error.
	/// @param error Error message; empty means no error.
	/// @pre Called on the owning event loop.
	/// @post The message is owned and the adapter queues error text and accessibility updates.
	auto SetError( std::string_view error ) -> void
	{
		if( this->error == error ) return;

		this->error = error;
		NotifyChanged();
	}

	/// @brief Reports whether browser values may change this field.
	/// @return Whether this field is read-only; false for ordinary fields.
	/// @pre None.
	/// @post State is unchanged.
	virtual auto IsReadOnly() const noexcept -> bool { return false; }

	/// @brief Copies the current typed field value for framework use.
	/// @return An owned text or boolean value.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged.
	virtual auto GetFieldValue() const -> FieldValue = 0;

	/// @brief Checks the type and permitted choices of a submitted value.
	/// @param value Candidate owned value from a form snapshot.
	/// @return Whether the value has the correct type and is an allowed choice.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged; required/custom validation is deferred.
	auto CanAcceptFieldValue( const FieldValue& value ) const -> bool
	{
		if( GetChangeValueType() == interface::ChangeValueType::Boolean ) return std::holds_alternative<bool>( value );
		auto* text = std::get_if<std::string>( &value );
		return text && AcceptsChangeValue( *text );
	}

	/// @brief Applies a validated browser snapshot without invoking input/change callbacks.
	/// @param value Value whose type and permitted choices have been checked.
	/// @pre Called on the owning event loop; CanAcceptFieldValue returns true.
	/// @post Editable enabled fields store the value without a browser value echo.
	virtual auto AcceptFieldValue( const FieldValue& value ) -> void = 0;

	/// @brief Runs required and custom validation for the current logical value.
	/// @return Whether validation succeeded; disabled fields succeed and clear their errors.
	/// @pre Called on the owning event loop; validators do not mutate fields or structure.
	/// @post The field error and accessible error presentation reflect the result.
	auto Validate() -> bool
	{
		if( disabled )
		{
			SetError( {} );
			return true;
		}

		auto value = GetFieldValue();
		const auto* text = std::get_if<std::string>( &value );
		const auto empty = text ? text->empty() : !std::get<bool>( value );
		if( required && empty ) SetError( "This field is required." );
		else
		{
			auto validator = validation_handler;
			SetError( validator ? validator() : std::string{} );
		}
		return error.empty();
	}

protected:
	/// @brief Installs a typed control's validation wrapper.
	/// @param handler Callback returning an error or an empty string; may be empty.
	/// @pre Called on the owning event loop; captures outlive their use.
	/// @post Future validation uses this handler; no validation is run immediately.
	auto SetValidationHandler( std::function<std::string()> handler ) -> void { validation_handler = std::move( handler ); }

	/// @brief Removes stale errors before notifying the adapter about a value change.
	/// @pre Called after storing a new value on the owning event loop.
	/// @post The error is empty and the adapter has synchronized the changed field state.
	auto NotifyValueChanged() -> void
	{
		error.clear();
		NotifyChanged();
	}

private:
	std::string label;
	std::string error;
	bool disabled = false;
	bool required = false;
	std::function<std::string()> validation_handler;
};

}
