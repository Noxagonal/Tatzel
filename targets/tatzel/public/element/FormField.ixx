/// @file
/// @brief Shared logical field state, validation, and form-snapshot integration.
module;

#include <functional>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

export module UI.Element.FormField;

import UI.Diagnostics;
export import UI.Protocol;

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
	/// @return Whether validation succeeded; removed fields are ignored and disabled fields clear their errors.
	/// @pre Called on the owning event loop; validators do not mutate fields or structure.
	/// @post The field error and accessible error presentation reflect the result.
	auto Validate() -> bool
	{
		auto dispatch = BeginDispatch();
		if( IsRemoved() ) return true;

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
			auto result = std::string{};
			if( validator )
			{
				auto scope = tatzel::CallbackScope{ "Field validator" };
				result = validator();
			}
			if( IsRemoved() ) return true;
			SetError( result );
		}
		return IsRemoved() || error.empty();
	}

	/// @brief Captures this field's current edit identity for asynchronous work.
	/// @return Owned token identifying this control and its client/server revisions.
	/// @pre Called on the owning loop while the field is alive.
	/// @post The token retains no control or page ownership.
	auto GetEditRevision() const -> EditRevision
	{
		return { std::string{ GetID() }, epoch, client_revision, server_revision };
	}

	/// @brief Checks whether an asynchronous result still targets unchanged logical state.
	/// @param revision Previously captured token for this field.
	/// @return Whether identity and all revision counters still match.
	/// @pre Called on the owning loop while the field is alive.
	/// @post State is unchanged; browser edits still in transit are checked separately on the browser.
	auto IsEditRevisionCurrent( const EditRevision& revision ) const -> bool
	{
		return !IsRemoved() && GetEditRevision() == revision;
	}

	/// @brief Reads metadata for the adapter's latest explicit value or choice update.
	/// @return Copy of the update generation, baseline, and policy.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetValueUpdate() const noexcept -> ValueUpdate { return value_update; }

	/// @brief Validates a browser edit counter and generation before accepting its value.
	/// @param edit Browser metadata validated as JavaScript-safe integers by the transport.
	/// @param value Candidate typed field value.
	/// @return Current, superseded, or invalid metadata.
	/// @pre Called on the owning loop; field identity has already been resolved.
	/// @post State is unchanged.
	auto CheckClientEdit(
		ClientEdit edit,
		const FieldValue& value
	) const -> EditAcceptance
	{
		if( edit.epoch > epoch || edit.revision > tatzel::MaximumWireRevision ) return EditAcceptance::Invalid;
		if( edit.epoch < epoch || edit.revision < client_revision ) return EditAcceptance::Stale;
		if( edit.revision == client_revision && !IsReadOnly() && value != GetFieldValue() ) return EditAcceptance::Invalid;
		return EditAcceptance::Current;
	}

	/// @brief Stores validated browser revision metadata without notifying application callbacks.
	/// @param edit Metadata already accepted by CheckClientEdit.
	/// @pre Called on the owning loop after complete message/snapshot validation.
	/// @post Later guarded updates use the accepted browser counter.
	auto AcceptClientEdit( ClientEdit edit ) noexcept -> void { client_revision = edit.revision; }

	/// @brief Reads the last accepted browser counter for duplicate-event suppression.
	/// @return Counter within the current generation.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetClientRevision() const noexcept -> std::uint64_t { return client_revision; }

protected:
	/// @brief Records an explicit value change before notifying the adapter.
	/// @param policy Whether newer browser input is preserved or deliberately replaced.
	/// @pre Called on the owning loop before a corresponding value notification.
	/// @post The server token changes; replacement begins a fresh browser edit generation.
	/// @throws tatzel::Exception If a JavaScript-safe revision counter is exhausted.
	auto RecordServerValueChange( ValueUpdatePolicy policy ) -> void
	{
		if( server_revision == tatzel::MaximumWireRevision || epoch == tatzel::MaximumWireRevision )
			throw tatzel::Exception{ "Field revision counter exhausted" };

		value_update = { epoch, epoch, client_revision, policy == ValueUpdatePolicy::Replace };
		++server_revision;
		if( policy == ValueUpdatePolicy::Replace )
		{
			value_update.epoch = ++epoch;
			client_revision = 0;
		}
	}

	/// @brief Starts a new choices generation while retaining its previous browser-edit baseline.
	/// @pre Called on the owning loop before replacing native choices or their prompt.
	/// @post Old-generation edits are obsolete; a surviving newer browser choice may be rebased by the runtime.
	auto RecordChoiceChange() -> void
	{
		RecordServerValueChange( ValueUpdatePolicy::Replace );
		value_update.replace = false;
	}

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
	std::uint64_t epoch = 0;
	std::uint64_t client_revision = 0;
	std::uint64_t server_revision = 0;
	ValueUpdate value_update;
	std::string label;
	std::string error;
	bool disabled = false;
	bool required = false;
	std::function<std::string()> validation_handler;
};

}
