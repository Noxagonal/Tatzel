/// @file
/// @brief Logical single-line text editor with explicit browser-edit acceptance.
module;

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.Element.TextInput;

export import UI.Element.LogicalElement;
export import UI.Element.Interface.Inputable;
export import UI.Element.Interface.Changeable;
export import UI.Element.Interface.Submittable;

namespace tatzel::ui {

/// @brief Page-owned single-line editor with owned label, placeholder, and value.
/// @details Browser edits update logical state before callbacks without echoing
/// values to the browser. Explicit SetValue calls request browser updates.
/// Mutations and callbacks run on the owning page's event loop. Callback value
/// views remain valid for the callback invocation, including nested setters.
export class TextInput : public LogicalElement, public interface::Inputable,
	public interface::Changeable, public interface::Submittable
{
public:
	/// @brief Creates an editable single-line text control.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible editor label, copied before returning.
	/// @param value Initial value, copied before returning.
	/// @param on_change Callback for committed changes; may be empty.
	/// @pre parent is nonnull; callback captures outlive their invocation.
	/// @post Text is owned; the builder assigns the adapter representation.
	TextInput(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label,
		std::string_view value,
		std::function<void( std::string_view )> on_change
	) :
		LogicalElement{ id, parent },
		label{ label },
		value{ value },
		on_change{ std::move( on_change ) }
	{}

	/// @brief Reads the accessible label.
	/// @return The accessible label, borrowed until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetLabel() const noexcept -> std::string_view { return label; }

	/// @brief Replaces the accessible label and notifies the adapter.
	/// @param label New accessible label, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetLabel( std::string_view label ) -> void
	{
		if( this->label == label ) return;

		this->label = label;
		NotifyChanged();
	}

	/// @brief Reads the editor value.
	/// @return The editor value, borrowed until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetValue() const noexcept -> std::string_view { return value; }

	/// @brief Replaces the editor value and notifies the adapter.
	/// @param value New editor value, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetValue( std::string_view value ) -> void
	{
		if( this->value == value ) return;

		this->value = value;
		++value_revision;
		NotifyChanged();
	}

	/// @brief Reads the placeholder.
	/// @return The placeholder, borrowed until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetPlaceholder() const noexcept -> std::string_view { return placeholder; }

	/// @brief Replaces the placeholder and notifies the adapter.
	/// @param placeholder New placeholder, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetPlaceholder( std::string_view placeholder ) -> void
	{
		if( this->placeholder == placeholder ) return;

		this->placeholder = placeholder;
		NotifyChanged();
	}

	/// @brief Reads the disabled state.
	/// @return The disabled state.
	/// @pre None.
	/// @post State is unchanged.
	auto IsDisabled() const noexcept -> bool { return disabled; }

	/// @brief Replaces the disabled state and notifies the adapter.
	/// @param disabled New disabled state.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetDisabled( bool disabled ) -> void
	{
		if( this->disabled == disabled ) return;

		this->disabled = disabled;
		NotifyChanged();
	}

	/// @brief Reads the read-only state.
	/// @return The read-only state.
	/// @pre None.
	/// @post State is unchanged.
	auto IsReadOnly() const noexcept -> bool { return read_only; }

	/// @brief Replaces the read-only state and notifies the adapter.
	/// @param read_only New read-only state.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetReadOnly( bool read_only ) -> void
	{
		if( this->read_only == read_only ) return;

		this->read_only = read_only;
		NotifyChanged();
	}

	/// @brief Reads the explicit server-value revision for adapter synchronization.
	/// @return Revision incremented only by a changed SetValue call.
	/// @pre None.
	/// @post State is unchanged.
	/// @note Framework integration API; browser edits do not increment this counter.
	auto GetValueRevision() const noexcept -> size_t { return value_revision; }

	/// @brief Replaces the callback for changed, non-composing browser edits.
	/// @param on_input New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captured objects outlive callback invocations.
	/// @post Future accepted events use the new callback.
	auto SetOnInput( std::function<void( std::string_view )> on_input ) -> void { this->on_input = std::move( on_input ); }

	/// @brief Replaces the committed-change callback.
	/// @param on_change New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captured objects outlive callback invocations.
	/// @post Future accepted events use the new callback.
	auto SetOnChange( std::function<void( std::string_view )> on_change ) -> void { this->on_change = std::move( on_change ); }

	/// @brief Replaces the Enter-key submission callback.
	/// @param on_submit New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captured objects outlive callback invocations.
	/// @post Future accepted events use the new callback.
	auto SetOnSubmit( std::function<void( std::string_view )> on_submit ) -> void { this->on_submit = std::move( on_submit ); }

	/// @brief Accepts an uncommitted browser edit and invokes the input callback.
	/// @param value Edited text, copied before invoking application code.
	/// @pre Called on the owning event loop for this editor's input source.
	/// @post Editable changed input is stored without a value echo; its callback has run.
	auto InvokeOnInput( std::string_view value ) -> void override
	{
		if( disabled || read_only || this->value == value ) return;

		this->value = value;
		NotifyChanged();

		auto callback = on_input;
		auto snapshot = this->value;
		if( callback ) callback( snapshot );
	}

	/// @brief Reports that this editor accepts string change values.
	/// @return The text wire-value type.
	/// @pre None.
	/// @post State is unchanged.
	auto GetChangeValueType() const noexcept -> interface::ChangeValueType override { return interface::ChangeValueType::Text; }

	/// @brief Accepts a committed browser value and invokes the change callback.
	/// @param value Committed text, copied before invoking application code.
	/// @pre Called on the owning event loop for this editor's change source.
	/// @post Editable input is stored without a value echo; its callback has run.
	auto InvokeOnChange( std::string_view value ) -> void override
	{
		if( disabled || read_only ) return;

		this->value = value;
		NotifyChanged();

		auto callback = on_change;
		auto snapshot = this->value;
		if( callback ) callback( snapshot );
	}

	/// @brief Submits the current logical value when Enter is pressed.
	/// @pre Called on the owning event loop after the browser's latest edit is accepted.
	/// @post The submit callback has run if enabled and installed.
	auto InvokeOnSubmit() -> void override
	{
		if( disabled ) return;

		auto callback = on_submit;
		auto snapshot = value;
		if( callback ) callback( snapshot );
	}

private:
	std::string label;
	std::string value;
	std::string placeholder;
	bool disabled = false;
	bool read_only = false;
	size_t value_revision = 0;
	std::function<void( std::string_view )> on_change;
	std::function<void( std::string_view )> on_input;
	std::function<void( std::string_view )> on_submit;
};

}
